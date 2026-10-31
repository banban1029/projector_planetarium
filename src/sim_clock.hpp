#pragma once
#include <string>
#include <vector>

struct LocalDateTime {
    int year, month, day, hour, minute;
    double second;
};

// シミュレーション時刻。内部は Unix 時刻（UTC 秒）で持ち、観測地の地方時との変換を行う。
// 実時間 dt ごとに speed 倍で進む（reverse で逆行、pause で停止）。
class SimClock {
public:
    SimClock(double timezoneHours, std::vector<double> speedSteps, double initialSpeed);

    void update(double realDt);

    void setNow();
    // "YYYY-MM-DD HH:MM"（地方時, 'T' 区切りも可）または "now"
    bool setLocalString(const std::string& text);
    void setUnixTime(double t) { unix_ = t; }
    void shift(double seconds) { unix_ += seconds; }

    double unixTime() const { return unix_; }
    double julianDate() const { return unix_ / 86400.0 + 2440587.5; }
    double localToUnix(int year, int month, int day, int hour, int minute, double second = 0) const;
    LocalDateTime local() const { return localAt(unix_); }
    LocalDateTime localAt(double unixTime) const;
    std::string formatLocal() const;
    // 実際の現在時刻（シミュレーション時刻とは無関係）を観測地の地方時で
    std::string formatRealNow() const;

    void faster();
    void slower();
    void togglePause() { paused_ = !paused_; }
    void toggleReverse() { reversed_ = !reversed_; }
    void resetRate();  // 実時間（×1, 順行, 再生中）に戻す
    void setSpeed(double speed);  // 最も近い倍率の段階にする（順行・再生中）

    double speed() const { return steps_[step_]; }
    bool paused() const { return paused_; }
    bool reversed() const { return reversed_; }

private:
    double timezoneHours_;
    std::vector<double> steps_;
    size_t step_ = 0;
    bool paused_ = false;
    bool reversed_ = false;
    double unix_ = 0.0;
};
