#pragma once
#include <vector>

#include "astro.hpp"
#include "config.hpp"

// 太陽・月・惑星（Astronomy Engine で計算）
struct BodyInfo {
    enum Kind { Sun, Moon, Planet } kind;
    const char* nameJa;
    astro::Vec3 dirJ2000;     // 観測地から見た方向（J2000 赤道座標, 光行差込み）
    double mag;               // 実視等級
    double phaseAngleDeg;     // 位相角（0 = 満月, 180 = 新月）
    double phaseFraction;     // 輝面比（0..1）
    double angularRadiusDeg;  // 視半径
    float color[3];           // 線形 sRGB
};

// jdUt の時刻に obs から見た太陽・月・惑星（水星〜天王星）
std::vector<BodyInfo> computeBodies(double jdUt, const ObserverConfig& obs);
