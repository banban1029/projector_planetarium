#version 450 core
// 恒星 1 個 = 1 頂点（点スプライト）。位置・大気の影響・明るさ・大きさを計算する

layout(location = 0) in vec3 a_dir;    // J2000 赤道座標の単位ベクトル
layout(location = 1) in float a_mag;   // 実視等級
layout(location = 2) in vec3 a_color;  // 線形 sRGB

uniform mat3 u_eqToHor;     // J2000 赤道 -> 地平（歳差 + 日周運動）
uniform float u_magLimit;   // これより暗い星は描かない（薄明で明るくなる）
uniform float u_magSat;     // 芯の明るさが飽和する等級。これより明るい星は大きさで表す
uniform float u_brightness;
uniform float u_sigma;      // 飽和前の星像の半径（ガウスの σ）[px]
uniform float u_extinction; // 大気減光係数 [mag / エアマス]。0 で無効
uniform float u_twinkle;    // 瞬きの振幅 [mag]
uniform float u_glow;       // 明るい星の光芒の強さ
uniform float u_saturation; // 星の色の彩度（1 = 黒体色そのまま）
uniform float u_time;       // 実時間 [s]（瞬き用）

out vec3 v_color;
out float v_sigma;
out float v_glow;
out float v_size;

void hide() {
    gl_Position = vec4(2.0, 2.0, 2.0, 1.0);  // クリップ範囲外
    gl_PointSize = 0.0;
}

// 大気差 [deg]（Sæmundsson）。真高度 -> 見かけの高度
float refractionDeg(float altDeg) {
    if (altDeg < -2.0) return 0.0;
    return 1.02 / tan(radians(altDeg + 10.3 / (altDeg + 5.11))) / 60.0;
}

void main() {
    vec3 h = u_eqToHor * a_dir;
    float alt = degrees(asin(clamp(h.z, -1.0, 1.0)));
    alt += refractionDeg(alt);
    if (alt < 0.0) {
        hide();
        return;
    }
    float l = length(h.xy);
    h = vec3((l > 1e-7 ? h.xy / l : vec2(1.0, 0.0)) * cos(radians(alt)), sin(radians(alt)));

    // 大気減光: エアマス（Kasten & Young 1989）に比例して暗くなる。天頂を基準にする
    float airmass = 1.0 / (sin(radians(alt)) + 0.50572 * pow(alt + 6.07995, -1.6364));
    float mag = a_mag + u_extinction * (airmass - 1.0);

    // 瞬き: 星ごとに位相の違う 2 つの正弦波。地平線に近いほど強い
    float seed = float(gl_VertexID);
    float n = 0.6 * sin(u_time * 7.3 + seed * 12.9898) + 0.4 * sin(u_time * 13.1 + seed * 78.233);
    mag += u_twinkle * n * sqrt(min(airmass, 10.0));

    if (mag > u_magLimit) {
        hide();
        return;
    }
    vec3 p = projectHorizon(h);
    if (p.z < 0.5) {
        hide();
        return;
    }

    // 等級 -> 光量。芯は 1 で飽和させ、あふれた分は星像を大きくして表す
    float e = u_brightness * pow(10.0, -0.4 * (mag - u_magSat));
    float fade = clamp((u_magLimit - mag) / 0.5, 0.0, 1.0);
    float peak = min(e, 1.0) * fade;
    v_sigma = u_sigma * pow(max(e, 1.0), 0.3);
    v_glow = u_glow * clamp((u_magSat - mag) / 4.0, 0.0, 1.0);

    float extent = 3.0 * v_sigma + v_glow * 12.0 * u_sigma;
    v_size = 2.0 * extent + 1.0;
    gl_PointSize = v_size;

    float lum = dot(a_color, vec3(0.2126, 0.7152, 0.0722));
    v_color = max(mix(vec3(lum), a_color, u_saturation), 0.0) * peak;
    gl_Position = vec4(p.xy, 0.0, 1.0);
}
