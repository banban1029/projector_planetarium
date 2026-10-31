#version 450 core
// 縁を 1px でぼかした線。加算合成で描く

in vec4 v_color;
in float v_edge;

uniform float u_widthPx;
uniform float u_alpha;
uniform vec3 u_color;  // 頂点の色に掛ける色

out vec4 o_color;

void main() {
    float hw = 0.5 * u_widthPx;
    float d = abs(v_edge) * (hw + 1.0);  // 中心からの距離 [px]
    float a = 1.0 - smoothstep(hw - 0.5, hw + 0.5, d);
    o_color = vec4(v_color.rgb * u_color * v_color.a * a * u_alpha, 1.0);
}
