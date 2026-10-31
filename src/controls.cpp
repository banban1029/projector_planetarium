#include "controls.hpp"

#include <GLFW/glfw3.h>

#include <cstdio>

#include "modes.hpp"

namespace {
const char* onOff(bool on) { return on ? "ON" : "OFF"; }

void selectMode(AppState& app, size_t index) {
    const auto& modes = allModes();
    app.mode = index % modes.size();
    app.transition = true;
    std::printf("[mode] %zu: %s\n", app.mode, modes[app.mode].name);
    showToast(app, std::string("モード: ") + modes[app.mode].name);
}

void selectLocation(AppState& app, size_t index) {
    app.location = index % app.locations.size();
    app.transition = true;
    const auto& l = app.locations[app.location];
    std::printf("[location] %s (%.2f, %.2f)\n", l.name.c_str(), l.latitude_deg, l.longitude_deg);
    showToast(app, "観測地: " + l.name);
}

void seasonJumped(AppState& app) {
    app.transition = true;
    showToast(app, "季節: " + app.seasons.currentName() + "  " + app.clock.formatLocal().substr(0, 16));
}

void speedChanged(AppState& app) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "時間 x%g%s%s", app.clock.speed(), app.clock.reversed() ? "（逆再生）" : "",
                  app.clock.paused() ? "（一時停止）" : "");
    showToast(app, buf);
}

// 魚眼（天頂中心・全天）<-> 透視（南の空・画角 90°）。デバッグ用
void toggleProjection(AppState& app) {
    auto& v = app.view;
    v.fisheye = !v.fisheye;
    v.fovDeg = v.fisheye ? 180.0 : 90.0;
    v.centerAltDeg = v.fisheye ? 90.0 : 30.0;
    v.centerAzDeg = 180.0;
    std::printf("[view] %s\n", v.fisheye ? "fisheye (zenith)" : "perspective (south)");
    showToast(app, v.fisheye ? "魚眼（天頂）" : "透視（南）");
}

// ウィンドウがいま載っているモニタで全画面にする（プロジェクタ側に置いていればプロジェクタで全画面）
GLFWmonitor* monitorUnderWindow(GLFWwindow* w) {
    int wx, wy, ww, wh;
    glfwGetWindowPos(w, &wx, &wy);
    glfwGetWindowSize(w, &ww, &wh);
    const int cx = wx + ww / 2, cy = wy + wh / 2;
    int count = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&count);
    for (int i = 0; i < count; ++i) {
        int mx, my;
        glfwGetMonitorPos(monitors[i], &mx, &my);
        const GLFWvidmode* vm = glfwGetVideoMode(monitors[i]);
        if (cx >= mx && cx < mx + vm->width && cy >= my && cy < my + vm->height) return monitors[i];
    }
    return glfwGetPrimaryMonitor();
}

void toggleFullscreen(AppState& app) {
    GLFWwindow* w = app.window;
    if (!w) return;
    if (glfwGetWindowMonitor(w)) {
        glfwSetWindowMonitor(w, nullptr, app.windowedX, app.windowedY, app.windowedW, app.windowedH, 0);
        return;
    }
    glfwGetWindowPos(w, &app.windowedX, &app.windowedY);
    glfwGetWindowSize(w, &app.windowedW, &app.windowedH);
    GLFWmonitor* monitor = monitorUnderWindow(w);
    const GLFWvidmode* vm = glfwGetVideoMode(monitor);
    glfwSetWindowMonitor(w, monitor, 0, 0, vm->width, vm->height, vm->refreshRate);
}

void toggleScenario(AppState& app) {
    if (!app.scenario || app.scenario->size() == 0) {
        showToast(app, "シナリオがありません（config の scenario）");
        return;
    }
    if (app.scenario->playing()) {
        app.scenario->stop();
        std::printf("[scenario] stop\n");
        showToast(app, "シナリオ停止");
    } else if (const ScenarioStep* step = app.scenario->start()) {
        std::printf("[scenario] start (%zu steps)\n", app.scenario->size());
        applyScenarioStep(app, *step);
    }
}

void onKey(GLFWwindow* window, int key, int /*scancode*/, int action, int mods) {
    handleKey(*static_cast<AppState*>(glfwGetWindowUserPointer(window)), key, action, mods);
}
}  // namespace

void handleKey(AppState& app, int key, int action, int mods) {
    const bool shift = mods & GLFW_MOD_SHIFT;

    // 押しっぱなしで連続させるのは時刻の微調整だけ
    if (action == GLFW_REPEAT || action == GLFW_PRESS) {
        const double step = shift ? 86400.0 : 3600.0;
        if (key == GLFW_KEY_RIGHT) app.clock.shift(step);
        if (key == GLFW_KEY_LEFT) app.clock.shift(-step);
    }
    if (action != GLFW_PRESS) return;

    if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) {
        const size_t index = static_cast<size_t>(key - GLFW_KEY_0);
        if (index < allModes().size()) selectMode(app, index);
        return;
    }
    if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F10) {
        const size_t index = static_cast<size_t>(key - GLFW_KEY_F1);
        if (index < app.seasons.size()) {
            app.seasons.jumpTo(index, app.clock);
            seasonJumped(app);
        }
        return;
    }

    auto& o = app.overlays;
    switch (key) {
        case GLFW_KEY_ESCAPE:
            std::printf("[quit]\n");
            if (app.window) glfwSetWindowShouldClose(app.window, 1);
            break;
        case GLFW_KEY_M: selectMode(app, app.mode + (shift ? allModes().size() - 1 : 1)); break;
        case GLFW_KEY_L: selectLocation(app, app.location + (shift ? app.locations.size() - 1 : 1)); break;
        case GLFW_KEY_S:
            shift ? app.seasons.prev(app.clock) : app.seasons.next(app.clock);
            seasonJumped(app);
            break;
        case GLFW_KEY_T:
            app.seasons.toggleTour();
            showToast(app, std::string("季節の自動巡回 ") + onOff(app.seasons.touring()));
            break;
        case GLFW_KEY_UP: app.clock.faster(); speedChanged(app); break;
        case GLFW_KEY_DOWN: app.clock.slower(); speedChanged(app); break;
        case GLFW_KEY_SPACE: app.clock.togglePause(); speedChanged(app); break;
        case GLFW_KEY_R: app.clock.toggleReverse(); speedChanged(app); break;
        case GLFW_KEY_N:
            app.clock.setNow();
            app.clock.resetRate();
            app.transition = true;
            std::printf("[time] now, x1\n");
            showToast(app, "現在時刻・実時間");
            break;
        case GLFW_KEY_C: o.compass = !o.compass; showToast(app, std::string("方位 ") + onOff(o.compass)); break;
        case GLFW_KEY_G: o.grid = !o.grid; showToast(app, std::string("グリッド ") + onOff(o.grid)); break;
        case GLFW_KEY_K:
            o.constellations = !o.constellations;
            showToast(app, std::string("星座線 ") + onOff(o.constellations));
            break;
        case GLFW_KEY_J: o.labels = !o.labels; showToast(app, std::string("名前 ") + onOff(o.labels)); break;
        case GLFW_KEY_I: o.info = !o.info; std::printf("[view] info %s\n", onOff(o.info)); break;
        case GLFW_KEY_P: toggleScenario(app); break;
        case GLFW_KEY_V: toggleProjection(app); break;
        case GLFW_KEY_F11: toggleFullscreen(app); std::printf("[view] fullscreen toggled\n"); break;
        case GLFW_KEY_H: printHelp(); break;
        default: break;
    }
}

void installControls(AppState& app) {
    glfwSetWindowUserPointer(app.window, &app);
    glfwSetKeyCallback(app.window, onKey);
}

void printHelp() {
    std::printf(
        "---- keys ----\n"
        "  0           real mode\n"
        "  1-8         fantasy modes\n"
        "  M / Shift+M next / previous mode\n"
        "  S / Shift+S next / previous season (cyclic)\n"
        "  F1-F5       jump to season preset\n"
        "  T           season auto tour on/off\n"
        "  L / Shift+L next / previous location\n"
        "  Up / Down   time speed step up / down\n"
        "  Space       pause / resume\n"
        "  R           reverse time\n"
        "  Left/Right  -/+ 1 hour (Shift: 1 day)\n"
        "  N           back to now, x1\n"
        "  K           constellation lines on/off\n"
        "  J           names (stars, planets, constellations) on/off\n"
        "  C           compass on/off\n"
        "  G           alt/az grid on/off\n"
        "  I           on-screen info on/off\n"
        "  P           play / stop scenario\n"
        "  V           fisheye / perspective\n"
        "  F11         fullscreen\n"
        "  H           help, Esc quit\n");
}

void showToast(AppState& app, const std::string& text) {
    app.toast = text;
    app.toastTime = glfwGetTime();
    std::printf("[toast] %s\n", text.c_str());
}

void applyScenarioStep(AppState& app, const ScenarioStep& s) {
    if (!s.location.empty()) {
        for (size_t i = 0; i < app.locations.size(); ++i) {
            if (app.locations[i].id == s.location) app.location = i;
        }
    }
    if (!s.time.empty()) app.clock.setLocalString(s.time);
    if (s.speed > 0) app.clock.setSpeed(s.speed);
    if (!s.mode.empty()) app.mode = findMode(s.mode);
    if (s.constellations >= 0) app.overlays.constellations = s.constellations != 0;
    if (s.labels >= 0) app.overlays.labels = s.labels != 0;
    app.transition = true;
    std::printf("[scenario] step %zu/%zu %s\n", app.scenario->index() + 1, app.scenario->size(), s.message.c_str());
    if (!s.message.empty()) showToast(app, s.message);
}

std::string statusLine(const AppState& app) {
    const auto& mode = allModes()[app.mode];
    const char* state = app.clock.paused() ? "||" : (app.clock.reversed() ? "<<" : ">>");
    const std::string season = app.seasons.currentName();
    char buf[512];
    std::snprintf(buf, sizeof(buf), "[%s] %s  %s  %s x%g%s%s%s  太陽高度 %+.0f°  %.0f fps  |  現在 %s", mode.name,
                  app.locations[app.location].name.c_str(), app.clock.formatLocal().c_str(), state,
                  app.clock.speed(), season.empty() ? "" : "  季節: ", season.c_str(),
                  app.seasons.touring() ? " (巡回中)" : "", app.sunAltDeg, app.fps, app.clock.formatRealNow().c_str());
    return buf;
}

std::vector<std::string> infoLines(const AppState& app) {
    const char* state = app.clock.paused() ? "一時停止" : (app.clock.reversed() ? "逆再生" : "");
    char line1[128];
    std::snprintf(line1, sizeof(line1), "%s  x%g %s", app.clock.formatLocal().substr(0, 16).c_str(),
                  app.clock.speed(), state);
    std::string line2 = app.locations[app.location].name + "  " + allModes()[app.mode].name;
    if (!app.seasons.currentName().empty()) line2 += "  " + app.seasons.currentName();
    if (app.scenario && app.scenario->playing()) line2 += "  ▶シナリオ";
    return {line1, line2};
}
