#pragma once
#include <GL/glew.h>

#include <string>
#include <vector>

#include "astro.hpp"
#include "config.hpp"
#include "modes.hpp"
#include "star_catalog.hpp"

// 視点・投影の設定（キー操作で変わるので Config とは別に持つ）
struct ViewSettings {
    bool fisheye = true;
    double fovDeg = 180.0;
    double centerAltDeg = 90.0;
    double centerAzDeg = 180.0;
    double rotationDeg = 0.0;
    bool flipH = false;
    bool flipV = false;
};

// 1 フレーム分の空の状態
struct SkyState {
    astro::Mat3 j2000ToHorizon;
    astro::Vec3 sunHorizon;  // 太陽方向（地平座標の単位ベクトル）
    double sunAltDeg;
};

// 星空に重ねる補助表示（キーで ON/OFF）
struct Overlays {
    bool grid = false;     // 高度・方位グリッド
    bool compass = false;  // 方位の文字 N/E/S/W
};

// 背景（空・地面・グリッド）→ 恒星 → 方位の文字 の順に HDR バッファへ描き、
// 最後に露出・ガンマ補正して出力する
class SkyRenderer {
public:
    SkyRenderer(const std::string& root, const std::vector<StarVertex>& stars, const RenderConfig& render);
    ~SkyRenderer();
    SkyRenderer(const SkyRenderer&) = delete;
    SkyRenderer& operator=(const SkyRenderer&) = delete;

    void render(int width, int height, const ViewSettings& view, const SkyState& sky, const ModeStyle& style,
                float realTimeSec, const Overlays& overlays);

private:
    void ensureTarget(int width, int height);
    void setProjectionUniforms(GLuint prog, int width, int height, const ViewSettings& view);
    void createCompass();

    RenderConfig render_;
    GLuint starProgram_ = 0, skyProgram_ = 0, postProgram_ = 0, compassProgram_ = 0;
    GLuint starVao_ = 0, starVbo_ = 0, emptyVao_ = 0;
    GLsizei starCount_ = 0;
    GLuint compassVao_ = 0, compassVbo_ = 0;
    GLsizei compassCount_ = 0;
    GLuint fbo_ = 0, sceneTex_ = 0;
    int targetW_ = 0, targetH_ = 0;
};

// 太陽高度から求める空の明るさと、見える最暗等級（リアルモード用）。詳細は docs/rendering.md
struct SkyLight {
    float zenith[3];
    float horizon[3];
    float sunGlow[3];
    float magLimit;
};
SkyLight skyLightForSun(double sunAltDeg, double nightMagLimit);
