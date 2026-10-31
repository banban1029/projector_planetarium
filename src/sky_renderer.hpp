#pragma once
#include <GL/glew.h>

#include <memory>
#include <string>
#include <vector>

#include "astro.hpp"
#include "bodies.hpp"
#include "config.hpp"
#include "constellations.hpp"
#include "line_renderer.hpp"
#include "modes.hpp"
#include "projection.hpp"
#include "star_catalog.hpp"
#include "text_renderer.hpp"

// 1 フレーム分の空の状態
struct SkyState {
    astro::Mat3 j2000ToHorizon;
    astro::Vec3 sunHorizon;  // 太陽方向（地平座標の単位ベクトル）
    double sunAltDeg;
    double nightMagLimit;    // 観測地の夜空の最暗等級（リアルモード）
    double moonAltDeg;
    double moonPhaseFraction;
    double eyeLimit = 99.0;  // 目が暗さに慣れる途中の最暗等級（上映の演出。99 = 制限なし）
};

// 星空に重ねる補助表示（キーで ON/OFF）
struct Overlays {
    bool grid = false;            // 高度・方位グリッド
    bool compass = false;         // 方位の文字 北/東/南/西
    bool constellations = false;      // 星座線
    bool constellationNames = false;  // 星座名
    bool labels = false;              // 星・惑星・月の名前
    bool info = false;                // 日時などの画面表示
    bool credits = false;             // クレジット（データの出典）
    bool testPattern = false;         // 投影の調整用テストパターン
};

struct FrameInput {
    int width, height;
    ViewSettings view;
    SkyState sky;
    ModeStyle style;
    float timeSec;  // 実時間（瞬き・演出のアニメーション用）
    Overlays overlays;
    int landscape;  // 0 = なし, 1 = 山並み, 2 = 街並み
    const std::vector<BodyInfo>* bodies;
    const std::vector<LineVertex>* effectLines;  // 流れ星・ワープの光跡
};

// 背景 → 星座線 → 恒星・惑星 → 月・太陽 → 演出の線 → 文字 の順に HDR バッファへ描き、
// 光のにじみ・水鏡・フェードを加えて露出・ガンマ補正して出力する。詳細は docs/rendering.md
class SkyRenderer {
public:
    SkyRenderer(const std::string& root, const std::vector<StarVertex>& stars, const ConstellationData& constellations,
                const RenderConfig& render);
    ~SkyRenderer();
    SkyRenderer(const SkyRenderer&) = delete;
    SkyRenderer& operator=(const SkyRenderer&) = delete;

    // 文字は render() の前に text().add() で積んでおく
    TextRenderer& text() { return *text_; }
    // 今の画面から次の画面へ durationSec 秒でフェードする（モード・場所・季節の切り替え時）
    void startTransition(float nowSec, float durationSec);
    void render(const FrameInput& in);
    // 最後に画面へ出した画像（操作パネルのプレビュー用。共有コンテキストから使える）
    GLuint outputTexture() const { return outputTex_; }

private:
    void ensureTargets(int width, int height);
    void setProjectionUniforms(GLuint prog, int width, int height, const ViewSettings& view, int landscape);
    void drawFullscreen(GLuint prog);

    RenderConfig render_;
    std::unique_ptr<TextRenderer> text_;
    std::unique_ptr<LineRenderer> lines_;
    GLuint starProgram_ = 0, skyProgram_ = 0, bodyProgram_ = 0, bloomProgram_ = 0, postProgram_ = 0;
    GLuint starVao_ = 0, starVbo_ = 0, planetVao_ = 0, planetVbo_ = 0, bodyVao_ = 0, bodyVbo_ = 0, emptyVao_ = 0;
    GLsizei starCount_ = 0;
    // HDR の描画先（scene）、フェード前の画面（fade）、ブルーム用の縮小バッファ 2 枚
    GLuint sceneFbo_ = 0, sceneTex_ = 0, fadeTex_ = 0;
    GLuint outputFbo_ = 0, outputTex_ = 0;  // 仕上げた画像（8bit）。ここから画面へ転送する
    GLuint testPatternProgram_ = 0;
    GLuint bloomFbo_[2] = {0, 0}, bloomTex_[2] = {0, 0};
    int targetW_ = 0, targetH_ = 0;
    bool hasFrame_ = false;
    float fadeStart_ = -1.0f, fadeDuration_ = 1.0f;
};

// 太陽高度から求める空の明るさと、見える最暗等級（リアルモード用）。詳細は docs/rendering.md
struct SkyLight {
    float zenith[3];
    float horizon[3];
    float sunGlow[3];
    float magLimit;
};
SkyLight skyLightForSun(double sunAltDeg, double nightMagLimit);
// 月明かりで空を明るくし、見える星を減らす
void addMoonlight(SkyLight& light, double moonAltDeg, double moonPhaseFraction);
