#version 450 core
// 光のにじみ（ブルーム）。明るい部分を取り出して、縮小したバッファでぼかす

in vec2 v_ndc;

uniform sampler2D u_src;
uniform int u_pass;         // 0 = 明るい部分の抽出, 1 = ぼかし
uniform vec2 u_texel;       // ぼかし: 1 回ずらす量（テクスチャ座標）。抽出: 元画像の 1 画素
uniform float u_threshold;  // これより明るい分だけがにじむ

out vec4 o_color;

void main() {
    vec2 uv = v_ndc * 0.5 + 0.5;
    if (u_pass == 0) {
        // 2x2 画素の平均で縮小（1px の星も取りこぼさない）
        vec3 c = 0.25 * (texture(u_src, uv + u_texel * vec2(-0.5, -0.5)).rgb + texture(u_src, uv + u_texel * vec2(0.5, -0.5)).rgb +
                         texture(u_src, uv + u_texel * vec2(-0.5, 0.5)).rgb + texture(u_src, uv + u_texel * vec2(0.5, 0.5)).rgb);
        o_color = vec4(max(c - u_threshold, 0.0), 1.0);
        return;
    }
    // 9 タップのガウスぼかし（片方向）
    const float w[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
    vec3 c = texture(u_src, uv).rgb * w[0];
    for (int i = 1; i < 5; ++i) {
        c += texture(u_src, uv + u_texel * float(i)).rgb * w[i];
        c += texture(u_src, uv - u_texel * float(i)).rgb * w[i];
    }
    o_color = vec4(c, 1.0);
}
