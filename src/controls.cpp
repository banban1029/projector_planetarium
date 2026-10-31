#include "controls.hpp"

#include <GLFW/glfw3.h>

#include <cstdio>

#include "modes.hpp"

namespace {
void selectMode(AppState& app, size_t index) {
    const auto& modes = allModes();
    app.mode = index % modes.size();
    std::printf("[mode] %zu: %s\n", app.mode, modes[app.mode].name);
}

// 魚眼（天頂中心・全天）<-> 透視（南の空・画角 90°）。デバッグ用
void toggleProjection(AppState& app) {
    auto& v = app.view;
    v.fisheye = !v.fisheye;
    if (v.fisheye) {
        v.fovDeg = 180.0;
        v.centerAltDeg = 90.0;
    } else {
        v.fovDeg = 90.0;
        v.centerAltDeg = 30.0;
    }
    v.centerAzDeg = 180.0;
    std::printf("[view] %s\n", v.fisheye ? "fisheye (zenith)" : "perspective (south)");
}

void toggleFullscreen(AppState& app) {
    GLFWwindow* w = app.window;
    if (glfwGetWindowMonitor(w)) {
        glfwSetWindowMonitor(w, nullptr, app.windowedX, app.windowedY, app.windowedW, app.windowedH, 0);
        return;
    }
    glfwGetWindowPos(w, &app.windowedX, &app.windowedY);
    glfwGetWindowSize(w, &app.windowedW, &app.windowedH);
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* vm = glfwGetVideoMode(monitor);
    glfwSetWindowMonitor(w, monitor, 0, 0, vm->width, vm->height, vm->refreshRate);
}

void onKey(GLFWwindow* window, int key, int /*scancode*/, int action, int mods) {
    auto& app = *static_cast<AppState*>(glfwGetWindowUserPointer(window));
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
        app.seasons.jumpTo(static_cast<size_t>(key - GLFW_KEY_F1), app.clock);
        return;
    }

    switch (key) {
        case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(window, 1); break;
        case GLFW_KEY_M: selectMode(app, app.mode + (shift ? allModes().size() - 1 : 1)); break;
        case GLFW_KEY_S: shift ? app.seasons.prev(app.clock) : app.seasons.next(app.clock); break;
        case GLFW_KEY_T: app.seasons.toggleTour(); break;
        case GLFW_KEY_L: {
            const size_t n = app.locations.size();
            app.location = (app.location + (shift ? n - 1 : 1)) % n;
            const auto& l = app.locations[app.location];
            std::printf("[location] %s (%.2f, %.2f)\n", l.name.c_str(), l.latitude_deg, l.longitude_deg);
            break;
        }
        case GLFW_KEY_UP: app.clock.faster(); break;
        case GLFW_KEY_DOWN: app.clock.slower(); break;
        case GLFW_KEY_SPACE: app.clock.togglePause(); break;
        case GLFW_KEY_R: app.clock.toggleReverse(); break;
        case GLFW_KEY_N:
            app.clock.setNow();
            app.clock.resetRate();
            std::printf("[time] now, x1\n");
            break;
        case GLFW_KEY_G:
            app.overlays.grid = !app.overlays.grid;
            std::printf("[view] grid %s\n", app.overlays.grid ? "ON" : "OFF");
            break;
        case GLFW_KEY_C:
            app.overlays.compass = !app.overlays.compass;
            std::printf("[view] compass (N/E/S/W) %s\n", app.overlays.compass ? "ON" : "OFF");
            break;
        case GLFW_KEY_V: toggleProjection(app); break;
        case GLFW_KEY_F11: toggleFullscreen(app); break;
        case GLFW_KEY_H: printHelp(); break;
        default: break;
    }
}
}  // namespace

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
        "  C           compass N/E/S/W on/off\n"
        "  G           alt/az grid on/off\n"
        "  V           fisheye / perspective\n"
        "  F11         fullscreen\n"
        "  H           help, Esc quit\n");
}

std::string statusLine(const AppState& app) {
    const auto& mode = allModes()[app.mode];
    const char* state = app.clock.paused() ? "||" : (app.clock.reversed() ? "<<" : ">>");
    const std::string season = app.seasons.currentName();
    char buf[512];
    std::snprintf(buf, sizeof(buf), "[%s] %s  %s  %s x%g%s%s%s  太陽高度 %+.0f°  |  現在 %s", mode.name,
                  app.locations[app.location].name.c_str(), app.clock.formatLocal().c_str(), state,
                  app.clock.speed(),
                  season.empty() ? "" : "  季節: ", season.c_str(),
                  app.seasons.touring() ? " (巡回中)" : "", app.sunAltDeg, app.clock.formatRealNow().c_str());
    return buf;
}
