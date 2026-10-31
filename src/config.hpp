#pragma once
#include <string>
#include <vector>

struct ProjectorConfig {
    std::string name;
    int width = 1280;
    int height = 800;
    int refresh_rate = 60;
    double throw_ratio = 0.8;
    int monitor_index = 1;
    bool flip_horizontal = false;
    bool flip_vertical = false;
    bool vsync = true;
};

struct ObserverConfig {
    std::string name;
    double latitude_deg = 34.82;
    double longitude_deg = 135.52;
    double elevation_m = 0.0;
    double timezone_hours = 9.0;
};

struct TimeConfig {
    std::string start = "now";
    double speed = 1.0;
    std::vector<double> speed_steps{1, 10, 60, 300, 600, 1800, 3600, 21600, 86400};
};

// 毎年同じ月日・地方時で表す季節の代表時刻
struct SeasonPreset {
    std::string name;
    int month = 1;
    int day = 1;
    int hour = 21;
    int minute = 0;
};

struct SeasonTourConfig {
    bool enabled = false;
    double interval_sec = 30.0;
};

struct Config {
    std::string output = "debug";  // debug | projector
    ProjectorConfig projector;
    ObserverConfig observer;
    TimeConfig time;
    std::vector<SeasonPreset> seasons;
    SeasonTourConfig season_tour;
    std::string mode = "real";
};

// YAML から設定を読み込む。失敗時は std::runtime_error を投げる。
Config loadConfig(const std::string& path);
