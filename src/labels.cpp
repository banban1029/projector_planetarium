#include "labels.hpp"

#include <algorithm>
#include <cmath>

namespace {
struct Rect {
    double x0, y0, x1, y1;
    bool overlaps(const Rect& o) const { return x0 < o.x1 && o.x0 < x1 && y0 < o.y1 && o.y0 < y1; }
};

// 基準点 (ax, ay) [px] から、文字の矩形（ずらし ox, oy・幅 w・高さ h、Left 揃え）を画面の回転・反転に合わせて囲む矩形
Rect screenRect(const Projection& p, double ax, double ay, double ox, double oy, double w, double h) {
    const double cx[4] = {ox, ox + w, ox, ox + w}, cy[4] = {oy - h * 0.5, oy - h * 0.5, oy + h * 0.5, oy + h * 0.5};
    Rect r{1e9, 1e9, -1e9, -1e9};
    for (int i = 0; i < 4; ++i) {
        const double x = ax + p.screen[0][0] * cx[i] + p.screen[0][1] * cy[i];
        const double y = ay + p.screen[1][0] * cx[i] + p.screen[1][1] * cy[i];
        r.x0 = std::min(r.x0, x);
        r.x1 = std::max(r.x1, x);
        r.y0 = std::min(r.y0, y);
        r.y1 = std::max(r.y1, y);
    }
    return r;
}
}  // namespace

void placeLabels(TextRenderer& text, std::vector<LabelRequest> labels, const Projection& projection,
                 const astro::Mat3& j2000ToHorizon, int width, int height) {
    std::stable_sort(labels.begin(), labels.end(),
                     [](const LabelRequest& a, const LabelRequest& b) { return a.priority < b.priority; });
    std::vector<Rect> placed;
    for (const auto& l : labels) {
        const astro::Vec3 h = l.anchor == TextRenderer::SkyJ2000 ? j2000ToHorizon * l.dir : l.dir;
        double alt, az;
        astro::altAz(h, alt, az);
        if (alt < groundAltDeg(projection.landscape, az) + 0.5) continue;  // 沈んでいる
        double nx, ny;
        if (!projection.project(h, nx, ny)) continue;
        const double ax = (nx + 1.0) * 0.5 * width, ay = (ny + 1.0) * 0.5 * height;
        if (ax < 0 || ax > width || ay < 0 || ay > height) continue;

        const double w = text.measure(l.text, l.sizePx), hgt = l.sizePx;
        const double gap = 0.6 * l.sizePx;
        // 試す位置（Left 揃えのずらし）。星の横 4 か所、または中央・上・下
        std::vector<std::pair<double, double>> offsets;
        if (l.besidePoint) {
            offsets = {{gap, -gap}, {gap, gap}, {-gap - w, -gap}, {-gap - w, gap}};
        } else {
            offsets = {{-w * 0.5, 0.0}, {-w * 0.5, hgt * 1.1}, {-w * 0.5, -hgt * 1.1}};
        }
        for (const auto& [ox, oy] : offsets) {
            const Rect r = screenRect(projection, ax, ay, ox, oy, w, hgt);
            const bool free = std::none_of(placed.begin(), placed.end(), [&](const Rect& p) { return p.overlaps(r); });
            if (!free) continue;
            placed.push_back(r);
            text.add(l.anchor, l.dir, l.text, l.sizePx, l.color, TextRenderer::Left, static_cast<float>(ox),
                     static_cast<float>(oy));
            break;
        }
    }
}
