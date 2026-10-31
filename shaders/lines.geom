#version 450 core
// 線分を、画面上で太さ u_widthPx の四角形に広げる（Core プロファイルでは glLineWidth > 1 が使えないため）

layout(lines) in;
layout(triangle_strip, max_vertices = 4) out;

in vec4 vg_color[];
in float vg_valid[];

uniform vec2 u_viewport;  // [px]
uniform float u_widthPx;

out vec4 v_color;
out float v_edge;  // 線の中心 0、縁 ±1

void main() {
    if (vg_valid[0] < 0.5 || vg_valid[1] < 0.5) return;
    vec2 a = gl_in[0].gl_Position.xy, b = gl_in[1].gl_Position.xy;
    vec2 d = (b - a) * u_viewport * 0.5;  // [px]
    float len = length(d);
    // 極端に長い線分は投影の切れ目（魚眼の反対側など）をまたいでいるので描かない
    if (len < 1e-4 || len > 0.4 * max(u_viewport.x, u_viewport.y)) return;
    vec2 n = vec2(-d.y, d.x) / len * (0.5 * u_widthPx + 1.0) * 2.0 / u_viewport;

    v_color = vg_color[0]; v_edge = 1.0;  gl_Position = vec4(a + n, 0.0, 1.0); EmitVertex();
    v_color = vg_color[0]; v_edge = -1.0; gl_Position = vec4(a - n, 0.0, 1.0); EmitVertex();
    v_color = vg_color[1]; v_edge = 1.0;  gl_Position = vec4(b + n, 0.0, 1.0); EmitVertex();
    v_color = vg_color[1]; v_edge = -1.0; gl_Position = vec4(b - n, 0.0, 1.0); EmitVertex();
    EndPrimitive();
}
