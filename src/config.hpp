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

struct ProjectionConfig {
    std::string type = "fisheye";  // fisheye | perspective
    double fov_deg = 180.0;
    double center_alt_deg = 90.0;
    double center_az_deg = 180.0;
    double rotation_deg = 0.0;
};

struct RenderConfig {
    double limiting_magnitude = 6.0;
    double saturation_magnitude = 3.0;  // これより明るい星は大きさで明るさを表す
    double star_size = 0.8;             // 星像の σ [px]（高さ 800px 基準）
    double brightness = 1.0;
    double extinction = 0.25;           // 大気減光係数 [mag / エアマス]
    double exposure = 1.0;
    double gamma = 2.2;
    bool show_grid = false;
    bool show_compass = false;
};

// 観測地。YAML の locations に並べ、L キーで切り替える
struct ObserverConfig {
    std::string id;
    std::string name;
    double latitude_deg = 34.82;
    double longitude_deg = 135.52;
    double elevation_m = 0.0;
    double timezone_hours = 9.0;
    double limiting_magnitude = -1.0;  // その場所の夜空の最暗等級（光害）。負なら render.limiting_magnitude
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
    ProjectionConfig projection;
    RenderConfig render;
    std::vector<ObserverConfig> locations;  // 1 件以上
    std::string location;                   // 起動時の観測地の id（空なら先頭）
    TimeConfig time;
    std::vector<SeasonPreset> seasons;
    SeasonTourConfig season_tour;
    std::string mode = "real";
};

// YAML から設定を読み込む。失敗時は std::runtime_error を投げる。
Config loadConfig(const std::string& path);
