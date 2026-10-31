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
    read(root, "operator_window", cfg.operator_window);
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
        read(r, "show_constellations", c.show_constellations);
        read(r, "show_constellation_names", c.show_constellation_names);
        read(r, "show_labels", c.show_labels);
        read(r, "show_info", c.show_info);
        read(r, "moon_scale", c.moon_scale);
        read(r, "bloom", c.bloom);
        read(r, "dither", c.dither);
        read(r, "font", c.font);
    }
    if (auto list = root["locations"]) {
        for (const auto& o : list) {
            ObserverConfig c;
            read(o, "id", c.id);
            read(o, "name", c.name);
            read(o, "latitude_deg", c.latitude_deg);
            read(o, "longitude_deg", c.longitude_deg);
            read(o, "elevation_m", c.elevation_m);
            read(o, "timezone_hours", c.timezone_hours);
            read(o, "limiting_magnitude", c.limiting_magnitude);
            read(o, "landscape", c.landscape);
            if (c.id.empty()) throw std::runtime_error("every entry in locations needs an id");
            cfg.locations.push_back(c);
        }
    }
    read(root, "location", cfg.location);
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
    if (auto s = root["scenario"]) {
        read(s, "loop", cfg.scenario.loop);
        for (const auto& n : s["steps"]) {
            ScenarioStep st;
            read(n, "duration_sec", st.duration_sec);
            read(n, "location", st.location);
            read(n, "time", st.time);
            read(n, "speed", st.speed);
            read(n, "mode", st.mode);
            read(n, "message", st.message);
            if (n["constellations"]) st.constellations = n["constellations"].as<bool>() ? 1 : 0;
            if (n["labels"]) st.labels = n["labels"].as<bool>() ? 1 : 0;
            if (n["constellation_names"]) st.constellationNames = n["constellation_names"].as<bool>() ? 1 : 0;
            cfg.scenario.steps.push_back(st);
        }
    }

    if (cfg.output != "debug" && cfg.output != "projector") {
        throw std::runtime_error("output must be 'debug' or 'projector': " + cfg.output);
    }
    if (cfg.projection.type != "fisheye" && cfg.projection.type != "perspective") {
        throw std::runtime_error("projection.type must be 'fisheye' or 'perspective': " + cfg.projection.type);
    }
    if (cfg.time.speed_steps.empty()) throw std::runtime_error("time.speed_steps is empty");
    if (cfg.locations.empty()) throw std::runtime_error("locations is empty");
    for (auto& l : cfg.locations) {
        if (l.limiting_magnitude < 0) l.limiting_magnitude = cfg.render.limiting_magnitude;
    }
    return cfg;
}
