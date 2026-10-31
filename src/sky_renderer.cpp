#include "sky_renderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

#include "gl_util.hpp"

namespace {
// 太陽高度ごとの空の色（線形 sRGB）と最暗等級。間は線形補間する
struct SkyKey {
    float sunAlt;
    float zenith[3], horizon[3], sunGlow[3];
    float magLimit;
};
constexpr SkyKey kSkyKeys[] = {
    {-18.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 99.0f},  // 天文薄明の終わり = 夜
    {-12.0f, {0.001f, 0.0015f, 0.004f}, {0.004f, 0.004f, 0.007f}, {0.006f, 0.003f, 0.001f}, 4.5f},
    {-6.0f, {0.008f, 0.015f, 0.04f}, {0.05f, 0.035f, 0.03f}, {0.08f, 0.03f, 0.005f}, 2.0f},
    {0.0f, {0.04f, 0.08f, 0.20f}, {0.30f, 0.22f, 0.16f}, {0.40f, 0.15f, 0.03f}, -1.0f},
    {10.0f, {0.10f, 0.25f, 0.60f}, {0.45f, 0.55f, 0.70f}, {0.50f, 0.45f, 0.35f}, -4.0f},
};

// 方位の文字の頂点
struct CompassVertex {
    float az;
    float offset[2];
    float color[3];
};

// 文字の形を折れ線で定義（文字の高さ 1、中心原点、y 上向き）
using Polyline = std::vector<std::array<float, 2>>;
struct Glyph {
    float az;
    float color[3];
    std::vector<Polyline> strokes;
};

const std::vector<Glyph>& compassGlyphs() {
    static const float north[3] = {0.85f, 0.15f, 0.12f};
    static const float other[3] = {0.55f, 0.60f, 0.70f};
    static const std::vector<Glyph> glyphs = {
        {0.0f, {north[0], north[1], north[2]},
         {{{-0.3f, -0.5f}, {-0.3f, 0.5f}, {0.3f, -0.5f}, {0.3f, 0.5f}}}},
        {90.0f, {other[0], other[1], other[2]},
         {{{0.3f, 0.5f}, {-0.3f, 0.5f}, {-0.3f, -0.5f}, {0.3f, -0.5f}}, {{-0.3f, 0.0f}, {0.2f, 0.0f}}}},
        {180.0f, {other[0], other[1], other[2]},
         {{{0.3f, 0.38f}, {0.15f, 0.5f}, {-0.15f, 0.5f}, {-0.3f, 0.36f}, {-0.3f, 0.14f}, {-0.15f, 0.0f},
           {0.15f, 0.0f}, {0.3f, -0.14f}, {0.3f, -0.36f}, {0.15f, -0.5f}, {-0.15f, -0.5f}, {-0.3f, -0.38f}}}},
        {270.0f, {other[0], other[1], other[2]},
         {{{-0.4f, 0.5f}, {-0.2f, -0.5f}, {0.0f, 0.2f}, {0.2f, -0.5f}, {0.4f, 0.5f}}}},
    };
    return glyphs;
}

// 折れ線の各線分を、太さ thickness の長方形（三角形 2 枚）にする。
// 端を太さの半分だけ延ばして、折れ目に隙間ができないようにする
std::vector<CompassVertex> buildCompassMesh(float thickness) {
    std::vector<CompassVertex> out;
    for (const auto& g : compassGlyphs()) {
        for (const auto& line : g.strokes) {
            for (size_t i = 0; i + 1 < line.size(); ++i) {
                const float ax = line[i][0], ay = line[i][1], bx = line[i + 1][0], by = line[i + 1][1];
                const float len = std::hypot(bx - ax, by - ay);
                const float dx = (bx - ax) / len * thickness * 0.5f, dy = (by - ay) / len * thickness * 0.5f;
                const float nx = -dy, ny = dx;
                const float quad[4][2] = {{ax - dx + nx, ay - dy + ny},
                                          {ax - dx - nx, ay - dy - ny},
                                          {bx + dx + nx, by + dy + ny},
                                          {bx + dx - nx, by + dy - ny}};
                for (int k : {0, 1, 2, 2, 1, 3}) {
                    out.push_back({g.az, {quad[k][0], quad[k][1]}, {g.color[0], g.color[1], g.color[2]}});
                }
            }
        }
    }
    return out;
}

void setUniform(GLuint p, const char* name, float v) { glProgramUniform1f(p, glGetUniformLocation(p, name), v); }
void setUniform(GLuint p, const char* name, int v) { glProgramUniform1i(p, glGetUniformLocation(p, name), v); }
void setUniform3(GLuint p, const char* name, const float* v) {
    glProgramUniform3fv(p, glGetUniformLocation(p, name), 1, v);
}
void setUniformMat3(GLuint p, const char* name, const double m[3][3]) {
    float f[9];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) f[i * 3 + j] = static_cast<float>(m[i][j]);
    glProgramUniformMatrix3fv(p, glGetUniformLocation(p, name), 1, GL_TRUE, f);  // 行優先で渡す
}
}  // namespace

SkyLight skyLightForSun(double sunAltDeg, double nightMagLimit) {
    const float a = static_cast<float>(sunAltDeg);
    const size_t n = std::size(kSkyKeys);
    size_t i = 0;
    while (i + 2 < n && a > kSkyKeys[i + 1].sunAlt) ++i;
    const SkyKey& k0 = kSkyKeys[i];
    const SkyKey& k1 = kSkyKeys[i + 1];
    const float t = std::clamp((a - k0.sunAlt) / (k1.sunAlt - k0.sunAlt), 0.0f, 1.0f);
    SkyLight s{};
    for (int c = 0; c < 3; ++c) {
        s.zenith[c] = k0.zenith[c] + (k1.zenith[c] - k0.zenith[c]) * t;
        s.horizon[c] = k0.horizon[c] + (k1.horizon[c] - k0.horizon[c]) * t;
        s.sunGlow[c] = k0.sunGlow[c] + (k1.sunGlow[c] - k0.sunGlow[c]) * t;
    }
    // 夜の限界等級（99）との補間を避けるため、-18° の値は nightMagLimit とする
    const float m0 = i == 0 ? static_cast<float>(nightMagLimit) : k0.magLimit;
    s.magLimit = std::min(static_cast<float>(nightMagLimit), m0 + (k1.magLimit - m0) * t);
    return s;
}

SkyRenderer::SkyRenderer(const std::string& root, const std::vector<StarVertex>& stars, const RenderConfig& render)
    : render_(render) {
    const std::string dir = root + "/shaders/";
    const std::string common = dir + "projection.glsl";
    starProgram_ = buildProgram(dir + "stars.vert", dir + "stars.frag", common);
    skyProgram_ = buildProgram(dir + "fullscreen.vert", dir + "sky.frag", common);
    postProgram_ = buildProgram(dir + "fullscreen.vert", dir + "post.frag");
    compassProgram_ = buildProgram(dir + "compass.vert", dir + "compass.frag", common);
    createCompass();

    glCreateVertexArrays(1, &emptyVao_);
    glCreateVertexArrays(1, &starVao_);
    glCreateBuffers(1, &starVbo_);
    glNamedBufferStorage(starVbo_, stars.size() * sizeof(StarVertex), stars.data(), 0);
    starCount_ = static_cast<GLsizei>(stars.size());

    glVertexArrayVertexBuffer(starVao_, 0, starVbo_, 0, sizeof(StarVertex));
    const struct {
        GLuint loc;
        GLint size;
        size_t offset;
    } attribs[] = {{0, 3, offsetof(StarVertex, dir)}, {1, 1, offsetof(StarVertex, mag)},
                   {2, 3, offsetof(StarVertex, color)}};
    for (const auto& a : attribs) {
        glEnableVertexArrayAttrib(starVao_, a.loc);
        glVertexArrayAttribFormat(starVao_, a.loc, a.size, GL_FLOAT, GL_FALSE, static_cast<GLuint>(a.offset));
        glVertexArrayAttribBinding(starVao_, a.loc, 0);
    }
}

void SkyRenderer::createCompass() {
    const auto mesh = buildCompassMesh(0.13f);
    compassCount_ = static_cast<GLsizei>(mesh.size());
    glCreateVertexArrays(1, &compassVao_);
    glCreateBuffers(1, &compassVbo_);
    glNamedBufferStorage(compassVbo_, mesh.size() * sizeof(CompassVertex), mesh.data(), 0);
    glVertexArrayVertexBuffer(compassVao_, 0, compassVbo_, 0, sizeof(CompassVertex));
    const struct {
        GLuint loc;
        GLint size;
        size_t offset;
    } attribs[] = {{0, 1, offsetof(CompassVertex, az)}, {1, 2, offsetof(CompassVertex, offset)},
                   {2, 3, offsetof(CompassVertex, color)}};
    for (const auto& a : attribs) {
        glEnableVertexArrayAttrib(compassVao_, a.loc);
        glVertexArrayAttribFormat(compassVao_, a.loc, a.size, GL_FLOAT, GL_FALSE, static_cast<GLuint>(a.offset));
        glVertexArrayAttribBinding(compassVao_, a.loc, 0);
    }
}

SkyRenderer::~SkyRenderer() {
    glDeleteProgram(starProgram_);
    glDeleteProgram(skyProgram_);
    glDeleteProgram(postProgram_);
    glDeleteProgram(compassProgram_);
    glDeleteVertexArrays(1, &starVao_);
    glDeleteVertexArrays(1, &emptyVao_);
    glDeleteVertexArrays(1, &compassVao_);
    glDeleteBuffers(1, &starVbo_);
    glDeleteBuffers(1, &compassVbo_);
    glDeleteFramebuffers(1, &fbo_);
    glDeleteTextures(1, &sceneTex_);
}

void SkyRenderer::ensureTarget(int width, int height) {
    if (width == targetW_ && height == targetH_) return;
    glDeleteFramebuffers(1, &fbo_);
    glDeleteTextures(1, &sceneTex_);
    glCreateTextures(GL_TEXTURE_2D, 1, &sceneTex_);
    glTextureStorage2D(sceneTex_, 1, GL_RGBA16F, width, height);
    glTextureParameteri(sceneTex_, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(sceneTex_, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glCreateFramebuffers(1, &fbo_);
    glNamedFramebufferTexture(fbo_, GL_COLOR_ATTACHMENT0, sceneTex_, 0);
    targetW_ = width;
    targetH_ = height;
}

void SkyRenderer::setProjectionUniforms(GLuint prog, int width, int height, const ViewSettings& view) {
    // 視線方向 f と、そこから天頂側へ向かう上方向 u（地平座標: x = 北, y = 東, z = 天頂）
    const double alt = view.centerAltDeg * astro::kDeg, az = view.centerAzDeg * astro::kDeg;
    const double f[3] = {std::cos(alt) * std::cos(az), std::cos(alt) * std::sin(az), std::sin(alt)};
    const double u[3] = {-std::sin(alt) * std::cos(az), -std::sin(alt) * std::sin(az), std::cos(alt)};
    // 右 = u × f。天頂を見上げて南が下のとき、右 = 西（東が左の、見上げた星空の向き）
    const double r[3] = {u[1] * f[2] - u[2] * f[1], u[2] * f[0] - u[0] * f[2], u[0] * f[1] - u[1] * f[0]};
    const double viewMat[3][3] = {{r[0], r[1], r[2]}, {u[0], u[1], u[2]}, {f[0], f[1], f[2]}};
    setUniformMat3(prog, "u_view", viewMat);

    setUniform(prog, "u_projType", view.fisheye ? 0 : 1);
    setUniform(prog, "u_halfFov", static_cast<float>(view.fovDeg * 0.5 * astro::kDeg));

    const float aspect[2] = {width >= height ? float(height) / float(width) : 1.0f,
                             width >= height ? 1.0f : float(width) / float(height)};
    glProgramUniform2fv(prog, glGetUniformLocation(prog, "u_aspect"), 1, aspect);

    // 回転してから反転する
    const double rot = view.rotationDeg * astro::kDeg;
    const float sx = view.flipH ? -1.0f : 1.0f, sy = view.flipV ? -1.0f : 1.0f;
    const float c = static_cast<float>(std::cos(rot)), s = static_cast<float>(std::sin(rot));
    const float screen[4] = {sx * c, -sx * s, sy * s, sy * c};  // 行優先
    glProgramUniformMatrix2fv(prog, glGetUniformLocation(prog, "u_screen"), 1, GL_TRUE, screen);
}

void SkyRenderer::render(int width, int height, const ViewSettings& view, const SkyState& sky,
                         const ModeStyle& style, float realTimeSec, const Overlays& overlays) {
    if (width <= 0 || height <= 0) return;  // 最小化中
    ensureTarget(width, height);

    // リアルモードは観測地の空の暗さと薄明に従う。幻想モードは常に暗い夜空として描く
    SkyLight light = style.realisticSky ? skyLightForSun(sky.sunAltDeg, sky.nightMagLimit)
                                        : skyLightForSun(-90.0, render_.limiting_magnitude);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, width, height);
    glDisable(GL_BLEND);

    // 1) 空・地面・グリッド
    setProjectionUniforms(skyProgram_, width, height, view);
    const float sunDir[3] = {float(sky.sunHorizon.x), float(sky.sunHorizon.y), float(sky.sunHorizon.z)};
    const float ground[3] = {0.002f, 0.002f, 0.0015f};
    setUniform3(skyProgram_, "u_sunDir", sunDir);
    setUniform3(skyProgram_, "u_zenithColor", light.zenith);
    setUniform3(skyProgram_, "u_horizonColor", light.horizon);
    setUniform3(skyProgram_, "u_sunGlowColor", light.sunGlow);
    setUniform3(skyProgram_, "u_groundColor", ground);
    setUniform3(skyProgram_, "u_tint", style.tint);
    setUniform(skyProgram_, "u_showGrid", overlays.grid ? 1 : 0);
    glUseProgram(skyProgram_);
    glBindVertexArray(emptyVao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    // 2) 恒星（加算合成）
    setProjectionUniforms(starProgram_, width, height, view);
    setUniformMat3(starProgram_, "u_eqToHor", sky.j2000ToHorizon.m);
    setUniform(starProgram_, "u_magLimit", light.magLimit);
    setUniform(starProgram_, "u_magSat", static_cast<float>(render_.saturation_magnitude));
    setUniform(starProgram_, "u_brightness", static_cast<float>(render_.brightness) * style.brightness);
    // 星の大きさは画面の高さ 800px を基準に解像度へ比例させる
    const float pxScale = static_cast<float>(std::min(width, height)) / 800.0f;
    setUniform(starProgram_, "u_sigma", static_cast<float>(render_.star_size) * style.size * pxScale);
    setUniform(starProgram_, "u_extinction", style.realisticSky ? static_cast<float>(render_.extinction) : 0.0f);
    setUniform(starProgram_, "u_twinkle", style.twinkle);
    setUniform(starProgram_, "u_glow", style.glow);
    setUniform(starProgram_, "u_saturation", style.saturation);
    setUniform(starProgram_, "u_time", realTimeSec);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glUseProgram(starProgram_);
    glBindVertexArray(starVao_);
    glDrawArrays(GL_POINTS, 0, starCount_);
    glDisable(GL_BLEND);

    // 3) 方位の文字（地平線から 7° 上。魚眼では円の縁の内側に収まる）
    if (overlays.compass) {
        setProjectionUniforms(compassProgram_, width, height, view);
        setUniform(compassProgram_, "u_labelAlt", 7.0f);
        setUniform(compassProgram_, "u_glyphPx", 26.0f * pxScale);
        const float viewport[2] = {float(width), float(height)};
        glProgramUniform2fv(compassProgram_, glGetUniformLocation(compassProgram_, "u_viewport"), 1, viewport);
        glUseProgram(compassProgram_);
        glBindVertexArray(compassVao_);
        glDrawArrays(GL_TRIANGLES, 0, compassCount_);
    }

    // 4) 出力（露出・ガンマ）
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    setUniform(postProgram_, "u_exposure", static_cast<float>(render_.exposure));
    setUniform(postProgram_, "u_gamma", static_cast<float>(render_.gamma));
    setUniform(postProgram_, "u_scene", 0);
    glBindTextureUnit(0, sceneTex_);
    glUseProgram(postProgram_);
    glBindVertexArray(emptyVao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}
