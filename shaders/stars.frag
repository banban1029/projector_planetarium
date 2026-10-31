#version 450 core
// 星像 = ガウス分布の芯 + 明るい星だけに付く指数関数的な光芒（+ 宝石箱モードの十字の光条）。加算合成で重ねる

in vec3 v_color;
in float v_sigma;
in float v_glow;
in float v_size;

uniform float u_spikes;  // 十字の光条の強さ（0 = なし）

out vec4 o_color;

void main() {
    vec2 q = (gl_PointCoord - 0.5) * v_size;  // 星の中心からのずれ [px]
    float d = length(q);
    float core = exp(-0.5 * d * d / (v_sigma * v_sigma));
    // スプライトの縁で 0 になるよう窓をかける（四角い切れ目が見えないように）
    float edge = 1.0 - smoothstep(0.3, 0.5, d / v_size);
    float halo = v_glow * 0.08 * exp(-d / (3.0 * v_sigma)) * edge;
    float spikes = 0.0;
    if (u_spikes > 0.0 && v_glow > 0.0) {
        float len = 0.15 * v_size;
        spikes = (exp(-abs(q.y) / 0.7 - abs(q.x) / len) + exp(-abs(q.x) / 0.7 - abs(q.y) / len)) * v_glow *
                 u_spikes * 0.3 * edge;
    }
    o_color = vec4(v_color * (core + halo + spikes), 1.0);
}
