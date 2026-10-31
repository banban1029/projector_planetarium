#pragma once
// 天文計算（座標変換・時刻）。アルゴリズムの説明は docs/astronomy.md を参照。
//
// 座標系の約束:
//   赤道座標ベクトル : x = 春分点方向, y = 赤経 6h, z = 天の北極
//   地平座標ベクトル : x = 北, y = 東, z = 天頂（右手系）
//   方位角は北から東回り、高度は地平線から上向き

namespace astro {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = kPi / 180.0;
constexpr double kJ2000 = 2451545.0;

// 3x3 行列（行優先）。m[row][col]
struct Mat3 {
    double m[3][3];
    Mat3 operator*(const Mat3& b) const;
};

struct Vec3 {
    double x, y, z;
};
Vec3 operator*(const Mat3& a, const Vec3& v);

Vec3 equatorialVector(double raDeg, double decDeg);

// 地平座標ベクトル -> 高度・方位（度）
void altAz(const Vec3& horizon, double& altDeg, double& azDeg);

double julianDateFromUnix(double unixSeconds);

// グリニッジ平均恒星時（度, 0-360）。IAU 1982
double greenwichMeanSiderealDeg(double jdUt);

// J2000.0 平均赤道座標 -> jd の平均赤道座標（IAU 1976 歳差）
Mat3 precessionFromJ2000(double jd);

// 日付の平均赤道座標 -> 地平座標
Mat3 equatorialToHorizon(double localSiderealDeg, double latitudeDeg);

// J2000 赤道座標 -> 観測地の地平座標（歳差 + 日周運動）を一度に行う行列
Mat3 j2000ToHorizon(double jdUt, double latitudeDeg, double longitudeDeg);

// 太陽の視赤経・赤緯（度, 日付の分点）。低精度式, 誤差 ~0.01°
void sunEquatorial(double jd, double& raDeg, double& decDeg);

// 大気差（度）。真高度 -> 見かけの高度への補正量（Sæmundsson の式）
double refractionDeg(double trueAltDeg);

}  // namespace astro
