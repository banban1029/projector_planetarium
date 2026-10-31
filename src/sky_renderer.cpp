#include "sky_renderer.hpp"

#include <algorithm>
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

// 月・太陽の頂点
struct BodyVertex {
    float dir[3];
    float radiusDeg;
    float phaseAngle;
    float color[3];
    float glow;
};

struct Attrib {
    GLuint loc;
    GLint size;
    size_t offset;
};

void setupVao(GLuint vao, GLuint vbo, GLsizei stride, std::initializer_list<Attrib> attribs) {
    glVertexArrayVertexBuffer(vao, 0, vbo, 0, stride);
    for (const auto& a : attribs) {
        glEnableVertexArrayAttrib(vao, a.loc);
        glVertexArrayAttribFormat(vao, a.loc, a.size, GL_FLOAT, GL_FALSE, static_cast<GLuint>(a.offset));
        glVertexArrayAttribBinding(vao, a.loc, 0);
    }
}

void setStarAttribs(GLuint vao, GLuint vbo) {
    setupVao(vao, vbo, sizeof(StarVertex),
             {{0, 3, offsetof(StarVertex, dir)}, {1, 1, offsetof(StarVertex, mag)}, {2, 3, offsetof(StarVertex, color)}});
}

void setUniform(GLuint p, const char* name, float v) { glProgramUniform1f(p, glGetUniformLocation(p, name), v); }
void setUniform(GLuint p, const char* name, int v) { glProgramUniform1i(p, glGetUniformLocation(p, name), v); }
void setUniform2(GLuint p, const char* name, float x, float y) {
    glProgramUniform2f(p, glGetUniformLocation(p, name), x, y);
}
void setUniform3(GLuint p, const char* name, const float* v) {
    glProgramUniform3fv(p, glGetUniformLocation(p, name), 1, v);
}
void setUniformMat3(GLuint p, const char* name, const double m[3][3]) {
    float f[9];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) f[i * 3 + j] = static_cast<float>(m[i][j]);
    glProgramUniformMatrix3fv(p, glGetUniformLocation(p, name), 1, GL_TRUE, f);  // 行優先で渡す
}

GLuint makeTexture(int w, int h) {
    GLuint tex;
    glCreateTextures(GL_TEXTURE_2D, 1, &tex);
    glTextureStorage2D(tex, 1, GL_RGBA16F, w, h);
    glTextureParameteri(tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return tex;
}

GLuint makeFbo(GLuint tex) {
    GLuint fbo;
    glCreateFramebuffers(1, &fbo);
    glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, tex, 0);
    return fbo;
}

float smoothstep(float e0, float e1, float x) {
    const float t = std::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
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

void addMoonlight(SkyLight& light, double moonAltDeg, double moonPhaseFraction) {
    // 満月が高く昇ると空が青白く明るくなり、見える星が 2 等級ほど減る
    const float m = static_cast<float>(std::pow(moonPhaseFraction, 1.5)) *
                    smoothstep(-2.0f, 10.0f, static_cast<float>(moonAltDeg));
    const float zenith[3] = {0.010f, 0.016f, 0.030f}, horizon[3] = {0.012f, 0.016f, 0.025f};
    for (int c = 0; c < 3; ++c) {
        light.zenith[c] += zenith[c] * m;
        light.horizon[c] += horizon[c] * m;
    }
    light.magLimit = std::min(light.magLimit, light.magLimit - 2.2f * m);
}

SkyRenderer::SkyRenderer(const std::string& root, const std::vector<StarVertex>& stars,
                         const ConstellationData& constellations, const RenderConfig& render)
    : render_(render) {
    const std::string dir = root + "/shaders/";
    const std::string common = dir + "projection.glsl";
    starProgram_ = buildProgram(dir + "stars.vert", dir + "stars.frag", common);
    skyProgram_ = buildProgram(dir + "fullscreen.vert", dir + "sky.frag", common);
    bodyProgram_ = buildProgram(dir + "body.vert", dir + "body.frag", common);
    bloomProgram_ = buildProgram(dir + "fullscreen.vert", dir + "bloom.frag");
    postProgram_ = buildProgram(dir + "fullscreen.vert", dir + "post.frag", common);
    testPatternProgram_ = buildProgram(dir + "fullscreen.vert", dir + "testpattern.frag", common);
    text_ = std::make_unique<TextRenderer>(root, render.font);
    lines_ = std::make_unique<LineRenderer>(root);

    glCreateVertexArrays(1, &emptyVao_);
    glCreateVertexArrays(1, &starVao_);
    glCreateBuffers(1, &starVbo_);
    glNamedBufferStorage(starVbo_, stars.size() * sizeof(StarVertex), stars.data(), 0);
    starCount_ = static_cast<GLsizei>(stars.size());
    setStarAttribs(starVao_, starVbo_);

    glCreateVertexArrays(1, &planetVao_);
    glCreateBuffers(1, &planetVbo_);
    setStarAttribs(planetVao_, planetVbo_);

    glCreateVertexArrays(1, &bodyVao_);
    glCreateBuffers(1, &bodyVbo_);
    setupVao(bodyVao_, bodyVbo_, sizeof(BodyVertex),
             {{0, 3, offsetof(BodyVertex, dir)},
              {1, 1, offsetof(BodyVertex, radiusDeg)},
              {2, 1, offsetof(BodyVertex, phaseAngle)},
              {3, 3, offsetof(BodyVertex, color)},
              {4, 1, offsetof(BodyVertex, glow)}});

    // 星座線は大円に沿って 1.5° ごとに分割しておく（魚眼で正しく曲がるように）
    std::vector<LineVertex> lineVerts;
    const float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    for (const auto& [a, b] : constellations.segments) appendArc(lineVerts, a, b, 0.0f, white, white);
    lines_->setStatic(lineVerts);
}

SkyRenderer::~SkyRenderer() {
    for (GLuint p : {starProgram_, skyProgram_, bodyProgram_, bloomProgram_, postProgram_, testPatternProgram_}) {
        glDeleteProgram(p);
    }
    for (GLuint v : {starVao_, planetVao_, bodyVao_, emptyVao_}) glDeleteVertexArrays(1, &v);
    for (GLuint b : {starVbo_, planetVbo_, bodyVbo_}) glDeleteBuffers(1, &b);
    for (GLuint f : {sceneFbo_, outputFbo_, bloomFbo_[0], bloomFbo_[1]}) glDeleteFramebuffers(1, &f);
    for (GLuint t : {sceneTex_, fadeTex_, outputTex_, bloomTex_[0], bloomTex_[1]}) glDeleteTextures(1, &t);
}

void SkyRenderer::ensureTargets(int width, int height) {
    if (width == targetW_ && height == targetH_) return;
    for (GLuint f : {sceneFbo_, outputFbo_, bloomFbo_[0], bloomFbo_[1]}) glDeleteFramebuffers(1, &f);
    for (GLuint t : {sceneTex_, fadeTex_, outputTex_, bloomTex_[0], bloomTex_[1]}) glDeleteTextures(1, &t);
    sceneTex_ = makeTexture(width, height);
    glCreateTextures(GL_TEXTURE_2D, 1, &outputTex_);
    glTextureStorage2D(outputTex_, 1, GL_RGBA8, width, height);
    glTextureParameteri(outputTex_, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(outputTex_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    outputFbo_ = makeFbo(outputTex_);
    fadeTex_ = makeTexture(width, height);
    sceneFbo_ = makeFbo(sceneTex_);
    for (int i = 0; i < 2; ++i) {
        bloomTex_[i] = makeTexture(std::max(width / 2, 1), std::max(height / 2, 1));
        bloomFbo_[i] = makeFbo(bloomTex_[i]);
    }
    targetW_ = width;
    targetH_ = height;
    hasFrame_ = false;
    fadeStart_ = -1.0f;
}

void SkyRenderer::setProjectionUniforms(GLuint prog, int width, int height, const ViewSettings& view, int landscape) {
    const Projection p = makeProjection(view, width, height, landscape);
    setUniformMat3(prog, "u_view", p.view);
    setUniform(prog, "u_projType", p.fisheye ? 0 : 1);
    setUniform(prog, "u_halfFov", static_cast<float>(p.halfFovRad));
    setUniform2(prog, "u_aspect", float(p.aspect[0]), float(p.aspect[1]));
    setUniform(prog, "u_landscape", landscape);
    const float screen[4] = {float(p.screen[0][0]), float(p.screen[0][1]), float(p.screen[1][0]),
                             float(p.screen[1][1])};  // 行優先
    glProgramUniformMatrix2fv(prog, glGetUniformLocation(prog, "u_screen"), 1, GL_TRUE, screen);
}

void SkyRenderer::drawFullscreen(GLuint prog) {
    glUseProgram(prog);
    glBindVertexArray(emptyVao_);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void SkyRenderer::startTransition(float nowSec, float durationSec) {
    if (!hasFrame_) return;
    // sceneTex_ には直前のフレームの画面が残っている
    glCopyImageSubData(sceneTex_, GL_TEXTURE_2D, 0, 0, 0, 0, fadeTex_, GL_TEXTURE_2D, 0, 0, 0, 0, targetW_, targetH_, 1);
    fadeStart_ = nowSec;
    fadeDuration_ = durationSec;
}

void SkyRenderer::render(const FrameInput& in) {
    const int width = in.width, height = in.height;
    if (width <= 0 || height <= 0) return;  // 最小化中
    ensureTargets(width, height);

    const ModeStyle& st = in.style;
    const SkyState& sky = in.sky;
    ViewSettings view = in.view;
    int landscape = in.landscape;
    if (st.mirror) applyMirror(view, landscape);

    // リアルモードは観測地の空の暗さ・薄明・月明かりに従う。幻想モードは常に暗い夜空として描く
    SkyLight light = st.realisticSky ? skyLightForSun(sky.sunAltDeg, sky.nightMagLimit)
                                     : skyLightForSun(-90.0, render_.limiting_magnitude);
    if (st.realisticSky) addMoonlight(light, sky.moonAltDeg, sky.moonPhaseFraction);
    const float lightPollution =
        st.realisticSky ? std::clamp(static_cast<float>(6.5 - sky.nightMagLimit) / 3.0f, 0.0f, 1.0f) : 0.0f;
    // 天の川は空が暗い（限界等級 4〜6.5 等）ほど見える
    const float milkyWay = st.milkyWay * (st.realisticSky ? smoothstep(4.0f, 6.5f, light.magLimit) : 1.0f);
    const float pxScale = static_cast<float>(std::min(width, height)) / 800.0f;

    glBindFramebuffer(GL_FRAMEBUFFER, sceneFbo_);
    glViewport(0, 0, width, height);
    glDisable(GL_BLEND);
    glEnable(GL_PROGRAM_POINT_SIZE);

    // 1) 空・天の川・演出・地上の景色・グリッド
    setProjectionUniforms(skyProgram_, width, height, view, landscape);
    const float sunDir[3] = {float(sky.sunHorizon.x), float(sky.sunHorizon.y), float(sky.sunHorizon.z)};
    const float ground[3] = {0.002f, 0.002f, 0.0015f};
    setUniform3(skyProgram_, "u_sunDir", sunDir);
    setUniform3(skyProgram_, "u_zenithColor", light.zenith);
    setUniform3(skyProgram_, "u_horizonColor", light.horizon);
    setUniform3(skyProgram_, "u_sunGlowColor", light.sunGlow);
    setUniform3(skyProgram_, "u_groundColor", ground);
    setUniform3(skyProgram_, "u_tint", st.tint);
    setUniformMat3(skyProgram_, "u_eqToHor", sky.j2000ToHorizon.m);
    setUniform(skyProgram_, "u_milkyWay", milkyWay);
    setUniform(skyProgram_, "u_lightPollution", lightPollution);
    setUniform(skyProgram_, "u_aurora", st.aurora);
    setUniform(skyProgram_, "u_nebula", st.nebula);
    setUniform(skyProgram_, "u_time", in.timeSec);
    setUniform(skyProgram_, "u_showGrid", in.overlays.grid ? 1 : 0);
    drawFullscreen(skyProgram_);

    // 2) 星座線
    const GLuint lineProg = lines_->program();
    setProjectionUniforms(lineProg, width, height, view, landscape);
    setUniformMat3(lineProg, "u_eqToHor", sky.j2000ToHorizon.m);
    if (in.overlays.constellations || st.constellationArt) {
        if (st.constellationArt) {
            const float gold[3] = {0.9f, 0.65f, 0.25f};
            lines_->drawStatic(width, height, 2.2f * pxScale, 0.75f + 0.25f * std::sin(in.timeSec * 1.5f), gold);
        } else {
            const float blue[3] = {0.16f, 0.26f, 0.42f};
            lines_->drawStatic(width, height, 1.4f * pxScale, 1.0f, blue);
        }
    }

    // 3) 恒星と惑星（加算合成）
    setProjectionUniforms(starProgram_, width, height, view, landscape);
    setUniformMat3(starProgram_, "u_eqToHor", sky.j2000ToHorizon.m);
    setUniform(starProgram_, "u_magLimit", light.magLimit);
    setUniform(starProgram_, "u_magSat", static_cast<float>(render_.saturation_magnitude));
    setUniform(starProgram_, "u_brightness", static_cast<float>(render_.brightness) * st.brightness);
    // 星の大きさは画面の高さ 800px を基準に解像度へ比例させる
    setUniform(starProgram_, "u_sigma", static_cast<float>(render_.star_size) * st.size * pxScale);
    setUniform(starProgram_, "u_extinction", st.realisticSky ? static_cast<float>(render_.extinction) : 0.0f);
    setUniform(starProgram_, "u_twinkle", st.twinkle);
    setUniform(starProgram_, "u_glow", st.glow);
    setUniform(starProgram_, "u_saturation", st.saturation);
    setUniform(starProgram_, "u_spikes", st.spikes);
    setUniform(starProgram_, "u_time", in.timeSec);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glUseProgram(starProgram_);
    glBindVertexArray(starVao_);
    glDrawArrays(GL_POINTS, 0, starCount_);

    std::vector<StarVertex> planets;
    std::vector<BodyVertex> discs;
    const double moonScale = render_.moon_scale;
    static const std::vector<BodyInfo> kNoBodies;
    for (const auto& b : in.bodies ? *in.bodies : kNoBodies) {
        const float dir[3] = {float(b.dirJ2000.x), float(b.dirJ2000.y), float(b.dirJ2000.z)};
        if (b.kind == BodyInfo::Planet) {
            planets.push_back({{dir[0], dir[1], dir[2]}, float(b.mag), {b.color[0], b.color[1], b.color[2]}});
        } else if (b.kind == BodyInfo::Moon) {
            // 円盤の明るさは一定で、満ち欠けは形で表す。周りのにじみは輝面比に比例
            const float k = 1.2f;
            discs.push_back({{dir[0], dir[1], dir[2]}, float(b.angularRadiusDeg * moonScale),
                             float(b.phaseAngleDeg * astro::kDeg), {b.color[0] * k, b.color[1] * k, b.color[2] * k},
                             float(0.4 * b.phaseFraction)});
        } else if (st.realisticSky) {  // 太陽（幻想モードは常に夜なので描かない）
            const float k = 4.0f;
            discs.push_back({{dir[0], dir[1], dir[2]}, float(b.angularRadiusDeg * moonScale), 0.0f,
                             {b.color[0] * k, b.color[1] * k, b.color[2] * k}, 1.0f});
        }
    }
    if (!planets.empty()) {
        glNamedBufferData(planetVbo_, static_cast<GLsizeiptr>(planets.size() * sizeof(StarVertex)), planets.data(),
                          GL_STREAM_DRAW);
        glBindVertexArray(planetVao_);
        glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(planets.size()));
    }

    // 4) 月・太陽
    if (!discs.empty()) {
        setProjectionUniforms(bodyProgram_, width, height, view, landscape);
        setUniformMat3(bodyProgram_, "u_eqToHor", sky.j2000ToHorizon.m);
        setUniform3(bodyProgram_, "u_sunDir", sunDir);
        setUniform2(bodyProgram_, "u_viewport", float(width), float(height));
        glNamedBufferData(bodyVbo_, static_cast<GLsizeiptr>(discs.size() * sizeof(BodyVertex)), discs.data(),
                          GL_STREAM_DRAW);
        glUseProgram(bodyProgram_);
        glBindVertexArray(bodyVao_);
        glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(discs.size()));
    }
    glDisable(GL_BLEND);

    // 5) 流れ星・ワープの光跡
    if (in.effectLines) lines_->drawDynamic(*in.effectLines, width, height, 2.5f * pxScale, 1.0f);

    // 6) 文字
    if (text_->ok()) {
        const GLuint tp = text_->program();
        setProjectionUniforms(tp, width, height, view, landscape);
        setUniformMat3(tp, "u_eqToHor", sky.j2000ToHorizon.m);
        text_->flush(width, height, TextRenderer::Sky);
    }

    // 7) 光のにじみ: 明るい部分を半分の解像度に取り出し、横・縦のぼかしを 2 回ずつ
    const float bloom = st.bloom * static_cast<float>(render_.bloom);
    const int bw = std::max(width / 2, 1), bh = std::max(height / 2, 1);
    glViewport(0, 0, bw, bh);
    glBindFramebuffer(GL_FRAMEBUFFER, bloomFbo_[0]);
    setUniform(bloomProgram_, "u_src", 0);
    setUniform(bloomProgram_, "u_pass", 0);
    setUniform(bloomProgram_, "u_threshold", 0.5f);
    setUniform2(bloomProgram_, "u_texel", 1.0f / width, 1.0f / height);
    glBindTextureUnit(0, sceneTex_);
    drawFullscreen(bloomProgram_);
    setUniform(bloomProgram_, "u_pass", 1);
    for (int i = 0; i < 2; ++i) {
        const float spread = 1.0f + 1.5f * i;
        glBindFramebuffer(GL_FRAMEBUFFER, bloomFbo_[1]);
        setUniform2(bloomProgram_, "u_texel", spread / bw, 0.0f);
        glBindTextureUnit(0, bloomTex_[0]);
        drawFullscreen(bloomProgram_);
        glBindFramebuffer(GL_FRAMEBUFFER, bloomFbo_[0]);
        setUniform2(bloomProgram_, "u_texel", 0.0f, spread / bh);
        glBindTextureUnit(0, bloomTex_[1]);
        drawFullscreen(bloomProgram_);
    }

    // 8) 仕上げ（水鏡・にじみ・フェード・露出・ガンマ）→ 出力テクスチャ
    glBindFramebuffer(GL_FRAMEBUFFER, outputFbo_);
    glViewport(0, 0, width, height);
    setProjectionUniforms(postProgram_, width, height, view, landscape);
    const float fadeT =
        fadeStart_ < 0.0f ? 1.0f : std::clamp((in.timeSec - fadeStart_) / fadeDuration_, 0.0f, 1.0f);
    setUniform(postProgram_, "u_scene", 0);
    setUniform(postProgram_, "u_bloom", 1);
    setUniform(postProgram_, "u_fade", 2);
    setUniform(postProgram_, "u_bloomStrength", bloom);
    setUniform(postProgram_, "u_fadeT", fadeT);
    setUniform(postProgram_, "u_mirror", st.mirror ? 1 : 0);
    setUniform(postProgram_, "u_time", in.timeSec);
    // クレジット表示中は星空を暗くして文字を読みやすくする
    setUniform(postProgram_, "u_exposure",
               static_cast<float>(render_.exposure) * (in.overlays.credits ? 0.25f : 1.0f));
    setUniform(postProgram_, "u_gamma", static_cast<float>(render_.gamma));
    setUniform(postProgram_, "u_dither", render_.dither ? 1 : 0);
    glBindTextureUnit(0, sceneTex_);
    glBindTextureUnit(1, bloomTex_[0]);
    glBindTextureUnit(2, fadeTex_);
    drawFullscreen(postProgram_);

    // 9) 仕上げの後に重ねる文字（画面表示・通知・字幕・クレジット）
    if (text_->ok()) text_->flush(width, height, TextRenderer::Overlay);

    // 10) テストパターン（投影の調整用。ガンマなどをかけず、出力の値をそのまま出す）
    if (in.overlays.testPattern) {
        setProjectionUniforms(testPatternProgram_, width, height, view, 0);
        setUniform2(testPatternProgram_, "u_viewport", float(width), float(height));
        drawFullscreen(testPatternProgram_);
    }

    // 11) 画面（プロジェクタ）へ
    glBlitNamedFramebuffer(outputFbo_, 0, 0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    hasFrame_ = true;
}
