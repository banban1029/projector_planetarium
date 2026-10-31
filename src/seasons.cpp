#include "seasons.hpp"

#include <cmath>
#include <cstdio>
#include <limits>

namespace {
// 同じ季節の直近へ再ジャンプしないための余裕（シミュレーション時間）
constexpr double kMargin = 86400.0;
}  // namespace

SeasonCycler::SeasonCycler(std::vector<SeasonPreset> presets, double tourIntervalSec, bool tourEnabled)
    : presets_(std::move(presets)), tourInterval_(tourIntervalSec), touring_(tourEnabled) {}

double SeasonCycler::occurrence(size_t index, int year, const SimClock& clock) const {
    const auto& p = presets_[index];
    return clock.localToUnix(year, p.month, p.day, p.hour, p.minute);
}

void SeasonCycler::jump(size_t index, double unixTime, SimClock& clock) {
    clock.setUnixTime(unixTime);
    current_ = static_cast<int>(index);
    tourElapsed_ = 0.0;
    std::printf("[season] %s -> %s\n", presets_[index].name.c_str(), clock.formatLocal().c_str());
}

void SeasonCycler::next(SimClock& clock) {
    if (presets_.empty()) return;
    const double now = clock.unixTime();
    const int year = clock.local().year;
    double best = std::numeric_limits<double>::infinity();
    size_t bestIndex = 0;
    for (size_t i = 0; i < presets_.size(); ++i) {
        for (int y = year - 1; y <= year + 1; ++y) {
            const double t = occurrence(i, y, clock);
            if (t > now + kMargin && t < best) {
                best = t;
                bestIndex = i;
            }
        }
    }
    jump(bestIndex, best, clock);
}

void SeasonCycler::prev(SimClock& clock) {
    if (presets_.empty()) return;
    const double now = clock.unixTime();
    const int year = clock.local().year;
    double best = -std::numeric_limits<double>::infinity();
    size_t bestIndex = 0;
    for (size_t i = 0; i < presets_.size(); ++i) {
        for (int y = year - 1; y <= year + 1; ++y) {
            const double t = occurrence(i, y, clock);
            if (t < now - kMargin && t > best) {
                best = t;
                bestIndex = i;
            }
        }
    }
    jump(bestIndex, best, clock);
}

void SeasonCycler::jumpTo(size_t index, SimClock& clock) {
    if (index >= presets_.size()) return;
    const double now = clock.unixTime();
    const int year = clock.local().year;
    double best = occurrence(index, year, clock);
    for (int y : {year - 1, year + 1}) {
        const double t = occurrence(index, y, clock);
        if (std::abs(t - now) < std::abs(best - now)) best = t;
    }
    jump(index, best, clock);
}

void SeasonCycler::update(double realDt, SimClock& clock) {
    if (!touring_) return;
    tourElapsed_ += realDt;
    if (tourElapsed_ >= tourInterval_) next(clock);
}

void SeasonCycler::toggleTour() {
    touring_ = !touring_;
    tourElapsed_ = 0.0;
    std::printf("[season] tour %s (every %.0f s)\n", touring_ ? "ON" : "OFF", tourInterval_);
}

std::string SeasonCycler::currentName() const {
    return current_ < 0 ? std::string() : presets_[current_].name;
}
