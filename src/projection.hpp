#pragma once
#include "astro.hpp"

// 視点・投影の設定（キー操作で変わるので Config とは別に持つ）
struct ViewSettings {
    bool fisheye = true;
    double fovDeg = 180.0;
    double centerAltDeg = 90.0;
    double centerAzDeg = 180.0;
    double rotationDeg = 0.0;
    bool flipH = false;
    bool flipV = false;
};

// 視線方向（地平座標の単位ベクトル）
astro::Vec3 viewForward(const ViewSettings& view);

// 投影のパラメータ。shaders/projection.glsl の uniform と 1 対 1 に対応し、
// CPU 側でも同じ投影を計算できる（名前の配置の重なり判定に使う）
struct Projection {
    double view[3][3];    // 地平 -> 視線座標（行 = 画面右, 画面上, 視線方向）
    bool fisheye;
    double halfFovRad;
    double aspect[2];     // 正方形の投影面 -> NDC
    double screen[2][2];  // 画面内の回転・反転
    int landscape;        // 0 = なし, 1 = 山並み, 2 = 街並み

    // 地平座標の方向 -> NDC。投影範囲外なら false（projection.glsl の projectHorizon と同じ式）
    bool project(const astro::Vec3& h, double& x, double& y) const;
};

Projection makeProjection(const ViewSettings& view, int width, int height, int landscape);

// 水鏡モードは魚眼の画角を広げ、地平線の下を水面にする（景色なし）
void applyMirror(ViewSettings& view, int& landscape);

// 方位ごとの地面の高さ [deg]（projection.glsl の groundAltDeg と同じ式）
double groundAltDeg(int landscape, double azDeg);
