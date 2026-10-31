#version 450 core
// 操作パネルのプレビュー: プロジェクタに出している画像をそのまま縮小して表示する

in vec2 v_ndc;

uniform sampler2D u_image;

out vec4 o_color;

void main() {
    o_color = vec4(texture(u_image, v_ndc * 0.5 + 0.5).rgb, 1.0);
}
