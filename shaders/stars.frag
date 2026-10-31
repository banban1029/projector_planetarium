#version 450 core
// 星像 = ガウス分布の芯 + 明るい星だけに付く指数関数的な光芒。加算合成で重ねる

in vec3 v_color;
in float v_sigma;
in float v_glow;
in float v_size;

out vec4 o_color;

void main() {
    float d = length(gl_PointCoord - 0.5) * v_size;  // 星の中心からの距離 [px]
    float core = exp(-0.5 * d * d / (v_sigma * v_sigma));
    // スプライトの縁で 0 になるよう窓をかける（四角い切れ目が見えないように）
    float edge = 1.0 - smoothstep(0.3, 0.5, d / v_size);
    float halo = v_glow * 0.08 * exp(-d / (3.0 * v_sigma)) * edge;
    o_color = vec4(v_color * (core + halo), 1.0);
}
