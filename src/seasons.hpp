#pragma once
#include <string>
#include <vector>

#include "config.hpp"
#include "sim_clock.hpp"

// 季節プリセットへのジャンプと自動巡回。
// 次／前へのジャンプは「現在のシミュレーション時刻から見て次（前）に来る季節」へ年をまたいで移動する。
class SeasonCycler {
public:
    SeasonCycler(std::vector<SeasonPreset> presets, double tourIntervalSec, bool tourEnabled);

    void next(SimClock& clock);
    void prev(SimClock& clock);
    void jumpTo(size_t index, SimClock& clock);  // 最も近い年のその季節へ

    // 自動巡回中なら interval ごとに next() する。ジャンプしたら true
    bool update(double realDt, SimClock& clock);
    void toggleTour();

    bool touring() const { return touring_; }
    size_t size() const { return presets_.size(); }
    // 最後にジャンプした季節名（まだなら空）
    std::string currentName() const;

private:
    double occurrence(size_t index, int year, const SimClock& clock) const;
    void jump(size_t index, double unixTime, SimClock& clock);

    std::vector<SeasonPreset> presets_;
    double tourInterval_;
    bool touring_;
    double tourElapsed_ = 0.0;
    int current_ = -1;
};
