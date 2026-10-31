// 地平座標 (x = 北, y = 東, z = 天頂) と画面 NDC の相互変換。
// 各シェーダの #version 行の直後に差し込まれる（gl_util.cpp）。詳細は docs/rendering.md

uniform int u_projType;   // 0 = 魚眼（等距離射影）, 1 = 透視投影
uniform mat3 u_view;      // 地平座標 -> 視線座標 (x = 画面右, y = 画面上, z = 視線方向)
uniform float u_halfFov;  // 半画角 [rad]。魚眼: 円の縁, 透視: 縦方向
uniform vec2 u_aspect;    // 正方形の投影面 -> NDC（長辺方向を縮める）
uniform mat2 u_screen;    // 画面内の回転・反転

// 戻り値: xy = NDC, z = 1 なら投影範囲内
vec3 projectHorizon(vec3 h) {
    vec3 d = u_view * h;
    vec2 p;
    if (u_projType == 0) {
        float theta = acos(clamp(d.z, -1.0, 1.0));  // 視線中心からの角距離
        if (theta > u_halfFov) return vec3(0.0);
        float len = length(d.xy);
        p = (len > 1e-7 ? d.xy / len : vec2(0.0)) * (theta / u_halfFov);
    } else {
        if (d.z < 1e-4) return vec3(0.0);
        p = d.xy / (d.z * tan(u_halfFov));
    }
    return vec3((u_screen * p) * u_aspect, 1.0);
}

// ---- 地平線の景色（山並み・街並み）----
// 方位ごとの「地面の高さ」[deg]。星・文字・線はこれより下では描かない。sky.frag はこれより下を地面で塗る
uniform int u_landscape;  // 0 = なし, 1 = 山並み, 2 = 街並み

float landscapeHash(float x) { return fract(sin(x * 127.1) * 43758.5453); }

float groundAltDeg(float azDeg) {
    float a = radians(azDeg);
    if (u_landscape == 1) {
        // 周期関数の和なので方位 0° と 360° でつながる。高さ 0.8〜5°
        float h = 2.4 + 1.3 * sin(2.0 * a + 1.3) + 0.9 * sin(5.0 * a + 0.4) + 0.5 * sin(11.0 * a + 2.1) +
                  0.25 * sin(23.0 * a + 0.7) + 0.12 * sin(47.0 * a + 1.9);
        return max(h, 0.8);
    }
    if (u_landscape == 2) {
        // 幅 2.5° ごとの建物。ときどき高いビル
        float i = floor(mod(azDeg, 360.0) / 2.5);
        float r = landscapeHash(i);
        return 0.3 + 2.2 * r * r + (landscapeHash(i + 91.0) > 0.93 ? 2.0 : 0.0);
    }
    return 0.0;
}

// 地平座標の方向 h が景色より上にあるか（余裕 marginDeg）
bool aboveGround(vec3 h, float marginDeg) {
    float alt = degrees(asin(clamp(h.z, -1.0, 1.0)));
    return alt > groundAltDeg(degrees(atan(h.y, h.x))) + marginDeg;
}

// NDC -> 地平座標の方向。投影範囲外なら false
bool unprojectNdc(vec2 ndc, out vec3 h) {
    vec2 p = inverse(u_screen) * (ndc / u_aspect);
    vec3 d;
    if (u_projType == 0) {
        float r = length(p);
        if (r > 1.0) return false;
        float theta = r * u_halfFov;
        d = vec3((r > 1e-7 ? p / r : vec2(0.0)) * sin(theta), cos(theta));
    } else {
        d = normalize(vec3(p * tan(u_halfFov), 1.0));
    }
    h = transpose(u_view) * d;
    return true;
}
