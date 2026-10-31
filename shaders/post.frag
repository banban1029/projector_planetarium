#version 450 core
// HDR（線形）シーン -> 出力。水鏡の映り込み、光のにじみ、場面切り替えのフェード、露出・ガンマ補正、ディザ

in vec2 v_ndc;

uniform sampler2D u_scene;
uniform sampler2D u_bloom;
uniform sampler2D u_fade;   // 切り替え前の場面
uniform float u_bloomStrength;
uniform float u_fadeT;      // 0 = 切り替え前の場面, 1 = 今の場面（フェードなし）
uniform bool u_mirror;      // 水鏡モード
uniform float u_time;
uniform float u_exposure;
uniform float u_gamma;
uniform float u_whiteLevel;  // 出力の最大値（プロジェクタの白とび対策）
uniform bool u_dither;

out vec4 o_color;

void main() {
    vec2 uv = v_ndc * 0.5 + 0.5;
    vec3 c = texture(u_scene, uv).rgb;

    if (u_mirror) {
        // 地平線より下の画素には、地平線で折り返した方向の空を映す。遠く（地平線近く）ほど波が細かい
        vec3 h;
        if (unprojectNdc(v_ndc, h)) {
            if (h.z < 0.0) {
                float depth = max(-h.z, 0.02);
                vec3 r = vec3(h.xy, -h.z + 0.004 * sin(3.0 / depth - u_time * 2.0) * (0.3 + depth));
                vec3 p = projectHorizon(normalize(r));
                c = p.z > 0.5 ? texture(u_scene, p.xy * 0.5 + 0.5).rgb * vec3(0.35, 0.45, 0.65) : vec3(0.0);
                c += vec3(0.0, 0.004, 0.010);  // 水面そのものの色
            }
            c += vec3(0.02, 0.035, 0.06) * exp(-abs(h.z) * 150.0);  // 水平線の光の筋
        }
    }
    c += texture(u_bloom, uv).rgb * u_bloomStrength;
    if (u_fadeT < 1.0) c = mix(texture(u_fade, uv).rgb, c, smoothstep(0.0, 1.0, u_fadeT));

    c = pow(clamp(c * u_exposure, 0.0, 1.0), vec3(1.0 / u_gamma)) * u_whiteLevel;
    // 8bit 出力で暗いグラデーションが縞にならないよう ±0.5 階調のノイズを加える
    float noise = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453) - 0.5;
    o_color = vec4(c + (u_dither ? noise / 255.0 : 0.0), 1.0);
}
