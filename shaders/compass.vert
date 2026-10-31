#version 450 core
// 方位の文字（N/E/S/W）。文字の基準点は地平線のすぐ上の方向を投影して求め、
// 文字の形は画面上のピクセル単位のオフセットとして足す（投影で文字が歪まないように）

layout(location = 0) in float a_az;     // 方位 [deg]（北 = 0, 東 = 90）
layout(location = 1) in vec2 a_offset;  // 文字内の位置（文字の高さ = 1, y 上向き）
layout(location = 2) in vec3 a_color;

uniform float u_labelAlt;  // 文字を置く高度 [deg]
uniform float u_glyphPx;   // 文字の高さ [px]
uniform vec2 u_viewport;   // [px]

out vec3 v_color;

void main() {
    float az = radians(a_az), alt = radians(u_labelAlt);
    vec3 p = projectHorizon(vec3(cos(alt) * cos(az), cos(alt) * sin(az), sin(alt)));
    if (p.z < 0.5) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);  // 画面外の方位は描かない
        return;
    }
    // 画像の回転・反転に文字も合わせる（リア投影で反転したとき、投影面で正しく読めるように）
    vec2 offset = (u_screen * a_offset) * u_glyphPx * 2.0 / u_viewport;
    gl_Position = vec4(p.xy + offset, 0.0, 1.0);
    v_color = a_color;
}
