#version 450 core
// HDR（線形）シーン -> 出力。露出・ガンマ補正と、暗部の段差を防ぐディザ

in vec2 v_ndc;

uniform sampler2D u_scene;
uniform float u_exposure;
uniform float u_gamma;

out vec4 o_color;

void main() {
    vec3 c = texture(u_scene, v_ndc * 0.5 + 0.5).rgb * u_exposure;
    c = pow(clamp(c, 0.0, 1.0), vec3(1.0 / u_gamma));
    // 8bit 出力で暗いグラデーションが縞にならないよう ±0.5 階調のノイズを加える
    float noise = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453) - 0.5;
    o_color = vec4(c + noise / 255.0, 1.0);
}
