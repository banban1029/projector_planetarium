// キー操作のテスト。GLFW のキーコールバックと同じ handleKey() に、すべてのキーを順に送って状態を確かめる。
// ウィンドウは作らない（Esc と F11 はウィンドウが無くても落ちないことだけ確かめる）
#include "controls.hpp"  // GL/glew.h を含むので GLFW より先に

#include <GLFW/glfw3.h>

#include <cstdio>
#include <string>

#include "modes.hpp"

namespace {
int failures = 0;

void check(const char* name, bool ok, const std::string& detail = "") {
    std::printf("[%s] %-48s %s\n", ok ? " OK " : "FAIL", name, detail.c_str());
    if (!ok) ++failures;
}

void press(AppState& app, int key, int mods = 0) { handleKey(app, key, GLFW_PRESS, mods); }

std::string localTime(const AppState& app) { return app.clock.formatLocal().substr(0, 16); }
}  // namespace

int main() {
    glfwInit();  // 通知の時刻（glfwGetTime）に使う

    ObserverConfig osaka, achi;
    osaka.id = "osaka";
    osaka.name = "大阪";
    achi.id = "achi";
    achi.name = "阿智村";
    Scenario scenario;
    scenario.steps = {{5.0, "achi", "2026-12-15 18:00", 600, "jewel", "テスト場面", 1, 1, 1}};
    AppState app{
        nullptr,
        SimClock(9.0, {1, 10, 60, 600, 3600, 86400}, 1.0),
        SeasonCycler({{"春", 4, 15, 21, 0}, {"夏", 8, 10, 21, 0}, {"秋", 10, 20, 21, 0}, {"冬", 1, 20, 21, 0}}, 30.0,
                     false),
        0,
        {osaka, achi},
        0,
        std::make_unique<ScenarioPlayer>(scenario),
    };
    app.clock.setLocalString("2026-10-31 21:00");

    // モード: 0〜8 で直接、9 は無し、M / Shift+M で巡回
    for (int k = 0; k <= 8; ++k) {
        press(app, GLFW_KEY_0 + k);
        check(("key " + std::to_string(k) + " -> mode " + allModes()[k].id).c_str(), app.mode == size_t(k));
    }
    press(app, GLFW_KEY_9);
    check("key 9 is ignored (only 9 modes)", app.mode == 8);
    press(app, GLFW_KEY_M);
    check("M wraps to real", app.mode == 0);
    press(app, GLFW_KEY_M, GLFW_MOD_SHIFT);
    check("Shift+M goes back to mirror", app.mode == 8);
    check("mode change requests a fade", app.transition);
    app.transition = false;

    // 観測地
    press(app, GLFW_KEY_L);
    check("L -> achi", app.location == 1);
    press(app, GLFW_KEY_L, GLFW_MOD_SHIFT);
    check("Shift+L -> osaka", app.location == 0);

    // 季節
    press(app, GLFW_KEY_S);
    check("S -> winter (next year)", localTime(app) == "2027-01-20 21:00", localTime(app));
    press(app, GLFW_KEY_S, GLFW_MOD_SHIFT);
    check("Shift+S -> autumn", localTime(app) == "2026-10-20 21:00", localTime(app));
    press(app, GLFW_KEY_F2);
    check("F2 -> summer (nearest)", localTime(app) == "2026-08-10 21:00", localTime(app));
    press(app, GLFW_KEY_F5);
    check("F5 is ignored (only 4 presets)", localTime(app) == "2026-08-10 21:00", localTime(app));
    press(app, GLFW_KEY_T);
    check("T starts the season tour", app.seasons.touring());
    press(app, GLFW_KEY_T);
    check("T stops the season tour", !app.seasons.touring());
    check("toast shows the last action", app.toast.find("季節の自動巡回") == 0, app.toast);

    // 時間
    press(app, GLFW_KEY_UP);
    press(app, GLFW_KEY_UP);
    check("Up x2 -> x60", app.clock.speed() == 60.0);
    press(app, GLFW_KEY_DOWN);
    check("Down -> x10", app.clock.speed() == 10.0);
    press(app, GLFW_KEY_SPACE);
    check("Space pauses", app.clock.paused());
    press(app, GLFW_KEY_SPACE);
    press(app, GLFW_KEY_R);
    check("R reverses", app.clock.reversed() && !app.clock.paused());
    press(app, GLFW_KEY_RIGHT);
    check("Right -> +1 h", localTime(app) == "2026-08-10 22:00", localTime(app));
    handleKey(app, GLFW_KEY_RIGHT, GLFW_REPEAT, 0);
    check("Right (repeat) -> +1 h", localTime(app) == "2026-08-10 23:00", localTime(app));
    press(app, GLFW_KEY_LEFT, GLFW_MOD_SHIFT);
    check("Shift+Left -> -1 day", localTime(app) == "2026-08-09 23:00", localTime(app));
    handleKey(app, GLFW_KEY_K, GLFW_RELEASE, 0);
    check("key release is ignored", !app.overlays.constellations);
    press(app, GLFW_KEY_N);
    check("N -> x1, forward, now", app.clock.speed() == 1.0 && !app.clock.reversed() &&
                                       app.clock.formatLocal().substr(0, 4) != "2026-08");

    // 表示
    press(app, GLFW_KEY_K);
    check("K (1st) -> constellation lines", app.overlays.constellations && !app.overlays.constellationNames);
    press(app, GLFW_KEY_K);
    check("K (2nd) -> lines + names", app.overlays.constellations && app.overlays.constellationNames);
    press(app, GLFW_KEY_K);
    check("K (3rd) -> off", !app.overlays.constellations && !app.overlays.constellationNames);
    press(app, GLFW_KEY_K);
    press(app, GLFW_KEY_K);
    press(app, GLFW_KEY_J);
    check("J toggles star / planet names only", app.overlays.labels && app.overlays.constellationNames);
    press(app, GLFW_KEY_J);
    check("J again -> star names off, constellation names kept", !app.overlays.labels && app.overlays.constellationNames);
    press(app, GLFW_KEY_J);
    press(app, GLFW_KEY_C);
    check("C toggles compass", app.overlays.compass);
    press(app, GLFW_KEY_G);
    check("G toggles grid", app.overlays.grid);
    press(app, GLFW_KEY_I);
    check("I toggles info", app.overlays.info);
    press(app, GLFW_KEY_X);
    check("X toggles test pattern", app.overlays.testPattern);
    press(app, GLFW_KEY_X);
    press(app, GLFW_KEY_F12);
    check("F12 toggles credits", app.overlays.credits && !app.overlays.testPattern);
    press(app, GLFW_KEY_F12);
    press(app, GLFW_KEY_V);
    check("V -> perspective south", !app.view.fisheye && app.view.centerAltDeg == 30.0 && app.view.fovDeg == 90.0);
    press(app, GLFW_KEY_V);
    check("V -> fisheye zenith", app.view.fisheye && app.view.centerAltDeg == 90.0 && app.view.fovDeg == 180.0);

    // シナリオ
    app.overlays.constellations = app.overlays.constellationNames = app.overlays.labels = false;
    press(app, GLFW_KEY_P);
    check("P starts scenario and applies step", app.scenario->playing() && app.location == 1 &&
                                                    allModes()[app.mode].id == std::string("jewel") &&
                                                    localTime(app) == "2026-12-15 18:00" &&
                                                    app.clock.speed() == 600.0 && app.overlays.constellations &&
                                                    app.overlays.constellationNames && app.overlays.labels,
          localTime(app));
    check("scenario message is shown", app.toast == "テスト場面", app.toast);
    press(app, GLFW_KEY_P);
    check("P stops scenario", !app.scenario->playing());

    // ウィンドウが無くても落ちない
    press(app, GLFW_KEY_F11);
    press(app, GLFW_KEY_ESCAPE);
    press(app, GLFW_KEY_H);
    check("F11 / Esc / H without a window", true);

    glfwTerminate();
    std::printf("%s (%d failures)\n", failures ? "FAILED" : "PASSED", failures);
    return failures ? 1 : 0;
}
