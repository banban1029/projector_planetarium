// 起動・メインループ。
// 現段階では星はまだ描かず、モードごとの仮の背景色とウィンドウタイトル（時刻・倍率・季節）で
// キーボード操作（モード切替・季節ジャンプ・時間倍率）を確認する。
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <cstdio>
#include <exception>

#include "config.hpp"
#include "controls.hpp"
#include "modes.hpp"

namespace {
// output: projector なら指定モニタにフルスクリーン、debug（または未接続）ならウィンドウ
GLFWwindow* createOutputWindow(const Config& cfg) {
    int monitorCount = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
    for (int i = 0; i < monitorCount; ++i) {
        const GLFWvidmode* m = glfwGetVideoMode(monitors[i]);
        std::printf("monitor[%d]: %s %dx%d@%dHz\n", i, glfwGetMonitorName(monitors[i]), m->width,
                    m->height, m->refreshRate);
    }

    const auto& pc = cfg.projector;
    GLFWmonitor* target = nullptr;
    if (cfg.output == "projector") {
        if (pc.monitor_index < monitorCount) {
            target = monitors[pc.monitor_index];
            glfwWindowHint(GLFW_REFRESH_RATE, pc.refresh_rate);
        } else {
            std::printf("monitor_index %d not found, using windowed mode\n", pc.monitor_index);
        }
    }
    return glfwCreateWindow(pc.width, pc.height, "Projector Planetarium", target, nullptr);
}
}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);  // ログを即時表示する
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    const char* configPath = argc > 1 ? argv[1] : PLANETARIUM_ROOT "/config/projector.yaml";

    Config cfg;
    try {
        cfg = loadConfig(configPath);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }

    if (!glfwInit()) {
        std::fprintf(stderr, "glfwInit failed\n");
        return 1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = createOutputWindow(cfg);
    if (!window) {
        std::fprintf(stderr, "glfwCreateWindow failed\n");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(cfg.projector.vsync ? 1 : 0);

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::fprintf(stderr, "glewInit failed\n");
        return 1;
    }
    std::printf("GL %s / %s\n", glGetString(GL_VERSION), glGetString(GL_RENDERER));
    std::printf("output: %s %dx%d, observer: %s (%.2f, %.2f)\n", cfg.output.c_str(), cfg.projector.width,
                cfg.projector.height, cfg.observer.name.c_str(), cfg.observer.latitude_deg,
                cfg.observer.longitude_deg);

    AppState app{
        window,
        SimClock(cfg.observer.timezone_hours, cfg.time.speed_steps, cfg.time.speed),
        SeasonCycler(cfg.seasons, cfg.season_tour.interval_sec, cfg.season_tour.enabled),
        findMode(cfg.mode),
    };
    if (!app.clock.setLocalString(cfg.time.start)) {
        std::fprintf(stderr, "invalid time.start: %s (using now)\n", cfg.time.start.c_str());
    }
    installControls(app);
    printHelp();

    double last = glfwGetTime();
    double lastTitle = -1.0;
    while (!glfwWindowShouldClose(window)) {
        const double now = glfwGetTime();
        const double dt = now - last;
        last = now;

        app.clock.update(dt);
        app.seasons.update(dt, app.clock);

        if (now - lastTitle > 0.1) {
            glfwSetWindowTitle(window, statusLine(app).c_str());
            lastTitle = now;
        }

        const auto& c = allModes()[app.mode].placeholder;
        glClearColor(c[0], c[1], c[2], 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
