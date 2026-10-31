#include "sim_clock.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace {
// グレゴリオ暦 <-> 1970-01-01 からの日数（Howard Hinnant のアルゴリズム）
int64_t daysFromCivil(int y, int m, int d) {
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const int yoe = static_cast<int>(y - era * 400);
    const int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

void civilFromDays(int64_t z, int& y, int& m, int& d) {
    z += 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const int doe = static_cast<int>(z - era * 146097);
    const int yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const int mp = (5 * doy + 2) / 153;
    d = doy - (153 * mp + 2) / 5 + 1;
    m = mp < 10 ? mp + 3 : mp - 9;
    y = static_cast<int>(yoe + era * 400 + (m <= 2));
}

double realUnixNow() {
    using namespace std::chrono;
    return duration<double>(system_clock::now().time_since_epoch()).count();
}
}  // namespace

SimClock::SimClock(double timezoneHours, std::vector<double> speedSteps, double initialSpeed)
    : timezoneHours_(timezoneHours), steps_(std::move(speedSteps)) {
    std::sort(steps_.begin(), steps_.end());
    // initialSpeed に最も近い段階を選ぶ
    for (size_t i = 0; i < steps_.size(); ++i) {
        if (std::abs(steps_[i] - initialSpeed) < std::abs(steps_[step_] - initialSpeed)) step_ = i;
    }
    setNow();
}

void SimClock::update(double realDt) {
    if (paused_) return;
    unix_ += realDt * speed() * (reversed_ ? -1.0 : 1.0);
}

void SimClock::setNow() { unix_ = realUnixNow(); }

bool SimClock::setLocalString(const std::string& text) {
    if (text == "now") {
        setNow();
        return true;
    }
    int y, mo, d, h = 0, mi = 0;
    char sep;
    const int n = std::sscanf(text.c_str(), "%d-%d-%d%c%d:%d", &y, &mo, &d, &sep, &h, &mi);
    if (n != 3 && n != 6) return false;
    unix_ = localToUnix(y, mo, d, h, mi);
    return true;
}

double SimClock::localToUnix(int year, int month, int day, int hour, int minute, double second) const {
    const double days = static_cast<double>(daysFromCivil(year, month, day));
    return days * 86400.0 + hour * 3600.0 + minute * 60.0 + second - timezoneHours_ * 3600.0;
}

LocalDateTime SimClock::localAt(double unixTime) const {
    const double t = unixTime + timezoneHours_ * 3600.0;
    const double days = std::floor(t / 86400.0);
    double sec = t - days * 86400.0;
    LocalDateTime lt{};
    civilFromDays(static_cast<int64_t>(days), lt.year, lt.month, lt.day);
    lt.hour = static_cast<int>(sec / 3600.0);
    sec -= lt.hour * 3600.0;
    lt.minute = static_cast<int>(sec / 60.0);
    lt.second = sec - lt.minute * 60.0;
    return lt;
}

std::string SimClock::formatLocal() const {
    const auto lt = local();
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d (UTC%+g)", lt.year, lt.month, lt.day,
                  lt.hour, lt.minute, static_cast<int>(lt.second), timezoneHours_);
    return buf;
}

std::string SimClock::formatRealNow() const {
    const auto lt = localAt(realUnixNow());
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d", lt.year, lt.month, lt.day, lt.hour,
                  lt.minute, static_cast<int>(lt.second));
    return buf;
}

void SimClock::faster() {
    if (step_ + 1 < steps_.size()) ++step_;
}

void SimClock::slower() {
    if (step_ > 0) --step_;
}

void SimClock::resetRate() {
    step_ = 0;
    for (size_t i = 0; i < steps_.size(); ++i) {
        if (std::abs(steps_[i] - 1.0) < std::abs(steps_[step_] - 1.0)) step_ = i;
    }
    paused_ = false;
    reversed_ = false;
}
