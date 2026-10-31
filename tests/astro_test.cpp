// 天文計算の検証。期待値は Meeus "Astronomical Algorithms" 2nd ed. の例題、
// および幾何学的に答えが決まる場合（子午線通過・東の地平線など）。
#include <cmath>
#include <cstdio>

#include "astro.hpp"
#include "astronomy.h"

using namespace astro;

namespace {
int failures = 0;

void check(const char* name, double actual, double expected, double tolerance) {
    const double diff = std::abs(actual - expected);
    const bool ok = diff <= tolerance;
    std::printf("[%s] %-40s actual=%.6f expected=%.6f diff=%.2e\n", ok ? " OK " : "FAIL", name, actual,
                expected, diff);
    if (!ok) ++failures;
}

double hms(double h, double m, double s) { return (h + m / 60.0 + s / 3600.0) * 15.0; }
double dms(double d, double m, double s) { return (d < 0 ? -1 : 1) * (std::abs(d) + m / 60.0 + s / 3600.0); }
constexpr double kArcsec = 1.0 / 3600.0;
}  // namespace

int main() {
    // Unix 0 = JD 2440587.5, J2000.0 = 2000-01-01 12:00 UTC
    check("JD of Unix epoch", julianDateFromUnix(0), 2440587.5, 1e-9);
    check("JD of J2000.0", julianDateFromUnix(946728000.0), kJ2000, 1e-9);

    // Meeus 12.a: 1987-04-10 0h UT -> GMST 13h10m46.3668s
    check("GMST Meeus 12.a", greenwichMeanSiderealDeg(2446895.5), hms(13, 10, 46.3668), 1e-5);

    // Meeus 21.b: θ Per（固有運動適用済み）J2000 -> JD 2462088.69
    {
        const Vec3 v = precessionFromJ2000(2462088.69) * equatorialVector(hms(2, 44, 12.975), dms(49, 13, 39.90));
        const double ra = std::atan2(v.y, v.x) / kDeg;
        const double dec = std::asin(v.z) / kDeg;
        check("precession RA  Meeus 21.b", ra, hms(2, 46, 11.331), 1.0 * kArcsec);
        check("precession Dec Meeus 21.b", dec, dms(49, 20, 54.54), 1.0 * kArcsec);
    }

    // Meeus 25.a: 1992-10-13 0h TD -> 太陽 α=13h13m31.4s δ=-7°47'06"
    {
        double ra, dec;
        sunEquatorial(2448908.5, ra, dec);
        check("sun RA  Meeus 25.a", ra, hms(13, 13, 31.4), 0.01);
        check("sun Dec Meeus 25.a", dec, dms(-7, 47, 6), 0.01);
    }

    // 地平座標: 大阪 φ=34.82°
    {
        const double lat = 34.82, lst = 100.0;
        const Mat3 m = equatorialToHorizon(lst, lat);
        double alt, az;

        altAz(m * equatorialVector(lst, 0.0), alt, az);  // 赤道上の星の南中
        check("transit dec=0: alt", alt, 90.0 - lat, 1e-9);
        check("transit dec=0: az (south)", az, 180.0, 1e-9);

        altAz(m * equatorialVector(lst, 60.0), alt, az);  // 天頂より北で南中（北中）
        check("transit dec=60: alt", alt, 90.0 - (60.0 - lat), 1e-9);
        check("transit dec=60: az (north)", az, 0.0, 1e-9);

        altAz(m * equatorialVector(lst + 90.0, 0.0), alt, az);  // 時角 -6h = 東の地平線
        check("H=-6h dec=0: alt", alt, 0.0, 1e-9);
        check("H=-6h dec=0: az (east)", az, 90.0, 1e-9);

        altAz(m * equatorialVector(0.0, 90.0), alt, az);  // 天の北極の高度 = 緯度
        check("celestial pole alt", alt, lat, 1e-9);
    }

    // 北極星（J2000 で極から 0.74°）は日時によらず高度 ≒ 緯度
    {
        const double jd = julianDateFromUnix(1793451600.0);  // 2026-10-31 12:00 UTC
        double alt, az;
        altAz(j2000ToHorizon(jd, 34.82, 135.52) * equatorialVector(37.946250, 89.264109), alt, az);
        check("Polaris alt ~ latitude", alt, 34.82, 0.8);
    }

    // 大気差: 地平線で約 0.5°、高度 45° で約 1'
    check("refraction at 0 deg", refractionDeg(0.0), 0.48, 0.03);
    check("refraction at 45 deg", refractionDeg(45.0), 1.0 / 60.0, 0.1 / 60.0);

    // 独立した実装（Astronomy Engine）との比較。章動・光行差・分点差の分（〜0.01°）だけずれてよい
    {
        const astro_observer_t osaka = Astronomy_MakeObserver(34.82, 135.52, 50.0);
        const struct {
            const char* name;
            double raDeg, decDeg;
        } stars[] = {{"Sirius", 101.287215, -16.716116}, {"Vega", 279.234600, 38.783692},
                     {"Polaris", 37.946250, 89.264109}, {"Antares", 247.351915, -26.432003}};
        const double unixTimes[] = {1765803600.0, 1781179200.0, 1793451600.0};  // 2025-12-15, 2026-06-11, 2026-10-31
        for (double unix : unixTimes) {
            const double jd = julianDateFromUnix(unix);
            astro_time_t t = Astronomy_TimeFromDays(jd - kJ2000);
            const Mat3 m = j2000ToHorizon(jd, 34.82, 135.52);
            for (const auto& s : stars) {
                Astronomy_DefineStar(BODY_STAR1, s.raDeg / 15.0, s.decDeg, 1000.0);
                const auto eq = Astronomy_Equator(BODY_STAR1, &t, osaka, EQUATOR_OF_DATE, ABERRATION);
                const auto hor = Astronomy_Horizon(&t, osaka, eq.ra, eq.dec, REFRACTION_NONE);
                double alt, az;
                altAz(m * equatorialVector(s.raDeg, s.decDeg), alt, az);
                char name[64];
                std::snprintf(name, sizeof(name), "vs AstronomyEngine %s alt", s.name);
                check(name, alt, hor.altitude, 0.02);
                if (hor.altitude < 85.0 && s.decDeg < 89.0) {  // 天頂・天の北極付近は方位が不安定
                    std::snprintf(name, sizeof(name), "vs AstronomyEngine %s az", s.name);
                    const double d = std::remainder(az - hor.azimuth, 360.0);
                    check(name, d, 0.0, 0.02 / std::cos(alt * kDeg));
                }
            }
            // 太陽（低精度式）
            double ra, dec, alt, az;
            sunEquatorial(jd, ra, dec);
            const double lst = greenwichMeanSiderealDeg(jd) + 135.52;
            altAz(equatorialToHorizon(lst, 34.82) * equatorialVector(ra, dec), alt, az);
            const auto seq = Astronomy_Equator(BODY_SUN, &t, osaka, EQUATOR_OF_DATE, ABERRATION);
            const auto shor = Astronomy_Horizon(&t, osaka, seq.ra, seq.dec, REFRACTION_NONE);
            check("vs AstronomyEngine Sun alt", alt, shor.altitude, 0.03);
        }
    }

    std::printf("%s (%d failures)\n", failures ? "FAILED" : "PASSED", failures);
    return failures ? 1 : 0;
}
