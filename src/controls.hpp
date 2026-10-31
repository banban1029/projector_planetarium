#pragma once
#include <memory>
#include <string>
#include <vector>

#include "config.hpp"
#include "scenario.hpp"
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
    std::vector<ObserverConfig> locations;
    size_t location = 0;
    std::unique_ptr<ScenarioPlayer> scenario;
    ViewSettings view{};
    Overlays overlays{};
    double sunAltDeg = 0.0;  // 表示用（毎フレーム更新）
    double fps = 0.0;        // 表示用（1 秒ごとに更新）

    // 画面に数秒だけ出す通知（プロジェクタ出力ではウィンドウタイトルが見えないため）
    std::string toast{};
    double toastTime = -100.0;
    // 場面が切り替わったのでフェードする（メインループで処理して false に戻す）
    bool transition = false;

    // F11 で全画面から戻るときのウィンドウ位置・サイズ
    int windowedX = 100, windowedY = 100, windowedW = 1280, windowedH = 800;
};

void installControls(AppState& app);
// 別のウィンドウ（操作パネル）でも同じキー操作を受け付ける
void attachControls(GLFWwindow* window, AppState& app);
// 全画面のときはマウスカーソルを隠す（星空の上に矢印が出ないように）
void updateCursor(AppState& app);
// キー 1 回分の処理（GLFW のキーコールバックから呼ぶ。テストからも呼ぶ。window が nullptr でもよい）
void handleKey(AppState& app, int key, int action, int mods);
void printHelp();
void showToast(AppState& app, const std::string& text);
// シナリオの場面を反映する
void applyScenarioStep(AppState& app, const ScenarioStep& step);
// ウィンドウタイトル用の 1 行
std::string statusLine(const AppState& app);
// 画面表示（I キー）用の 2 行
std::vector<std::string> infoLines(const AppState& app);
// 操作パネル用の行（# で始まる行は見出し）
std::vector<std::string> operatorLines(const AppState& app);
