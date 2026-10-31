// 起動・メインループ。
// 毎フレーム: シミュレーション時刻を進める -> 天文計算（地平座標への変換行列・太陽）-> 描画
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <cstdio>
#include <exception>

#include <memory>
#include <string>
#include <vector>

#include "astro.hpp"
#include "config.hpp"
#include "controls.hpp"
#include "modes.hpp"
#include "sky_renderer.hpp"
#include "star_catalog.hpp"

namespace {
SkyState computeSky(const SimClock& clock, const ObserverConfig& obs) {
    const double jd = clock.julianDate();
    SkyState sky{};
    sky.j2000ToHorizon = astro::j2000ToHorizon(jd, obs.latitude_deg, obs.longitude_deg);
    // 太陽は日付の分点で求まるので、歳差を含まない赤道 -> 地平の変換を使う
    double ra, dec, az;
    astro::sunEquatorial(jd, ra, dec);
    const double lst = astro::greenwichMeanSiderealDeg(jd) + obs.longitude_deg;
    sky.sunHorizon = astro::equatorialToHorizon(lst, obs.latitude_deg) * astro::equatorialVector(ra, dec);
    astro::altAz(sky.sunHorizon, sky.sunAltDeg, az);
    sky.nightMagLimit = obs.limiting_magnitude;
    return sky;
}

ViewSettings viewFromConfig(const Config& cfg) {
    ViewSettings v;
    v.fisheye = cfg.projection.type == "fisheye";
    v.fovDeg = cfg.projection.fov_deg;
    v.centerAltDeg = cfg.projection.center_alt_deg;
    v.centerAzDeg = cfg.projection.center_az_deg;
    v.rotationDeg = cfg.projection.rotation_deg;
    v.flipH = cfg.projector.flip_horizontal;
    v.flipV = cfg.projector.flip_vertical;
    return v;
}

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

// コマンドライン: planetarium [config.yaml] [--time "YYYY-MM-DD HH:MM"] [--mode id] [--location id]
//                            [--grid] [--compass]
//                            [--screenshot out.ppm]（3 フレーム描いて保存し終了。検証・資料用）
struct Options {
    std::string configPath = PLANETARIUM_ROOT "/config/projector.yaml";
    std::string time, mode, location, screenshot;
    bool grid = false;
    bool compass = false;
};

Options parseArgs(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        const bool hasValue = i + 1 < argc;
        if (a == "--time" && hasValue) o.time = argv[++i];
        else if (a == "--mode" && hasValue) o.mode = argv[++i];
        else if (a == "--location" && hasValue) o.location = argv[++i];
        else if (a == "--screenshot" && hasValue) o.screenshot = argv[++i];
        else if (a == "--grid") o.grid = true;
        else if (a == "--compass") o.compass = true;
        else o.configPath = a;
    }
    return o;
}

// 既定フレームバッファを binary PPM で保存する
void saveScreenshot(const std::string& path, int w, int h) {
    std::vector<unsigned char> px(static_cast<size_t>(w) * h * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) {
        std::fprintf(stderr, "cannot write %s\n", path.c_str());
        return;
    }
    std::fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int y = h - 1; y >= 0; --y) std::fwrite(&px[static_cast<size_t>(y) * w * 3], 1, w * 3, f);  // 上下反転
    std::fclose(f);
    std::printf("saved %s\n", path.c_str());
}
}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);  // ログを即時表示する
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    const Options opts = parseArgs(argc, argv);

    Config cfg;
    try {
        cfg = loadConfig(opts.configPath);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
    if (!opts.time.empty()) cfg.time.start = opts.time;

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
    size_t location = cfg.locations.size();
    const std::string locationId = opts.location.empty() ? cfg.location : opts.location;
    for (size_t i = 0; i < cfg.locations.size(); ++i) {
        if (cfg.locations[i].id == locationId) location = i;
    }
    if (location == cfg.locations.size()) {
        if (!locationId.empty()) std::fprintf(stderr, "unknown location '%s', using the first one\n", locationId.c_str());
        location = 0;
    }
    const auto& obs = cfg.locations[location];
    std::printf("output: %s %dx%d, location: %s (%.2f, %.2f)\n", cfg.output.c_str(), cfg.projector.width,
                cfg.projector.height, obs.name.c_str(), obs.latitude_deg, obs.longitude_deg);

    // 時刻の表示は起動時の観測地のタイムゾーンで行う（日本国内の切り替えなら常に JST）
    AppState app{
        window,
        SimClock(obs.timezone_hours, cfg.time.speed_steps, cfg.time.speed),
        SeasonCycler(cfg.seasons, cfg.season_tour.interval_sec, cfg.season_tour.enabled),
        findMode(cfg.mode),
        cfg.locations,
        location,
    };
    app.view = viewFromConfig(cfg);
    app.overlays.grid = cfg.render.show_grid || opts.grid;
    app.overlays.compass = cfg.render.show_compass || opts.compass;
    if (!opts.mode.empty()) app.mode = findMode(opts.mode);
    if (!app.clock.setLocalString(cfg.time.start)) {
        std::fprintf(stderr, "invalid time.start: %s (using now)\n", cfg.time.start.c_str());
    }

    std::unique_ptr<SkyRenderer> renderer;
    try {
        const auto stars = loadStarCatalog(PLANETARIUM_ROOT "/data/stars.csv");
        std::printf("stars: %zu\n", stars.size());
        renderer = std::make_unique<SkyRenderer>(PLANETARIUM_ROOT, stars, cfg.render);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        glfwDestroyWindow(window);
        glfwTerminate();
        return 1;
    }
    installControls(app);
    printHelp();

    double last = glfwGetTime();
    double lastTitle = -1.0;
    int frame = 0;
    while (!glfwWindowShouldClose(window)) {
        const double now = glfwGetTime();
        const double dt = now - last;
        last = now;

        app.clock.update(dt);
        app.seasons.update(dt, app.clock);
        const SkyState sky = computeSky(app.clock, app.locations[app.location]);
        app.sunAltDeg = sky.sunAltDeg;

        if (now - lastTitle > 0.1) {
            glfwSetWindowTitle(window, statusLine(app).c_str());
            lastTitle = now;
        }

        int fbW, fbH;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        renderer->render(fbW, fbH, app.view, sky, allModes()[app.mode].style, static_cast<float>(now),
                         app.overlays);

        if (!opts.screenshot.empty() && ++frame == 3) {
            saveScreenshot(opts.screenshot, fbW, fbH);
            glfwSetWindowShouldClose(window, 1);
        }
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    renderer.reset();  // GL コンテキストが生きているうちに解放する
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
