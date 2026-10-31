#version 450 core
// 線（星座線・流れ星・ワープの光跡）の頂点。方向を投影し、景色より下なら無効にする

layout(location = 0) in vec3 a_dir;
layout(location = 1) in float a_anchor;  // 0 = J2000 赤道座標, 1 = 地平座標
layout(location = 2) in vec4 a_color;

uniform mat3 u_eqToHor;

out vec4 vg_color;
out float vg_valid;

void main() {
    vec3 h = a_anchor < 0.5 ? u_eqToHor * a_dir : a_dir;
    vec3 p = projectHorizon(h);
    vg_valid = (p.z > 0.5 && aboveGround(h, 0.0)) ? 1.0 : 0.0;
    vg_color = a_color;
    gl_Position = vec4(p.xy, 0.0, 1.0);
}
