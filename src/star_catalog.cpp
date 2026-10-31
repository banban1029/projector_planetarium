#include "star_catalog.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "astro.hpp"

namespace {
// CIE 1931 等色関数の多峰ガウス近似（Wyman, Sloan, Shirley 2013）
double lobe(double lambda, double mu, double s1, double s2) {
    const double t = (lambda - mu) / (lambda < mu ? s1 : s2);
    return std::exp(-0.5 * t * t);
}

void cieXYZ(double nm, double& x, double& y, double& z) {
    x = 1.056 * lobe(nm, 599.8, 37.9, 31.0) + 0.362 * lobe(nm, 442.0, 16.0, 26.7) -
        0.065 * lobe(nm, 501.1, 20.4, 26.2);
    y = 0.821 * lobe(nm, 568.8, 46.9, 40.5) + 0.286 * lobe(nm, 530.9, 16.3, 31.1);
    z = 1.217 * lobe(nm, 437.0, 11.8, 36.0) + 0.681 * lobe(nm, 459.0, 26.0, 13.8);
}

std::vector<std::string> splitCsv(const std::string& line) {
    std::vector<std::string> out;
    std::stringstream ss(line);
    std::string field;
    while (std::getline(ss, field, ',')) out.push_back(field);
    if (!line.empty() && line.back() == ',') out.emplace_back();
    return out;
}
}  // namespace

double temperatureFromBV(double bv) {
    return 4600.0 * (1.0 / (0.92 * bv + 1.7) + 1.0 / (0.92 * bv + 0.62));
}

void blackbodyColor(double kelvin, float rgb[3]) {
    // プランクの放射則を可視域で積分して XYZ にする
    constexpr double c2 = 1.4388e7;  // 第2放射定数 [nm K]
    double X = 0, Y = 0, Z = 0;
    for (double nm = 380.0; nm <= 780.0; nm += 5.0) {
        const double b = 1.0 / (std::pow(nm, 5.0) * (std::exp(c2 / (nm * kelvin)) - 1.0));
        double x, y, z;
        cieXYZ(nm, x, y, z);
        X += b * x;
        Y += b * y;
        Z += b * z;
    }
    // XYZ -> 線形 sRGB (D65)
    double r = 3.2406 * X - 1.5372 * Y - 0.4986 * Z;
    double g = -0.9689 * X + 1.8758 * Y + 0.0415 * Z;
    double b = 0.0557 * X - 0.2040 * Y + 1.0570 * Z;
    r = std::max(r, 0.0);
    g = std::max(g, 0.0);
    b = std::max(b, 0.0);
    const double m = std::max({r, g, b});
    rgb[0] = static_cast<float>(r / m);
    rgb[1] = static_cast<float>(g / m);
    rgb[2] = static_cast<float>(b / m);
}

std::vector<StarVertex> loadStarCatalog(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open " + path + " (run: python scripts/fetch_hyg.py)");

    std::vector<StarVertex> stars;
    std::string line;
    bool header = true;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (header) {  // hip,ra_deg,dec_deg,mag,ci,con,proper
            header = false;
            continue;
        }
        const auto f = splitCsv(line);
        if (f.size() < 5) continue;
        const auto v = astro::equatorialVector(std::stod(f[1]), std::stod(f[2]));
        StarVertex s{};
        s.dir[0] = static_cast<float>(v.x);
        s.dir[1] = static_cast<float>(v.y);
        s.dir[2] = static_cast<float>(v.z);
        s.mag = std::stof(f[3]);
        if (f[4].empty()) {
            s.color[0] = s.color[1] = s.color[2] = 1.0f;
        } else {
            blackbodyColor(temperatureFromBV(std::stod(f[4])), s.color);
        }
        stars.push_back(s);
    }
    if (stars.empty()) throw std::runtime_error("no stars in " + path);
    return stars;
}
