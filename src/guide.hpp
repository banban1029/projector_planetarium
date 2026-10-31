#pragma once
#include <string>
#include <unordered_map>
#include <vector>

#include "astro.hpp"
#include "bodies.hpp"
#include "line_renderer.hpp"

// 上映の案内: 季節の目印（アステリズム）の線と、星を指し示す円。
// 星は名前（英語の proper 名、または data/star_names.csv の日本語名）、惑星は日本語・英語名で指定する
class SkyGuide {
public:
    // data/stars.csv・data/star_names.csv・data/asterisms.csv を読む。失敗時は std::runtime_error
    explicit SkyGuide(const std::string& dataDir);

    struct Asterism {
        std::string id, nameJa;
        std::vector<std::vector<astro::Vec3>> lines;  // J2000 方向の折れ線
        astro::Vec3 center;                           // 名前を置く位置
    };
    const Asterism* asterism(const std::string& id) const;

    // 名前 → J2000 方向と表示名。見つからなければ false
    bool find(const std::string& name, const std::vector<BodyInfo>& bodies, astro::Vec3& dir,
              std::string& display) const;

    // 目印の線（progress 0→1 で線が順に伸びる）
    void appendAsterism(std::vector<LineVertex>& out, const Asterism& a, double progress, float alpha) const;
    // dir を囲む円（radiusDeg）
    void appendPointer(std::vector<LineVertex>& out, const astro::Vec3& dir, double radiusDeg, float alpha) const;

private:
    std::unordered_map<std::string, astro::Vec3> stars_;      // 英語名・日本語名 → 方向
    std::unordered_map<std::string, std::string> japanese_;  // 英語名 → 日本語名
    std::vector<Asterism> asterisms_;
};
