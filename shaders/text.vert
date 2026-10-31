#version 450 core
// 文字の四角形。基準点（星座・星・方位の方向、または画面上の位置）を投影し、
// 文字の形は画面上のピクセル単位で足す（投影で文字が歪まないように）

layout(location = 0) in vec3 a_pos;     // 基準点: 方向ベクトル または NDC
layout(location = 1) in float a_anchor; // 0 = J2000 赤道座標, 1 = 地平座標, 2 = 画面 (NDC)
layout(location = 2) in vec2 a_offset;  // 基準点からのずらし [px]
layout(location = 3) in vec2 a_uv;
layout(location = 4) in vec4 a_color;

uniform mat3 u_eqToHor;
uniform vec2 u_viewport;  // [px]

out vec2 v_uv;
out vec4 v_color;

void main() {
    vec2 base;
    float fade = 1.0;
    if (a_anchor < 1.5) {
        vec3 h = a_anchor < 0.5 ? u_eqToHor * a_pos : a_pos;
        float alt = degrees(asin(clamp(h.z, -1.0, 1.0)));
        float ground = groundAltDeg(degrees(atan(h.y, h.x)));
        fade = smoothstep(ground, ground + 3.0, alt);  // 地平線に沈むときに消えていく
        vec3 p = projectHorizon(h);
        if (p.z < 0.5 || fade <= 0.0) {
            gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
            return;
        }
        base = p.xy;
    } else {
        base = u_screen * a_pos.xy;  // 画面全体の反転・回転に合わせる
    }
    // 画像の回転・反転に文字も合わせる（リア投影で反転したとき、投影面で正しく読めるように）
    gl_Position = vec4(base + (u_screen * a_offset) * 2.0 / u_viewport, 0.0, 1.0);
    v_uv = a_uv;
    v_color = vec4(a_color.rgb, a_color.a * fade);
}
