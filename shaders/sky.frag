#version 450 core
// 背景: 画素ごとに逆投影して方向を求め、空の明るさ・天の川・演出・地上の景色・座標グリッドを塗る

in vec2 v_ndc;

uniform vec3 u_sunDir;          // 太陽方向（地平座標）
uniform vec3 u_zenithColor;     // 線形 sRGB（薄明・月明かり込み）
uniform vec3 u_horizonColor;
uniform vec3 u_sunGlowColor;
uniform vec3 u_groundColor;
uniform vec3 u_tint;            // モードごとの色味（幻想モード）
uniform mat3 u_eqToHor;         // J2000 赤道 -> 地平
uniform float u_milkyWay;       // 天の川の明るさ（0 で無し）
uniform float u_lightPollution; // 街明かり 0..1
uniform float u_aurora;         // オーロラ（幻想モード）
uniform float u_nebula;         // 星雲（幻想モード）
uniform float u_time;           // 実時間 [s]
uniform bool u_showGrid;

out vec4 o_color;

// ---- ノイズ（値ノイズ + fBm）。オーロラ・星雲で使う ----
float hash3(vec3 p) {
    p = fract(p * 0.3183099 + 0.1);
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float valueNoise(vec3 x) {
    vec3 i = floor(x), f = fract(x);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(hash3(i), hash3(i + vec3(1, 0, 0)), f.x),
                   mix(hash3(i + vec3(0, 1, 0)), hash3(i + vec3(1, 1, 0)), f.x), f.y),
               mix(mix(hash3(i + vec3(0, 0, 1)), hash3(i + vec3(1, 0, 1)), f.x),
                   mix(hash3(i + vec3(0, 1, 1)), hash3(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}

float fbm(vec3 p) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 5; ++i) {
        s += a * valueNoise(p);
        p *= 2.03;
        a *= 0.5;
    }
    return s;
}

// ---- 天の川 ----
// 起動時に milkyway_bake.frag が全天の画像（J2000 の赤経・赤緯）に描いておいたものを引く
uniform sampler2D u_milkyWayTex;

vec3 milkyWay(vec3 j) {
    vec2 uv = vec2(fract(atan(j.y, j.x) / 6.28318530718), asin(clamp(j.z, -1.0, 1.0)) / 3.14159265359 + 0.5);
    return texture(u_milkyWayTex, uv).rgb;
}

// ---- オーロラ（北の空のカーテン）----
vec3 aurora(float alt, float az) {
    float fromNorth = abs(mod(az + 180.0, 360.0) - 180.0);
    float mask = smoothstep(95.0, 30.0, fromNorth);
    float x = radians(az) * 3.0;
    float base = 12.0 + 6.0 * sin(x * 0.7 + u_time * 0.35) + 5.0 * fbm(vec3(x * 0.5, u_time * 0.12, 1.0));
    float height = 25.0 + 10.0 * fbm(vec3(x * 0.3, 2.0, u_time * 0.03));
    float above = alt - base;
    float curtain = smoothstep(-2.0, 1.0, above) * exp(-max(above, 0.0) / height * 2.5);
    float rays = 0.15 + 0.85 * smoothstep(0.3, 0.75, fbm(vec3(x * 9.0, u_time * 0.6, 0.0)));  // 縦の筋
    vec3 color = mix(vec3(0.1, 1.0, 0.45), vec3(0.8, 0.2, 0.7), smoothstep(0.35, 1.0, above / height));
    return color * curtain * rays * mask * 0.12;
}

// ---- 星雲（星と一緒に動く色の雲）----
vec3 nebula(vec3 j) {
    float density = smoothstep(0.45, 0.85, fbm(j * 2.5 + vec3(5.0)));
    vec3 color = mix(vec3(0.9, 0.2, 0.5), vec3(0.2, 0.5, 1.0), fbm(j * 4.0 + vec3(11.0)));
    return color * density * density * 0.10;
}

// value [deg] が step の倍数に近いほど 1。線幅は画面上でほぼ一定 [px]
float gridLine(float value, float stepDeg, float deriv, float widthPx) {
    float d = abs(fract(value / stepDeg + 0.5) - 0.5) * stepDeg;
    return 1.0 - smoothstep(0.0, max(deriv, 1e-6) * widthPx, d);
}

void main() {
    vec3 h;
    if (!unprojectNdc(v_ndc, h)) {
        o_color = vec4(0.0, 0.0, 0.0, 1.0);  // 魚眼の円の外
        return;
    }
    float alt = degrees(asin(clamp(h.z, -1.0, 1.0)));
    float az = degrees(atan(h.y, h.x));  // 北 = 0, 東 = 90（-180..180）
    float ground = groundAltDeg(az);

    vec3 c;
    if (alt < ground) {
        // 地面・山・建物のシルエット。空の明るさがわずかに映る
        c = u_groundColor + u_horizonColor * 0.04;
        if (u_landscape == 2) {
            // ビルの窓明かり
            vec2 cell = floor(vec2(az * 6.0, alt * 6.0));
            float lit = step(0.9, fract(sin(dot(cell, vec2(12.9898, 78.233))) * 43758.5453));
            c += lit * vec3(0.03, 0.022, 0.012) * (0.3 + u_lightPollution);
        }
    } else {
        float t = pow(1.0 - clamp(alt, 0.0, 90.0) / 90.0, 3.0);  // 地平線ほど明るい
        c = mix(u_zenithColor, u_horizonColor, t);
        float toSun = max(dot(normalize(h), u_sunDir), 0.0);
        c += u_sunGlowColor * pow(toSun, 8.0) * (0.3 + 0.7 * t);
        // 街明かり: 地平線付近がオレンジ色に明るく、空全体も少し明るい
        c += u_lightPollution * (vec3(0.035, 0.024, 0.012) * exp(-max(alt, 0.0) / 8.0) + vec3(0.0012, 0.0011, 0.001));
        vec3 j = transpose(u_eqToHor) * h;  // 星と一緒に動く座標（J2000）
        if (u_milkyWay > 0.0) c += milkyWay(j) * 0.02 * u_milkyWay * smoothstep(0.0, 8.0, alt);
        if (u_nebula > 0.0) c += nebula(j) * u_nebula;
        if (u_aurora > 0.0) c += aurora(alt, az) * u_aurora;
        c += u_tint;
    }

    if (u_showGrid) {
        float dAlt = fwidth(alt);
        // atan の不連続（真南）で微分が跳ねないよう、ずらした値の微分と小さい方を使う
        float dAz = min(fwidth(az), fwidth(mod(az + 360.0, 360.0)));
        float zenithFade = smoothstep(88.0, 75.0, alt);  // 天頂付近は経線が集まるので消す
        float altLines = gridLine(alt, 15.0, dAlt, 1.0);
        float azLines = gridLine(az, 30.0, dAz, 1.0) * zenithFade * step(ground, alt);
        float horizon = gridLine(alt, 360.0, dAlt, 2.0);
        float north = gridLine(az, 360.0, dAz, 2.0) * zenithFade * step(ground, alt);
        c += vec3(0.04, 0.06, 0.10) * max(altLines, azLines);
        c += vec3(0.10, 0.12, 0.08) * horizon;
        c += vec3(0.20, 0.02, 0.02) * north;
    }
    o_color = vec4(c, 1.0);
}
