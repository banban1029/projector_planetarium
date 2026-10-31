#include "effects.hpp"

#include <algorithm>
#include <cmath>

namespace {
using astro::Vec3;

Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
double dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 normalize(const Vec3& v) {
    const double l = std::sqrt(dot(v, v));
    return {v.x / l, v.y / l, v.z / l};
}
Vec3 add(const Vec3& a, const Vec3& b, double s) { return {a.x + b.x * s, a.y + b.y * s, a.z + b.z * s}; }
Vec3 scale(const Vec3& v, double s) { return {v.x * s, v.y * s, v.z * s}; }

// v を単位ベクトル axis まわりに angle 回す（ロドリゲスの回転公式。v ⊥ axis を仮定）
Vec3 rotate(const Vec3& v, const Vec3& axis, double angle) {
    const Vec3 c = cross(axis, v);
    return {v.x * std::cos(angle) + c.x * std::sin(angle), v.y * std::cos(angle) + c.y * std::sin(angle),
            v.z * std::cos(angle) + c.z * std::sin(angle)};
}

Vec3 fromAltAz(double altDeg, double azDeg) {
    const double a = altDeg * astro::kDeg, z = azDeg * astro::kDeg;
    return {std::cos(a) * std::cos(z), std::cos(a) * std::sin(z), std::sin(a)};
}
}  // namespace

void MeteorShower::update(double realDt, double rate, const Vec3& radiant) {
    for (auto& m : meteors_) m.age += realDt;
    meteors_.erase(std::remove_if(meteors_.begin(), meteors_.end(), [](const Meteor& m) { return m.age > m.life; }),
                   meteors_.end());
    if (rate <= 0.0) return;

    std::uniform_real_distribution<double> u(0.0, 1.0);
    // dt の間に生まれる数はポアソン分布（平均 rate × dt）
    const int count = std::poisson_distribution<int>(realDt * rate)(rng_);
    for (int k = 0; k < count; ++k) {
        Meteor m{};
        m.start = fromAltAz(20.0 + 60.0 * u(rng_), 360.0 * u(rng_));
        const bool hasRadiant = dot(radiant, radiant) > 0.5;
        // 放射点があれば「放射点 → 出発点」の大円の向きに飛ぶ。無ければランダムな向き
        Vec3 axis = hasRadiant ? cross(radiant, m.start) : cross(m.start, fromAltAz(90.0 * u(rng_), 360.0 * u(rng_)));
        if (dot(axis, axis) < 1e-8) continue;
        m.axis = normalize(axis);
        m.lengthRad = (8.0 + 20.0 * u(rng_)) * astro::kDeg;
        m.life = 0.5 + 0.8 * u(rng_);
        m.brightness = static_cast<float>(1.0 + 2.0 * u(rng_) * u(rng_));
        meteors_.push_back(m);
    }
}

void MeteorShower::appendLines(std::vector<LineVertex>& out) const {
    for (const auto& m : meteors_) {
        const double t = m.age / m.life;
        const double head = m.lengthRad * t;
        const double tail = std::max(0.0, head - m.lengthRad * 0.35);
        // 出て、明るくなり、消える
        const float a = static_cast<float>(std::sin(t * astro::kPi)) * m.brightness;
        const float tailColor[4] = {0.6f, 0.7f, 1.0f, 0.0f};
        const float headColor[4] = {1.0f, 1.0f, 0.9f, a};
        appendArc(out, rotate(m.start, m.axis, tail), rotate(m.start, m.axis, head), 1.0f, tailColor, headColor, 1.0);
    }
}

void appendWarpStreaks(std::vector<LineVertex>& out, const std::vector<StarVertex>& stars,
                       const astro::Mat3& j2000ToHorizon, const Vec3& viewForward, double halfFovRad,
                       double timeSec) {
    // 視線 f に垂直な基底 e1, e2。星の「視線まわりの角度」は実際の位置から取り、中心からの距離を時間で動かす
    const Vec3 f = normalize(viewForward);
    const Vec3 e1 = normalize(std::abs(f.z) < 0.9 ? cross(f, Vec3{0, 0, 1}) : cross(f, Vec3{1, 0, 0}));
    const Vec3 e2 = cross(f, e1);
    for (size_t i = 0; i < stars.size(); ++i) {
        const auto& s = stars[i];
        if (s.mag > 5.0f) continue;
        const Vec3 h = j2000ToHorizon * Vec3{s.dir[0], s.dir[1], s.dir[2]};
        const double phi = std::atan2(dot(h, e2), dot(h, e1));
        const double seed = std::fmod(i * 0.6180339887, 1.0);
        const double phase = std::fmod(timeSec * 0.35 + seed, 1.0);
        const double theta = halfFovRad * phase * phase;  // 中心付近はゆっくり、外へ行くほど速く
        const double tail = theta * 0.8;
        const Vec3 radial = add(scale(e1, std::cos(phi)), e2, std::sin(phi));
        auto dirAt = [&](double th) { return normalize(add(scale(f, std::cos(th)), radial, std::sin(th))); };
        const float fade = static_cast<float>(std::min(1.0, phase * 4.0) * std::pow(10.0, -0.25 * (s.mag - 1.0)));
        const float tailColor[4] = {s.color[0], s.color[1], s.color[2], 0.0f};
        const float headColor[4] = {s.color[0], s.color[1], s.color[2], std::min(fade, 1.5f)};
        appendArc(out, dirAt(tail), dirAt(theta), 1.0f, tailColor, headColor, 3.0);
    }
}
