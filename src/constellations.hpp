#pragma once
#include <string>
#include <vector>

#include "astro.hpp"

// 天球上の名前（星座名・星の名前）
struct SkyLabel {
    astro::Vec3 dirJ2000;
    std::string text;
    int rank;     // 星座: 1 = 主要, 2, 3 = 目立たない / 星: 1
    float mag;    // 星の名前のとき、その星の等級（星座名は 0）
};

struct ConstellationData {
    std::vector<std::pair<astro::Vec3, astro::Vec3>> segments;  // 星座線（J2000）
    std::vector<SkyLabel> names;                                 // 星座名（日本語）
};

// data/constellation_lines.csv と data/constellation_names.csv を読む。失敗時は std::runtime_error
ConstellationData loadConstellations(const std::string& dataDir);

// data/star_names.csv の日本語名を、data/stars.csv の proper 列で星に結び付ける
std::vector<SkyLabel> loadStarNames(const std::string& dataDir);
