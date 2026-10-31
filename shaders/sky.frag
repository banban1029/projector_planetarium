#version 450 core
// 背景: 画素ごとに逆投影して方向を求め、空の明るさ・地面・座標グリッドを塗る

in vec2 v_ndc;

uniform vec3 u_sunDir;        // 太陽方向（地平座標）
uniform vec3 u_zenithColor;   // 線形 sRGB
uniform vec3 u_horizonColor;
uniform vec3 u_sunGlowColor;
uniform vec3 u_groundColor;
uniform vec3 u_tint;          // モードごとの色味（幻想モード）
uniform bool u_showGrid;

out vec4 o_color;

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

    vec3 c;
    if (alt < 0.0) {
        c = u_groundColor;
    } else {
        float t = pow(1.0 - alt / 90.0, 3.0);  // 地平線ほど明るい
        c = mix(u_zenithColor, u_horizonColor, t);
        float toSun = max(dot(normalize(h), u_sunDir), 0.0);
        c += u_sunGlowColor * pow(toSun, 8.0) * (0.3 + 0.7 * t);
        c += u_tint;
    }

    if (u_showGrid) {
        float dAlt = fwidth(alt);
        // atan の不連続（真南）で微分が跳ねないよう、ずらした値の微分と小さい方を使う
        float dAz = min(fwidth(az), fwidth(mod(az + 360.0, 360.0)));
        float zenithFade = smoothstep(88.0, 75.0, alt);  // 天頂付近は経線が集まるので消す
        float altLines = gridLine(alt, 15.0, dAlt, 1.0);
        float azLines = gridLine(az, 30.0, dAz, 1.0) * zenithFade * step(0.0, alt);
        float horizon = gridLine(alt, 360.0, dAlt, 2.0);
        float north = gridLine(az, 360.0, dAz, 2.0) * zenithFade * step(0.0, alt);
        c += vec3(0.04, 0.06, 0.10) * max(altLines, azLines);
        c += vec3(0.10, 0.12, 0.08) * horizon;
        c += vec3(0.20, 0.02, 0.02) * north;
    }
    o_color = vec4(c, 1.0);
}
