#version 450 core

in vec2 v_uv;
in vec4 v_color;

uniform sampler2D u_atlas;  // 文字の形（1 チャンネル）

out vec4 o_color;

void main() {
    o_color = vec4(v_color.rgb, v_color.a * texture(u_atlas, v_uv).r);
}
