#include "bodies.hpp"

#include <cmath>

#include "astronomy.h"

namespace {
constexpr double kAuKm = 149597870.7;

struct BodyDef {
    astro_body_t body;
    BodyInfo::Kind kind;
    const char* nameJa;
    double radiusKm;
    float color[3];
};

// 惑星の色は見た目の印象で決めた値（線形 sRGB）
const BodyDef kBodies[] = {
    {BODY_SUN, BodyInfo::Sun, "太陽", 696000.0, {1.0f, 0.95f, 0.85f}},
    {BODY_MOON, BodyInfo::Moon, "月", 1737.4, {1.0f, 0.96f, 0.90f}},
    {BODY_MERCURY, BodyInfo::Planet, "水星", 2439.7, {0.95f, 0.88f, 0.80f}},
    {BODY_VENUS, BodyInfo::Planet, "金星", 6051.8, {1.0f, 0.97f, 0.88f}},
    {BODY_MARS, BodyInfo::Planet, "火星", 3389.5, {1.0f, 0.55f, 0.35f}},
    {BODY_JUPITER, BodyInfo::Planet, "木星", 69911.0, {1.0f, 0.93f, 0.82f}},
    {BODY_SATURN, BodyInfo::Planet, "土星", 58232.0, {1.0f, 0.88f, 0.65f}},
    {BODY_URANUS, BodyInfo::Planet, "天王星", 25362.0, {0.75f, 0.92f, 1.0f}},
};
}  // namespace

std::vector<BodyInfo> computeBodies(double jdUt, const ObserverConfig& obs) {
    astro_time_t time = Astronomy_TimeFromDays(jdUt - astro::kJ2000);
    const astro_observer_t observer = Astronomy_MakeObserver(obs.latitude_deg, obs.longitude_deg, obs.elevation_m);

    std::vector<BodyInfo> out;
    for (const auto& def : kBodies) {
        // 観測地からの見かけの位置（地心でなく地表から。月では視差が最大 1° 効く）
        const astro_equatorial_t eq = Astronomy_Equator(def.body, &time, observer, EQUATOR_J2000, ABERRATION);
        if (eq.status != ASTRO_SUCCESS) continue;
        BodyInfo b{};
        b.kind = def.kind;
        b.nameJa = def.nameJa;
        b.dirJ2000 = astro::equatorialVector(eq.ra * 15.0, eq.dec);
        b.angularRadiusDeg = std::asin(def.radiusKm / (eq.dist * kAuKm)) / astro::kDeg;
        for (int i = 0; i < 3; ++i) b.color[i] = def.color[i];
        if (def.body == BODY_SUN) {
            b.mag = -26.74;
            b.phaseFraction = 1.0;
        } else {
            const astro_illum_t ill = Astronomy_Illumination(def.body, time);
            b.mag = ill.mag;
            b.phaseAngleDeg = ill.phase_angle;
            b.phaseFraction = ill.phase_fraction;
        }
        out.push_back(b);
    }
    return out;
}
