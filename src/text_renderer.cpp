#include "text_renderer.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <iterator>

#include "gl_util.hpp"
#include "stb_truetype.h"

namespace {
constexpr int kAtlasSize = 2048;
constexpr float kBakePx = 48.0f;  // 焼き込む文字の大きさ。描くときはこれを縮小する

const char* const kFallbackFonts[] = {
    "C:/Windows/Fonts/BIZ-UDGothicR.ttc",  // UD（ユニバーサルデザイン）フォント。遠くからでも読みやすい
    "C:/Windows/Fonts/YuGothM.ttc",
    "C:/Windows/Fonts/meiryo.ttc",
    "C:/Windows/Fonts/msgothic.ttc",
};

std::vector<unsigned char> readBinary(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
}  // namespace

std::vector<int> decodeUtf8(const std::string& s) {
    std::vector<int> out;
    for (size_t i = 0; i < s.size();) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        int cp, len;
        if (c < 0x80) cp = c, len = 1;
        else if ((c >> 5) == 0x6) cp = c & 0x1F, len = 2;
        else if ((c >> 4) == 0xE) cp = c & 0x0F, len = 3;
        else cp = c & 0x07, len = 4;
        for (int k = 1; k < len && i + k < s.size(); ++k) cp = (cp << 6) | (s[i + k] & 0x3F);
        out.push_back(cp);
        i += len;
    }
    return out;
}

TextRenderer::TextRenderer(const std::string& root, const std::string& fontPath) {
    if (!fontPath.empty()) {
        fontData_ = readBinary(fontPath);
        fontPath_ = fontPath;
    }
    for (const char* f : kFallbackFonts) {
        if (!fontData_.empty()) break;
        fontData_ = readBinary(f);
        fontPath_ = f;
    }
    if (fontData_.empty()) {
        std::fprintf(stderr, "no Japanese font found; text is disabled\n");
        return;
    }
    std::printf("font: %s\n", fontPath_.c_str());

    const std::string dir = root + "/shaders/";
    program_ = buildProgram(dir + "text.vert", dir + "text.frag", dir + "projection.glsl");
    glCreateVertexArrays(1, &vao_);
    glCreateBuffers(1, &vbo_);
    glVertexArrayVertexBuffer(vao_, 0, vbo_, 0, sizeof(Vertex));
    const struct {
        GLuint loc;
        GLint size;
        size_t offset;
    } attribs[] = {{0, 3, offsetof(Vertex, pos)},
                   {1, 1, offsetof(Vertex, anchor)},
                   {2, 2, offsetof(Vertex, offset)},
                   {3, 2, offsetof(Vertex, uv)},
                   {4, 4, offsetof(Vertex, color)}};
    for (const auto& a : attribs) {
        glEnableVertexArrayAttrib(vao_, a.loc);
        glVertexArrayAttribFormat(vao_, a.loc, a.size, GL_FLOAT, GL_FALSE, static_cast<GLuint>(a.offset));
        glVertexArrayAttribBinding(vao_, a.loc, 0);
    }
    // 英数字・記号は常に入れる
    for (int c = 0x20; c < 0x7F; ++c) codepoints_.push_back(c);
    loaded_ = true;
}

TextRenderer::~TextRenderer() {
    if (program_) glDeleteProgram(program_);
    glDeleteTextures(1, &atlas_);
    glDeleteVertexArrays(1, &vao_);
    glDeleteBuffers(1, &vbo_);
}

void TextRenderer::addCharacters(const std::string& utf8) {
    const auto cps = decodeUtf8(utf8);
    codepoints_.insert(codepoints_.end(), cps.begin(), cps.end());
}

void TextRenderer::build() {
    if (!ok()) return;
    std::sort(codepoints_.begin(), codepoints_.end());
    codepoints_.erase(std::unique(codepoints_.begin(), codepoints_.end()), codepoints_.end());

    std::vector<unsigned char> pixels(kAtlasSize * kAtlasSize);
    std::vector<stbtt_packedchar> packed(codepoints_.size());
    stbtt_pack_context pc;
    stbtt_PackBegin(&pc, pixels.data(), kAtlasSize, kAtlasSize, 0, 2, nullptr);
    stbtt_PackSetOversampling(&pc, 2, 2);
    stbtt_pack_range range{};
    range.font_size = kBakePx;
    range.array_of_unicode_codepoints = codepoints_.data();
    range.num_chars = static_cast<int>(codepoints_.size());
    range.chardata_for_range = packed.data();
    // 第 3 引数は書体の番号（.ttc は複数の書体を含む。0 = 最初の書体）
    if (!stbtt_PackFontRanges(&pc, fontData_.data(), 0, &range, 1)) {
        std::fprintf(stderr, "font atlas is full; some characters are missing\n");
    }
    stbtt_PackEnd(&pc);

    for (size_t i = 0; i < codepoints_.size(); ++i) {
        const auto& p = packed[i];
        float x = 0, y = 0;
        stbtt_aligned_quad q;
        stbtt_GetPackedQuad(packed.data(), kAtlasSize, kAtlasSize, static_cast<int>(i), &x, &y, &q, 0);
        // stb は y 下向きなので反転して em 単位にする
        glyphs_[codepoints_[i]] = {q.x0 / kBakePx, -q.y1 / kBakePx, q.x1 / kBakePx, -q.y0 / kBakePx,
                                   q.s0,         q.t1,           q.s1,         q.t0,
                                   p.xadvance / kBakePx};
    }

    glDeleteTextures(1, &atlas_);
    glCreateTextures(GL_TEXTURE_2D, 1, &atlas_);
    glTextureStorage2D(atlas_, 6, GL_R8, kAtlasSize, kAtlasSize);
    glTextureSubImage2D(atlas_, 0, 0, 0, kAtlasSize, kAtlasSize, GL_RED, GL_UNSIGNED_BYTE, pixels.data());
    glGenerateTextureMipmap(atlas_);
    glTextureParameteri(atlas_, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(atlas_, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    needsBuild_ = false;
    std::printf("font atlas: %zu glyphs\n", codepoints_.size());
}

float TextRenderer::measure(const std::string& utf8, float sizePx) {
    float width = 0;
    for (int cp : decodeUtf8(utf8)) {
        if (auto it = glyphs_.find(cp); it != glyphs_.end()) {
            width += it->second.advance;
        } else if (std::find(codepoints_.begin(), codepoints_.end(), cp) == codepoints_.end()) {
            codepoints_.push_back(cp);
            needsBuild_ = true;
        }
    }
    return width * sizePx;
}

void TextRenderer::add(Anchor anchor, const astro::Vec3& pos, const std::string& utf8, float sizePx,
                       const float rgba[4], Align align, float offsetXPx, float offsetYPx) {
    if (!ok()) return;
    const auto cps = decodeUtf8(utf8);
    float width = 0;
    for (int cp : cps) {
        if (auto it = glyphs_.find(cp); it != glyphs_.end()) {
            width += it->second.advance;
        } else if (std::find(codepoints_.begin(), codepoints_.end(), cp) == codepoints_.end()) {
            // 登録されていない文字: 次の flush() でアトラスを作り直す（このフレームでは描かない）
            codepoints_.push_back(cp);
            needsBuild_ = true;
        }
    }
    // 文字の高さの中央（em の 0.35 付近）を基準点に合わせる
    const float alignShift = align == Center ? -0.5f * width : align == Right ? -width : 0.0f;
    float x = alignShift * sizePx + offsetXPx;
    const float y = -0.35f * sizePx + offsetYPx;
    for (int cp : cps) {
        auto it = glyphs_.find(cp);
        if (it == glyphs_.end()) continue;
        const Glyph& g = it->second;
        const float x0 = x + g.x0 * sizePx, x1 = x + g.x1 * sizePx;
        const float y0 = y + g.y0 * sizePx, y1 = y + g.y1 * sizePx;
        const float corners[4][4] = {{x0, y0, g.u0, g.v0}, {x1, y0, g.u1, g.v0}, {x0, y1, g.u0, g.v1},
                                     {x1, y1, g.u1, g.v1}};
        for (int k : {0, 1, 2, 2, 1, 3}) {
            vertices_.push_back({{float(pos.x), float(pos.y), float(pos.z)},
                                 float(anchor),
                                 {corners[k][0], corners[k][1]},
                                 {corners[k][2], corners[k][3]},
                                 {rgba[0], rgba[1], rgba[2], rgba[3]}});
        }
        x += g.advance * sizePx;
    }
}

void TextRenderer::flush(int width, int height) {
    if (!ok()) return;
    if (needsBuild_) build();
    if (vertices_.empty()) return;
    const size_t bytes = vertices_.size() * sizeof(Vertex);
    if (bytes > vboCapacity_) {
        vboCapacity_ = bytes * 2;
        glNamedBufferData(vbo_, static_cast<GLsizeiptr>(vboCapacity_), nullptr, GL_DYNAMIC_DRAW);
    }
    glNamedBufferSubData(vbo_, 0, static_cast<GLsizeiptr>(bytes), vertices_.data());

    const float viewport[2] = {float(width), float(height)};
    glProgramUniform2fv(program_, glGetUniformLocation(program_, "u_viewport"), 1, viewport);
    glProgramUniform1i(program_, glGetUniformLocation(program_, "u_atlas"), 0);
    glBindTextureUnit(0, atlas_);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(program_);
    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices_.size()));
    glDisable(GL_BLEND);
    vertices_.clear();
}
