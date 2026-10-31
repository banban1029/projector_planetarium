#pragma once
#include <string>

#include "seasons.hpp"
#include "sim_clock.hpp"
#include "sky_renderer.hpp"

struct GLFWwindow;

// キーボード操作で書き換えるアプリの状態
struct AppState {
    GLFWwindow* window = nullptr;
    SimClock clock;
    SeasonCycler seasons;
    size_t mode = 0;
    ViewSettings view{};
    Overlays overlays{};
    double sunAltDeg = 0.0;  // 表示用（毎フレーム更新）

    // F11 で全画面から戻るときのウィンドウ位置・サイズ
    int windowedX = 100, windowedY = 100, windowedW = 1280, windowedH = 800;
};

void installControls(AppState& app);
void printHelp();
std::string statusLine(const AppState& app);
