#pragma once
#include <GL/glew.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "astro.hpp"

// 日本語の文字を描く。起動時に使う文字だけをフォントからテクスチャ（アトラス）に焼き込み、
// 毎フレーム文字ごとの四角形を並べて描く。詳細は docs/rendering.md
class TextRenderer {
public:
    // 文字の基準点の種類
    enum Anchor { SkyJ2000 = 0, SkyHorizon = 1, Screen = 2 };
    enum Align { Left, Center, Right };

    // fontPath が空なら Windows の日本語フォントを順に探す。見つからなければ ok() == false
    TextRenderer(const std::string& root, const std::string& fontPath);
    ~TextRenderer();
    TextRenderer(const TextRenderer&) = delete;
    TextRenderer& operator=(const TextRenderer&) = delete;

    bool ok() const { return loaded_; }

    // 描く可能性のある文字を先に登録しておく（未登録の文字は初めて使われたときにアトラスを作り直す）
    void addCharacters(const std::string& utf8);
    void build();

    // 1 フレーム分の文字を積む。anchor は Sky* なら方向ベクトル、Screen なら NDC (x, y, 0)。
    // offsetPx は基準点からのずらし [px]（y 上向き）
    void add(Anchor anchor, const astro::Vec3& pos, const std::string& utf8, float sizePx, const float rgba[4],
             Align align = Center, float offsetXPx = 0.0f, float offsetYPx = 0.0f);
    // 文字列の幅 [px]（未登録の文字は次の flush() で登録される）
    float measure(const std::string& utf8, float sizePx);
    // 積んだ文字を描いて空にする。投影の uniform は呼び出し側で program() に設定しておく
    void flush(int width, int height);

    GLuint program() const { return program_; }

private:
    struct Glyph {
        float x0, y0, x1, y1;  // 基準線からの四角形 [em]
        float u0, v0, u1, v1;
        float advance;         // [em]
    };
    struct Vertex {
        float pos[3];
        float anchor;
        float offset[2];
        float uv[2];
        float color[4];
    };

    std::string fontPath_;
    std::vector<unsigned char> fontData_;
    std::vector<int> codepoints_;
    std::unordered_map<int, Glyph> glyphs_;
    std::vector<Vertex> vertices_;
    bool loaded_ = false;
    bool needsBuild_ = true;
    GLuint program_ = 0, atlas_ = 0, vao_ = 0, vbo_ = 0;
    size_t vboCapacity_ = 0;
};

// UTF-8 -> Unicode コードポイント
std::vector<int> decodeUtf8(const std::string& s);
