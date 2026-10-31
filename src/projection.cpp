#include "projection.hpp"

#include <algorithm>
#include <cmath>

astro::Vec3 viewForward(const ViewSettings& view) {
    const double alt = view.centerAltDeg * astro::kDeg, az = view.centerAzDeg * astro::kDeg;
    return {std::cos(alt) * std::cos(az), std::cos(alt) * std::sin(az), std::sin(alt)};
}

Projection makeProjection(const ViewSettings& view, int width, int height, int landscape) {
    Projection p{};
    // 視線方向 f と、そこから天頂側へ向かう上方向 u（地平座標: x = 北, y = 東, z = 天頂）
    const double alt = view.centerAltDeg * astro::kDeg, az = view.centerAzDeg * astro::kDeg;
    const astro::Vec3 f = viewForward(view);
    const double u[3] = {-std::sin(alt) * std::cos(az), -std::sin(alt) * std::sin(az), std::cos(alt)};
    // 右 = u × f。天頂を見上げて南が下のとき、右 = 西（東が左の、見上げた星空の向き）
    const double r[3] = {u[1] * f.z - u[2] * f.y, u[2] * f.x - u[0] * f.z, u[0] * f.y - u[1] * f.x};
    for (int i = 0; i < 3; ++i) {
        p.view[0][i] = r[i];
        p.view[1][i] = u[i];
    }
    p.view[2][0] = f.x;
    p.view[2][1] = f.y;
    p.view[2][2] = f.z;

    p.fisheye = view.fisheye;
    p.halfFovRad = view.fovDeg * 0.5 * astro::kDeg;
    p.aspect[0] = width >= height ? double(height) / width : 1.0;
    p.aspect[1] = width >= height ? 1.0 : double(width) / height;
    // 回転してから反転する
    const double rot = view.rotationDeg * astro::kDeg;
    const double sx = view.flipH ? -1.0 : 1.0, sy = view.flipV ? -1.0 : 1.0;
    p.screen[0][0] = sx * std::cos(rot);
    p.screen[0][1] = -sx * std::sin(rot);
    p.screen[1][0] = sy * std::sin(rot);
    p.screen[1][1] = sy * std::cos(rot);
    p.landscape = landscape;
    return p;
}

bool Projection::project(const astro::Vec3& h, double& x, double& y) const {
    const double d[3] = {view[0][0] * h.x + view[0][1] * h.y + view[0][2] * h.z,
                         view[1][0] * h.x + view[1][1] * h.y + view[1][2] * h.z,
                         view[2][0] * h.x + view[2][1] * h.y + view[2][2] * h.z};
    double px, py;
    if (fisheye) {
        const double theta = std::acos(std::clamp(d[2], -1.0, 1.0));
        if (theta > halfFovRad) return false;
        const double len = std::hypot(d[0], d[1]);
        const double k = len > 1e-7 ? theta / halfFovRad / len : 0.0;
        px = d[0] * k;
        py = d[1] * k;
    } else {
        if (d[2] < 1e-4) return false;
        const double t = std::tan(halfFovRad);
        px = d[0] / (d[2] * t);
        py = d[1] / (d[2] * t);
    }
    x = (screen[0][0] * px + screen[0][1] * py) * aspect[0];
    y = (screen[1][0] * px + screen[1][1] * py) * aspect[1];
    return true;
}

void applyMirror(ViewSettings& view, int& landscape) {
    landscape = 0;
    if (view.fisheye) view.fovDeg = std::min(view.fovDeg * 1.35, 300.0);
}

double groundAltDeg(int landscape, double azDeg) {
    const double a = azDeg * astro::kDeg;
    if (landscape == 1) {
        const double h = 2.4 + 1.3 * std::sin(2.0 * a + 1.3) + 0.9 * std::sin(5.0 * a + 0.4) +
                         0.5 * std::sin(11.0 * a + 2.1) + 0.25 * std::sin(23.0 * a + 0.7) +
                         0.12 * std::sin(47.0 * a + 1.9);
        return std::max(h, 0.8);
    }
    if (landscape == 2) {
        // GPU の float と完全には一致しないが、名前の表示判定には十分（建物の最高 4.5°）
        auto hash = [](double x) {
            const double v = std::sin(x * 127.1) * 43758.5453;
            return v - std::floor(v);
        };
        const double i = std::floor(std::fmod(std::fmod(azDeg, 360.0) + 360.0, 360.0) / 2.5);
        const double r = hash(i);
        return 0.3 + 2.2 * r * r + (hash(i + 91.0) > 0.93 ? 2.0 : 0.0);
    }
    return 0.0;
}
