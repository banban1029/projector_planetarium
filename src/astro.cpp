#include "astro.hpp"

#include <cmath>

namespace astro {

namespace {
double wrap360(double deg) {
    deg = std::fmod(deg, 360.0);
    if (deg < 0) deg += 360.0;
    return deg >= 360.0 ? 0.0 : deg;  // -1e-15 + 360 が 360 に丸まる場合
}

// 座標軸の回転（フレーム回転）。ベクトルを角度 a だけ逆向きに回すのと同じ
Mat3 rotZ(double a) {
    const double c = std::cos(a), s = std::sin(a);
    return {{{c, s, 0}, {-s, c, 0}, {0, 0, 1}}};
}
}  // namespace

Mat3 Mat3::operator*(const Mat3& b) const {
    Mat3 r{};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            for (int k = 0; k < 3; ++k) r.m[i][j] += m[i][k] * b.m[k][j];
    return r;
}

Vec3 operator*(const Mat3& a, const Vec3& v) {
    return {a.m[0][0] * v.x + a.m[0][1] * v.y + a.m[0][2] * v.z,
            a.m[1][0] * v.x + a.m[1][1] * v.y + a.m[1][2] * v.z,
            a.m[2][0] * v.x + a.m[2][1] * v.y + a.m[2][2] * v.z};
}

Vec3 equatorialVector(double raDeg, double decDeg) {
    const double ra = raDeg * kDeg, dec = decDeg * kDeg;
    return {std::cos(dec) * std::cos(ra), std::cos(dec) * std::sin(ra), std::sin(dec)};
}

void altAz(const Vec3& h, double& altDeg, double& azDeg) {
    altDeg = std::asin(std::fmax(-1.0, std::fmin(1.0, h.z))) / kDeg;
    azDeg = wrap360(std::atan2(h.y, h.x) / kDeg);
}

double julianDateFromUnix(double unixSeconds) { return unixSeconds / 86400.0 + 2440587.5; }

double greenwichMeanSiderealDeg(double jdUt) {
    const double d = jdUt - kJ2000;
    const double t = d / 36525.0;
    return wrap360(280.46061837 + 360.98564736629 * d + 0.000387933 * t * t - t * t * t / 38710000.0);
}

Mat3 precessionFromJ2000(double jd) {
    const double t = (jd - kJ2000) / 36525.0;
    const double arcsec = kDeg / 3600.0;
    const double zeta = (2306.2181 * t + 0.30188 * t * t + 0.017998 * t * t * t) * arcsec;
    const double z = (2306.2181 * t + 1.09468 * t * t + 0.018203 * t * t * t) * arcsec;
    const double theta = (2004.3109 * t - 0.42665 * t * t - 0.041833 * t * t * t) * arcsec;
    const double cz = std::cos(zeta), sz = std::sin(zeta);
    const double cZ = std::cos(z), sZ = std::sin(z);
    const double ct = std::cos(theta), st = std::sin(theta);
    // Meeus, Astronomical Algorithms 2nd ed. 21 章の厳密な回転行列
    return {{{cz * cZ * ct - sz * sZ, -sz * cZ * ct - cz * sZ, -st * cZ},
             {cz * sZ * ct + sz * cZ, -sz * sZ * ct + cz * cZ, -st * sZ},
             {cz * st, -sz * st, ct}}};
}

Mat3 equatorialToHorizon(double localSiderealDeg, double latitudeDeg) {
    // 1) z 軸まわりに恒星時だけ回す -> x = 子午線方向（時角 0）, y = 東, z = 天の北極
    const Mat3 hourAngle = rotZ(localSiderealDeg * kDeg);
    // 2) 天頂 Z=(cosφ,0,sinφ), 北 N=(-sinφ,0,cosφ), 東 E=(0,1,0) を行に並べて射影
    const double phi = latitudeDeg * kDeg;
    const double s = std::sin(phi), c = std::cos(phi);
    const Mat3 toHorizon{{{-s, 0, c}, {0, 1, 0}, {c, 0, s}}};
    return toHorizon * hourAngle;
}

Mat3 j2000ToHorizon(double jdUt, double latitudeDeg, double longitudeDeg) {
    const double lst = greenwichMeanSiderealDeg(jdUt) + longitudeDeg;
    return equatorialToHorizon(lst, latitudeDeg) * precessionFromJ2000(jdUt);
}

void sunEquatorial(double jd, double& raDeg, double& decDeg) {
    // Meeus 25 章の低精度式（章動・光行差を Ω 項で近似）
    const double t = (jd - kJ2000) / 36525.0;
    const double l0 = 280.46646 + 36000.76983 * t + 0.0003032 * t * t;
    const double m = (357.52911 + 35999.05029 * t - 0.0001537 * t * t) * kDeg;
    const double c = (1.914602 - 0.004817 * t - 0.000014 * t * t) * std::sin(m) +
                     (0.019993 - 0.000101 * t) * std::sin(2 * m) + 0.000289 * std::sin(3 * m);
    const double omega = (125.04 - 1934.136 * t) * kDeg;
    const double lambda = (l0 + c - 0.00569 - 0.00478 * std::sin(omega)) * kDeg;
    const double eps0 = 23.0 + (26.0 + (21.448 - 46.8150 * t) / 60.0) / 60.0;
    const double eps = (eps0 + 0.00256 * std::cos(omega)) * kDeg;
    raDeg = wrap360(std::atan2(std::cos(eps) * std::sin(lambda), std::cos(lambda)) / kDeg);
    decDeg = std::asin(std::sin(eps) * std::sin(lambda)) / kDeg;
}

double refractionDeg(double trueAltDeg) {
    if (trueAltDeg < -2.0) return 0.0;
    // Sæmundsson (1986), 気温 10℃・1010 hPa。結果は分角
    const double r = 1.02 / std::tan((trueAltDeg + 10.3 / (trueAltDeg + 5.11)) * kDeg);
    return r / 60.0;
}

}  // namespace astro
