#pragma once
#include <string>

#include "seasons.hpp"
#include "sim_clock.hpp"

struct GLFWwindow;

// キーボード操作で書き換えるアプリの状態
struct AppState {
    GLFWwindow* window = nullptr;
    SimClock clock;
    SeasonCycler seasons;
    size_t mode = 0;

    // F11 で全画面から戻るときのウィンドウ位置・サイズ
    int windowedX = 100, windowedY = 100, windowedW = 1280, windowedH = 800;
};

void installControls(AppState& app);
void printHelp();
std::string statusLine(const AppState& app);
