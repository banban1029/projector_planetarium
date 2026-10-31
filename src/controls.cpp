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
        clearShow(app);
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
            // 星座の表示を 線 → 線と名前 → なし の順に切り替える
            if (!o.constellations && !o.constellationNames) {
                o.constellations = true;
                showToast(app, "星座: 線");
            } else if (!o.constellationNames) {
                o.constellations = o.constellationNames = true;
                showToast(app, "星座: 線と名前");
            } else {
                o.constellations = o.constellationNames = false;
                showToast(app, "星座: OFF");
            }
            break;
        case GLFW_KEY_J:
            o.labels = !o.labels;
            showToast(app, std::string("星・惑星の名前 ") + onOff(o.labels));
            break;
        case GLFW_KEY_I: o.info = !o.info; std::printf("[view] info %s\n", onOff(o.info)); break;
        case GLFW_KEY_P: toggleScenario(app); break;
        case GLFW_KEY_V: toggleProjection(app); break;
        case GLFW_KEY_F11:
            toggleFullscreen(app);
            updateCursor(app);
            std::printf("[view] fullscreen toggled\n");
            break;
        case GLFW_KEY_X:
            o.testPattern = !o.testPattern;
            showToast(app, std::string("テストパターン ") + onOff(o.testPattern));
            break;
        case GLFW_KEY_B:
            app.audioMuted = !app.audioMuted;
            showToast(app, std::string("音 ") + onOff(!app.audioMuted));
            break;
        case GLFW_KEY_F12:
            o.credits = !o.credits;
            showToast(app, std::string("クレジット ") + onOff(o.credits));
            break;
        case GLFW_KEY_H: printHelp(); break;
        default: break;
    }
}

void installControls(AppState& app) { attachControls(app.window, app); }

void attachControls(GLFWwindow* window, AppState& app) {
    glfwSetWindowUserPointer(window, &app);
    glfwSetKeyCallback(window, onKey);
}

void updateCursor(AppState& app) {
    if (!app.window) return;
    const bool fullscreen = glfwGetWindowMonitor(app.window) != nullptr;
    glfwSetInputMode(app.window, GLFW_CURSOR, fullscreen ? GLFW_CURSOR_HIDDEN : GLFW_CURSOR_NORMAL);
    std::printf("[view] cursor %s\n", fullscreen ? "hidden" : "shown");
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
        "  K           constellations: lines -> lines + names -> off\n"
        "  J           star / planet names on/off\n"
        "  C           compass on/off\n"
        "  G           alt/az grid on/off\n"
        "  I           on-screen info on/off\n"
        "  P           play / stop scenario\n"
        "  V           fisheye / perspective\n"
        "  X           test pattern (projector adjustment)\n"
        "  F12         credits\n"
        "  B           sound on/off (show narration and music)\n"
        "  F11         fullscreen\n"
        "  H           help, Esc quit\n");
}

void showToast(AppState& app, const std::string& text) {
    app.toast = text;
    app.toastTime = glfwGetTime();
    std::printf("[toast] %s\n", text.c_str());
}

void applyScenarioStep(AppState& app, const ScenarioStep& s) {
    const Scenario& sc = app.scenario->scenario();
    // location: main → 上映の本編の観測地
    const std::string location = s.location == "main" ? sc.location : s.location;
    if (!location.empty()) {
        for (size_t i = 0; i < app.locations.size(); ++i) {
            if (app.locations[i].id == location) app.location = i;
        }
    }
    // time: start → 上映の日時、"start+5:00" → その 5 時間後
    if (s.time.rfind("start", 0) == 0 && !sc.date.empty()) {
        app.clock.setLocalString(sc.date);
        int h = 0, m = 0;
        if (std::sscanf(s.time.c_str(), "start+%d:%d", &h, &m) >= 1) app.clock.shift(h * 3600.0 + m * 60.0);
    } else if (!s.time.empty()) {
        app.clock.setLocalString(s.time);
    }
    if (s.speed > 0) app.clock.setSpeed(s.speed);
    if (!s.mode.empty()) app.mode = findMode(s.mode);
    if (s.constellations >= 0) app.overlays.constellations = s.constellations != 0;
    if (s.constellationNames >= 0) app.overlays.constellationNames = s.constellationNames != 0;
    if (s.labels >= 0) app.overlays.labels = s.labels != 0;
    if (s.credits >= 0) app.overlays.credits = s.credits != 0;
    auto& show = app.show;
    show.subtitle = s.subtitle;
    show.asterisms = s.asterisms;
    show.point = s.point;
    show.stepTime = glfwGetTime();
    show.stepIndex = app.scenario->index();
    show.stepChanged = true;
    if (s.eyeAdaptSec > 0) {
        show.eyeAdaptStart = show.stepTime;
        show.eyeAdaptSec = s.eyeAdaptSec;
    }
    if (s.meteorRate >= 0) show.meteorRate = s.meteorRate;
    if (s.meteorRadiant.size() == 2) {
        show.hasRadiant = true;
        show.radiantRa = s.meteorRadiant[0];
        show.radiantDec = s.meteorRadiant[1];
    }
    app.transitionSec = s.fadeSec > 0 ? static_cast<float>(s.fadeSec) : 1.0f;
    app.transition = true;
    std::printf("[scenario] step %zu/%zu %s\n", app.scenario->index() + 1, app.scenario->size(), s.message.c_str());
    if (!s.message.empty()) showToast(app, s.message);
}

void clearShow(AppState& app) {
    app.show = AppState::Show{};
    app.overlays.credits = false;
    app.transitionSec = 1.0f;
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

std::vector<std::string> operatorLines(const AppState& app) {
    const auto& o = app.overlays;
    const char* state = app.clock.paused() ? "一時停止" : (app.clock.reversed() ? "逆再生" : "再生");
    char buf[256];
    std::vector<std::string> lines;
    std::snprintf(buf, sizeof(buf), "#%s  x%g %s   %.0f fps", app.clock.formatLocal().c_str(), app.clock.speed(), state,
                  app.fps);
    lines.push_back(buf);
    std::string where = "#" + app.locations[app.location].name + "  /  " + allModes()[app.mode].name;
    if (!app.seasons.currentName().empty()) where += "  /  季節: " + app.seasons.currentName();
    lines.push_back(where);
    std::snprintf(buf, sizeof(buf), "表示: 星座線 %s  星座名 %s  星の名前 %s  方位 %s  グリッド %s  画面表示 %s",
                  onOff(o.constellations), onOff(o.constellationNames), onOff(o.labels), onOff(o.compass),
                  onOff(o.grid), onOff(o.info));
    lines.push_back(buf);
    if (app.scenario && app.scenario->playing()) {
        std::snprintf(buf, sizeof(buf), "シナリオ再生中: %zu / %zu 場面", app.scenario->index() + 1, app.scenario->size());
        lines.push_back(buf);
        if (!app.show.subtitle.empty()) {
            std::string sub = app.show.subtitle;
            for (auto p = sub.find('\n'); p != std::string::npos; p = sub.find('\n')) sub.replace(p, 1, " ");
            lines.push_back("字幕: " + sub);
        }
    } else {
        lines.push_back("シナリオ: 停止中（P で再生）");
    }
    if (o.testPattern) lines.push_back("#テストパターン表示中（X で戻る）");
    lines.push_back("");
    lines.push_back("0-8 モード   M 次のモード   L 観測地   S 次の季節   F1-F5 季節   T 季節の自動巡回");
    lines.push_back("↑↓ 時間倍率   Space 一時停止   R 逆再生   ←→ ±1時間   N 現在時刻");
    lines.push_back("K 星座   J 星の名前   C 方位   G グリッド   I 画面表示   V 投影   P シナリオ");
    lines.push_back("X テストパターン   F12 クレジット   B 音   F11 全画面   Esc 終了");
    return lines;
}
