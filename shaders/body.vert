#version 450 core
// 月・太陽の円盤（点スプライト）。画面上の大きさと、太陽の来る向き（満ち欠けの向き）を求める

layout(location = 0) in vec3 a_dir;          // J2000 赤道座標
layout(location = 1) in float a_radiusDeg;   // 描く視半径（実際の moon_scale 倍）
layout(location = 2) in float a_phaseAngle;  // 位相角 [rad]（0 = 満月）
layout(location = 3) in vec3 a_color;        // 明るさ込みの色（線形）
layout(location = 4) in float a_glow;        // 周りのにじみ

uniform mat3 u_eqToHor;
uniform vec3 u_sunDir;    // 地平座標
uniform vec2 u_viewport;  // [px]

out vec3 v_color;
out vec2 v_lightDir;  // 画面上で太陽の来る向き（単位ベクトル, y 上向き）
out float v_phase;
out float v_radiusPx;
out float v_size;
out float v_glow;

float refractionDeg(float altDeg) {
    if (altDeg < -2.0) return 0.0;
    return 1.02 / tan(radians(altDeg + 10.3 / (altDeg + 5.11))) / 60.0;
}

void main() {
    vec3 h = u_eqToHor * a_dir;
    float alt = degrees(asin(clamp(h.z, -1.0, 1.0)));
    alt += refractionDeg(alt);
    float l = length(h.xy);
    h = vec3((l > 1e-7 ? h.xy / l : vec2(1.0, 0.0)) * cos(radians(alt)), sin(radians(alt)));
    vec3 p = projectHorizon(h);
    if (p.z < 0.5 || !aboveGround(h, -a_radiusDeg)) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        gl_PointSize = 0.0;
        return;
    }
    // 視半径だけ離れた点を投影して、画面上の半径 [px] を求める
    vec3 side = abs(h.z) < 0.99 ? normalize(cross(h, vec3(0.0, 0.0, 1.0))) : vec3(1.0, 0.0, 0.0);
    vec3 pr = projectHorizon(normalize(h * cos(radians(a_radiusDeg)) + side * sin(radians(a_radiusDeg))));
    v_radiusPx = max(length((pr.xy - p.xy) * u_viewport * 0.5), 1.5);

    // 太陽の方向へ少し動かした点を投影して、画面上の「太陽の来る向き」を求める
    vec3 toSun = u_sunDir - h * dot(u_sunDir, h);
    toSun = length(toSun) > 1e-6 ? normalize(toSun) : side;
    vec3 ps = projectHorizon(normalize(h + toSun * 0.01));
    vec2 d = (ps.xy - p.xy) * u_viewport;
    v_lightDir = length(d) > 1e-6 ? normalize(d) : vec2(1.0, 0.0);

    v_phase = a_phaseAngle;
    v_color = a_color;
    v_glow = a_glow;
    v_size = v_radiusPx * (a_glow > 0.0 ? 8.0 : 2.4) + 2.0;
    gl_PointSize = v_size;
    gl_Position = vec4(p.xy, 0.0, 1.0);
}
