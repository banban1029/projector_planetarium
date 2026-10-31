#pragma once
#include <string>
#include <vector>

#include "astro.hpp"
#include "projection.hpp"
#include "text_renderer.hpp"

// 天球上に置く名前 1 つ
struct LabelRequest {
    TextRenderer::Anchor anchor;  // SkyJ2000 または SkyHorizon
    astro::Vec3 dir;
    std::string text;
    float sizePx;
    float color[4];
    float priority;    // 小さいほど優先して置く
    bool besidePoint;  // true: 星の横に置く（右下・右上・左下・左上の順に試す） / false: 中央に置く（上下にずらして試す）
};

// 画面上で重ならないように位置を選び、text に積む。地平線（景色）より下、画面の外、
// どこに置いても重なるものは省く。選んだ位置は shaders/text.vert と同じ投影で計算する
void placeLabels(TextRenderer& text, std::vector<LabelRequest> labels, const Projection& projection,
                 const astro::Mat3& j2000ToHorizon, int width, int height);
