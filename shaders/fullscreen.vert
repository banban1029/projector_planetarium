#version 450 core
// 画面全体を覆う 1 枚の三角形（頂点バッファ不要）

out vec2 v_ndc;

void main() {
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2) * 2.0 - 1.0;
    v_ndc = p;
    gl_Position = vec4(p, 0.0, 1.0);
}
