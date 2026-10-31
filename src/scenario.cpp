#include "scenario.hpp"

const ScenarioStep* ScenarioPlayer::start() {
    if (scenario_.steps.empty()) return nullptr;
    playing_ = true;
    index_ = 0;
    elapsed_ = 0.0;
    return &scenario_.steps[0];
}

const ScenarioStep* ScenarioPlayer::update(double realDt) {
    if (!playing_) return nullptr;
    elapsed_ += realDt;
    if (elapsed_ < scenario_.steps[index_].duration_sec) return nullptr;
    elapsed_ = 0.0;
    if (index_ + 1 < scenario_.steps.size()) {
        ++index_;
    } else if (scenario_.loop) {
        index_ = 0;
    } else {
        playing_ = false;
        return nullptr;
    }
    return &scenario_.steps[index_];
}
