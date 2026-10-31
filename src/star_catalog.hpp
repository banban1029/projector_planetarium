#pragma once
#include <string>
#include <vector>

// GPU にそのまま送る恒星データ（頂点属性）
struct StarVertex {
    float dir[3];    // J2000 赤道座標の単位ベクトル
    float mag;       // 実視等級
    float color[3];  // 線形 sRGB、最大成分 = 1
};

// scripts/fetch_hyg.py が出力した data/stars.csv を読む。失敗時は std::runtime_error
std::vector<StarVertex> loadStarCatalog(const std::string& path);

// 色指数 B-V -> 有効温度 [K]（Ballesteros 2012）
double temperatureFromBV(double bv);
// 黒体の色（線形 sRGB、最大成分 = 1）
void blackbodyColor(double kelvin, float rgb[3]);
