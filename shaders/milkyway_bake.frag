#version 450 core
// 天の川を全天の画像（J2000 の赤経・赤緯の正距円筒図法, 2048×1024）に一度だけ描く（起動時）。
// 毎フレームは sky.frag がこの画像を引くだけにして、ノイズの計算（1 画素あたり 10 回以上）を省く。
// 天の川は星と一緒に回るだけで模様は変わらないので、J2000 座標で描いておけば使い回せる

in vec2 v_ndc;

out vec4 o_color;

// ---- ノイズ（値ノイズ + fBm）。sky.frag と同じ式 ----
float hash3(vec3 p) {
    p = fract(p * 0.3183099 + 0.1);
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float valueNoise(vec3 x) {
    vec3 i = floor(x), f = fract(x);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(hash3(i), hash3(i + vec3(1, 0, 0)), f.x),
                   mix(hash3(i + vec3(0, 1, 0)), hash3(i + vec3(1, 1, 0)), f.x), f.y),
               mix(mix(hash3(i + vec3(0, 0, 1)), hash3(i + vec3(1, 0, 1)), f.x),
                   mix(hash3(i + vec3(0, 1, 1)), hash3(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}

float fbm(vec3 p) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 5; ++i) {
        s += a * valueNoise(p);
        p *= 2.03;
        a *= 0.5;
    }
    return s;
}

// J2000 赤道 -> 銀河座標（IAU 1958 の定義による定数行列。GLSL は列優先なので j * M = 行列 × j）
const mat3 kJ2000ToGalactic = mat3(-0.0548755604, -0.8734370902, -0.4838350155,
                                   0.4941094279, -0.4448296300, 0.7469822445,
                                   -0.8676661490, -0.1980763734, 0.4559837762);

vec3 milkyWay(vec3 j) {
    vec3 g = j * kJ2000ToGalactic;
    float b = asin(clamp(g.z, -1.0, 1.0));  // 銀緯
    float l = atan(g.y, g.x);               // 銀経（0 = 銀河中心, いて座の方向）
    float toCenter = cos(l);
    float width = radians(7.0 + 6.0 * max(toCenter, 0.0));  // 中心方向ほど太い
    float band = exp(-pow(b / width, 2.0));
    float along = 0.35 + 0.65 * pow(0.5 + 0.5 * toCenter, 2.0);  // 中心方向ほど明るい
    float bulge = exp(-(pow(b / radians(9.0), 2.0) + pow(l / radians(16.0), 2.0)));
    float clouds = mix(0.3, 1.45, fbm(g * 5.0));  // 星雲のむら
    // 暗黒帯（はくちょう座からいて座にかけての「グレートリフト」）
    float rift = 1.0 - 0.75 * exp(-pow((b - radians(1.0) - radians(1.5) * sin(l * 2.0)) / radians(2.2), 2.0)) *
                           smoothstep(radians(110.0), radians(20.0), abs(l)) * smoothstep(0.3, 0.7, fbm(g * 9.0 + 3.0));
    float v = (band * along + 1.2 * bulge) * clouds * rift;
    vec3 color = mix(vec3(0.75, 0.82, 1.0), vec3(1.0, 0.88, 0.70), clamp(bulge * 1.5 + 0.3 * max(toCenter, 0.0), 0.0, 1.0));
    return color * v;
}

void main() {
    // 画像の横 = 赤経 0〜360°、縦 = 赤緯 -90〜+90°
    vec2 uv = v_ndc * 0.5 + 0.5;
    float ra = uv.x * 6.28318530718;
    float dec = (uv.y - 0.5) * 3.14159265359;
    vec3 j = vec3(cos(dec) * cos(ra), cos(dec) * sin(ra), sin(dec));
    o_color = vec4(milkyWay(j), 1.0);
}
