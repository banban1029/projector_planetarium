#pragma once
#include <random>
#include <vector>

#include "astro.hpp"
#include "line_renderer.hpp"
#include "star_catalog.hpp"

// 流れ星。地平座標で生まれ、放射点から離れる向きに大円に沿って飛ぶ
class MeteorShower {
public:
    // rate [個/秒] で発生させ、寿命の尽きたものを消す。radiant は放射点（地平座標、長さ 0 ならランダムな向き）
    void update(double realDt, double rate, const astro::Vec3& radiant);
    // 尾（暗い）→ 頭（明るい）のグラデーションの線を足す
    void appendLines(std::vector<LineVertex>& out) const;

private:
    struct Meteor {
        astro::Vec3 start, axis;  // 出発点と、回転軸（start を axis まわりに回すと進む）
        double lengthRad, age, life;
        float brightness;
    };
    std::vector<Meteor> meteors_;
    std::mt19937 rng_{12345};
};

// ワープ航行: 明るい星を、視線の中心から外へ流れる光跡にする（地平座標の線）
void appendWarpStreaks(std::vector<LineVertex>& out, const std::vector<StarVertex>& stars,
                       const astro::Mat3& j2000ToHorizon, const astro::Vec3& viewForward, double halfFovRad,
                       double timeSec);
