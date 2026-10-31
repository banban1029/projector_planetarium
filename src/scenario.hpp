#pragma once
#include <string>
#include <vector>

// シナリオの 1 場面。空欄・負の値の項目は「変更しない」
struct ScenarioStep {
    double duration_sec = 10.0;  // この場面を見せる実時間
    std::string location;        // 観測地の id
    std::string time;            // "YYYY-MM-DD HH:MM"（地方時）
    double speed = -1.0;         // 時間倍率
    std::string mode;            // 表示モードの id
    std::string message;         // 画面に表示する文字
    int constellations = -1;     // 星座線 0/1
    int labels = -1;             // 星・惑星・星座の名前 0/1
};

struct Scenario {
    bool loop = true;
    std::vector<ScenarioStep> steps;
};

// 実時間でシナリオを進める。場面が切り替わった瞬間だけ、その場面を返す
class ScenarioPlayer {
public:
    explicit ScenarioPlayer(Scenario scenario) : scenario_(std::move(scenario)) {}

    // 再生開始（先頭の場面を返す）。場面が無ければ nullptr
    const ScenarioStep* start();
    void stop() { playing_ = false; }
    // 次の場面に切り替わったらその場面、それ以外は nullptr。最後まで行き loop でなければ停止
    const ScenarioStep* update(double realDt);

    bool playing() const { return playing_; }
    size_t index() const { return index_; }
    size_t size() const { return scenario_.steps.size(); }

private:
    Scenario scenario_;
    bool playing_ = false;
    size_t index_ = 0;
    double elapsed_ = 0.0;
};
