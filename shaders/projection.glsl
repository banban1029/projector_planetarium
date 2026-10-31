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
