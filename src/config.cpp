#include "config.hpp"

#include <cstdio>
#include <stdexcept>
#include <yaml-cpp/yaml.h>

namespace {
template <typename T>
void read(const YAML::Node& node, const char* key, T& out) {
    if (node[key]) out = node[key].as<T>();
}

SeasonPreset parseSeason(const YAML::Node& n) {
    SeasonPreset s;
    read(n, "name", s.name);
    const auto date = n["date"].as<std::string>("");
    const auto time = n["time"].as<std::string>("21:00");
    if (std::sscanf(date.c_str(), "%d-%d", &s.month, &s.day) != 2 ||
        std::sscanf(time.c_str(), "%d:%d", &s.hour, &s.minute) != 2) {
        throw std::runtime_error("invalid season preset '" + s.name + "': date must be MM-DD, time HH:MM");
    }
    return s;
}
}  // namespace

Config loadConfig(const std::string& path) {
    YAML::Node root;
    try {
        root = YAML::LoadFile(path);
    } catch (const YAML::Exception& e) {
        throw std::runtime_error("failed to load " + path + ": " + e.what());
    }

    Config cfg;
    read(root, "output", cfg.output);
    if (auto p = root["projector"]) {
        auto& c = cfg.projector;
        read(p, "name", c.name);
        read(p, "width", c.width);
        read(p, "height", c.height);
        read(p, "refresh_rate", c.refresh_rate);
        read(p, "throw_ratio", c.throw_ratio);
        read(p, "monitor_index", c.monitor_index);
        read(p, "flip_horizontal", c.flip_horizontal);
        read(p, "flip_vertical", c.flip_vertical);
        read(p, "vsync", c.vsync);
    }
    if (auto p = root["projection"]) {
        auto& c = cfg.projection;
        read(p, "type", c.type);
        read(p, "fov_deg", c.fov_deg);
        read(p, "center_alt_deg", c.center_alt_deg);
        read(p, "center_az_deg", c.center_az_deg);
        read(p, "rotation_deg", c.rotation_deg);
    }
    if (auto r = root["render"]) {
        auto& c = cfg.render;
        read(r, "limiting_magnitude", c.limiting_magnitude);
        read(r, "saturation_magnitude", c.saturation_magnitude);
        read(r, "star_size", c.star_size);
        read(r, "brightness", c.brightness);
        read(r, "extinction", c.extinction);
        read(r, "exposure", c.exposure);
        read(r, "gamma", c.gamma);
        read(r, "show_grid", c.show_grid);
        read(r, "show_compass", c.show_compass);
    }
    if (auto o = root["observer"]) {
        auto& c = cfg.observer;
        read(o, "name", c.name);
        read(o, "latitude_deg", c.latitude_deg);
        read(o, "longitude_deg", c.longitude_deg);
        read(o, "elevation_m", c.elevation_m);
        read(o, "timezone_hours", c.timezone_hours);
    }
    if (auto t = root["time"]) {
        read(t, "start", cfg.time.start);
        read(t, "speed", cfg.time.speed);
        read(t, "speed_steps", cfg.time.speed_steps);
    }
    if (auto s = root["seasons"]) {
        for (const auto& n : s) cfg.seasons.push_back(parseSeason(n));
    }
    if (auto t = root["season_tour"]) {
        read(t, "enabled", cfg.season_tour.enabled);
        read(t, "interval_sec", cfg.season_tour.interval_sec);
    }
    read(root, "mode", cfg.mode);

    if (cfg.output != "debug" && cfg.output != "projector") {
        throw std::runtime_error("output must be 'debug' or 'projector': " + cfg.output);
    }
    if (cfg.projection.type != "fisheye" && cfg.projection.type != "perspective") {
        throw std::runtime_error("projection.type must be 'fisheye' or 'perspective': " + cfg.projection.type);
    }
    if (cfg.time.speed_steps.empty()) throw std::runtime_error("time.speed_steps is empty");
    return cfg;
}
