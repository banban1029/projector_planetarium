#version 450 core
// 月・太陽の円盤。球の法線と太陽の向きの内積で満ち欠けを付ける。加算合成

in vec3 v_color;
in vec2 v_lightDir;
in float v_phase;
in float v_radiusPx;
in float v_size;
in float v_glow;

out vec4 o_color;

float hash2(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }

float valueNoise(vec2 x) {
    vec2 i = floor(x), f = fract(x);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash2(i), hash2(i + vec2(1, 0)), f.x), mix(hash2(i + vec2(0, 1)), hash2(i + vec2(1, 1)), f.x), f.y);
}

void main() {
    vec2 q = (gl_PointCoord - 0.5) * vec2(1.0, -1.0) * v_size;  // 中心からのずれ [px]（y 上向き）
    float r = length(q) / v_radiusPx;                              // 円盤の半径 = 1
    float aa = 1.0 / v_radiusPx;
    float disk = 1.0 - smoothstep(1.0 - aa, 1.0 + aa, r);

    // 円盤上の点の球面の法線（z = 観測者の方向）と、太陽の方向
    vec3 n = vec3(q / v_radiusPx, sqrt(max(1.0 - r * r, 0.0)));
    vec3 sun = vec3(v_lightDir * sin(v_phase), cos(v_phase));
    float lit = smoothstep(-0.05, 0.05, dot(n, sun));
    // 月の「海」の模様（太陽では弱い）
    float maria = 1.0 - 0.3 * smoothstep(0.45, 0.75, valueNoise(n.xy * 3.0 + 7.0)) * step(v_glow, 0.5);
    vec3 c = v_color * (lit * maria + 0.02) * disk;  // 0.02 = 地球照

    float edge = 1.0 - smoothstep(0.35, 0.5, length(q) / v_size);
    float halo = v_glow * 0.25 * exp(-max(r - 1.0, 0.0) * 1.2) * (1.0 - disk) * edge;
    o_color = vec4(c + v_color * halo, 1.0);
}
