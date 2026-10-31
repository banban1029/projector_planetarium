#include "guide.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {
using astro::Vec3;

std::vector<std::vector<std::string>> readCsv(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open " + path);
    std::vector<std::vector<std::string>> rows;
    std::string line;
    bool header = true;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        if (header) {
            header = false;
            continue;
        }
        std::vector<std::string> f;
        std::stringstream ss(line);
        std::string field;
        while (std::getline(ss, field, ',')) f.push_back(field);
        if (line.back() == ',') f.emplace_back();
        rows.push_back(std::move(f));
    }
    return rows;
}

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, sep)) out.push_back(item);
    return out;
}

double dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 normalize(const Vec3& v) {
    const double l = std::sqrt(dot(v, v));
    return {v.x / l, v.y / l, v.z / l};
}
Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

// a から b へ大円に沿って fraction だけ進んだ方向
Vec3 slerp(const Vec3& a, const Vec3& b, double fraction) {
    const double angle = std::acos(std::clamp(dot(a, b), -1.0, 1.0));
    if (angle < 1e-9) return a;
    const double wa = std::sin((1.0 - fraction) * angle) / std::sin(angle);
    const double wb = std::sin(fraction * angle) / std::sin(angle);
    return {wa * a.x + wb * b.x, wa * a.y + wb * b.y, wa * a.z + wb * b.z};
}

// 惑星などの英語名 → 日本語名（bodies.cpp の名前）
const std::unordered_map<std::string, std::string> kBodyNames = {
    {"Sun", "太陽"},  {"Moon", "月"},     {"Mercury", "水星"}, {"Venus", "金星"},
    {"Mars", "火星"}, {"Jupiter", "木星"}, {"Saturn", "土星"},  {"Uranus", "天王星"}};
}  // namespace

SkyGuide::SkyGuide(const std::string& dataDir) {
    for (const auto& f : readCsv(dataDir + "/star_names.csv")) {
        if (f.size() >= 2) japanese_[f[0]] = f[1];
    }
    for (const auto& f : readCsv(dataDir + "/stars.csv")) {  // hip,ra_deg,dec_deg,mag,ci,con,proper
        if (f.size() < 7 || f[6].empty()) continue;
        const Vec3 dir = astro::equatorialVector(std::stod(f[1]), std::stod(f[2]));
        stars_[f[6]] = dir;
        if (auto it = japanese_.find(f[6]); it != japanese_.end()) {
            stars_[it->second] = dir;
            // 「ベガ（織姫星）」のような名前は、括弧の前（「ベガ」）でも引けるようにする
            const auto paren = it->second.find("（");
            if (paren != std::string::npos) stars_[it->second.substr(0, paren)] = dir;
        }
    }
    for (const auto& f : readCsv(dataDir + "/asterisms.csv")) {  // id,name_ja,lines
        if (f.size() < 3) continue;
        Asterism a{f[0], f[1], {}, {0, 0, 0}};
        Vec3 sum{0, 0, 0};
        int count = 0;
        for (const auto& polyline : split(f[2], ';')) {
            std::vector<Vec3> points;
            for (const auto& name : split(polyline, '-')) {
                auto it = stars_.find(name);
                if (it == stars_.end()) throw std::runtime_error("asterism " + a.id + ": unknown star " + name);
                points.push_back(it->second);
                sum = {sum.x + it->second.x, sum.y + it->second.y, sum.z + it->second.z};
                ++count;
            }
            a.lines.push_back(points);
        }
        a.center = count ? normalize(sum) : Vec3{1, 0, 0};
        asterisms_.push_back(a);
    }
}

const SkyGuide::Asterism* SkyGuide::asterism(const std::string& id) const {
    for (const auto& a : asterisms_) {
        if (a.id == id) return &a;
    }
    return nullptr;
}

bool SkyGuide::find(const std::string& name, const std::vector<BodyInfo>& bodies, Vec3& dir,
                    std::string& display) const {
    std::string ja = name;
    if (auto it = kBodyNames.find(name); it != kBodyNames.end()) ja = it->second;
    for (const auto& b : bodies) {
        if (ja == b.nameJa) {
            dir = b.dirJ2000;
            display = b.nameJa;
            return true;
        }
    }
    auto it = stars_.find(name);
    if (it == stars_.end()) return false;
    dir = it->second;
    auto jt = japanese_.find(name);
    display = jt != japanese_.end() ? jt->second : name;
    return true;
}

void SkyGuide::appendAsterism(std::vector<LineVertex>& out, const Asterism& a, double progress, float alpha) const {
    size_t total = 0;
    for (const auto& line : a.lines) total += line.size() > 1 ? line.size() - 1 : 0;
    // 線分を 1 本ずつ順に伸ばす。progress = 1 ですべて描く
    const double drawn = std::clamp(progress, 0.0, 1.0) * total;
    const float gold[4] = {1.0f, 0.78f, 0.38f, alpha};
    size_t index = 0;
    for (const auto& line : a.lines) {
        for (size_t i = 0; i + 1 < line.size(); ++i, ++index) {
            const double part = std::clamp(drawn - index, 0.0, 1.0);
            if (part <= 0.0) return;
            appendArc(out, line[i], slerp(line[i], line[i + 1], part), 0.0f, gold, gold, 1.0);
        }
    }
}

void SkyGuide::appendPointer(std::vector<LineVertex>& out, const Vec3& dir, double radiusDeg, float alpha) const {
    const Vec3 d = normalize(dir);
    const Vec3 u = normalize(std::abs(d.z) < 0.9 ? cross(d, Vec3{0, 0, 1}) : cross(d, Vec3{1, 0, 0}));
    const Vec3 v = cross(d, u);
    const double r = radiusDeg * astro::kDeg;
    const float color[4] = {0.75f, 0.92f, 1.0f, alpha};
    const int segments = 48;
    auto at = [&](int k) {
        const double t = 2.0 * astro::kPi * k / segments;
        const double c = std::cos(t) * std::sin(r), s = std::sin(t) * std::sin(r);
        return Vec3{d.x * std::cos(r) + u.x * c + v.x * s, d.y * std::cos(r) + u.y * c + v.y * s,
                    d.z * std::cos(r) + u.z * c + v.z * s};
    };
    for (int k = 0; k < segments; ++k) {
        const Vec3 a = at(k), b = at(k + 1);
        LineVertex va{{float(a.x), float(a.y), float(a.z)}, 0.0f, {color[0], color[1], color[2], color[3]}};
        LineVertex vb{{float(b.x), float(b.y), float(b.z)}, 0.0f, {color[0], color[1], color[2], color[3]}};
        out.push_back(va);
        out.push_back(vb);
    }
}
