#version 450 core
// 投影の調整用テストパターン（X キー）。ガンマなどをかけず、出力の値をそのまま出す（white_level だけは掛ける）。
//   下端: 灰色の 17 段階（0, 16, 32 … 255）。全段が見分けられるか → ガンマ・明るさ
//   上端: 暗部の 9 段階（0, 4, 8 … 32）。何段目から見えるか → 黒つぶれ・黒浮き（暗い星がどこまで見えるか）
//   中央と四隅: 1px の白黒の市松模様 → ピント（灰色に溶けていればぼけている）
//   格子: 画面の 10% ごと → 台形のゆがみ
//   魚眼の地平線と高度 30°・60° の円、方位線（北は赤） → 映像の北を部屋の北に合わせる（回転・反転の設定も反映）

in vec2 v_ndc;

uniform vec2 u_viewport;  // [px]
uniform float u_whiteLevel;  // 出力の最大値。星空と同じく掛けて、合わせた結果を確かめられるようにする

out vec4 o_color;

bool checker(vec2 px, vec2 center) {
    vec2 q = abs(px - center);
    return max(q.x, q.y) < 32.0;
}

void main() {
    vec2 px = gl_FragCoord.xy;
    vec2 uv = v_ndc * 0.5 + 0.5;
    vec3 c = vec3(0.0);

    if (uv.y < 0.08) {
        c = vec3(floor(uv.x * 17.0) / 16.0);
    } else if (uv.y > 0.92) {
        c = vec3(floor(uv.x * 9.0) * 4.0 / 255.0);
    } else {
        vec2 g = abs(fract(uv * 10.0 + 0.5) - 0.5) * u_viewport / 10.0;
        if (min(g.x, g.y) < 0.75) c = vec3(0.25);

        vec3 h;
        if (unprojectNdc(v_ndc, h)) {
            float alt = degrees(asin(clamp(h.z, -1.0, 1.0)));
            float az = degrees(atan(h.y, h.x));
            float dAlt = fwidth(alt);
            float dAz = min(fwidth(az), fwidth(mod(az + 360.0, 360.0)));
            if (abs(alt) < dAlt * 1.5 || abs(alt - 30.0) < dAlt || abs(alt - 60.0) < dAlt) c = vec3(0.6);
            if (alt > 1.0 && alt < 85.0) {
                if (abs(az) < dAz * 2.5) c = vec3(1.0, 0.15, 0.15);  // 北
                else if (abs(fract(az / 90.0 + 0.5) - 0.5) * 90.0 < dAz * 1.2) c = vec3(0.6);  // 東・南・西
            }
        }

        vec2 size = u_viewport;
        if (checker(px, size * 0.5) || checker(px, size * vec2(0.1, 0.2)) || checker(px, size * vec2(0.9, 0.2)) ||
            checker(px, size * vec2(0.1, 0.8)) || checker(px, size * vec2(0.9, 0.8))) {
            c = vec3(mod(floor(px.x) + floor(px.y), 2.0));
        }
    }
    o_color = vec4(c * u_whiteLevel, 1.0);
}
