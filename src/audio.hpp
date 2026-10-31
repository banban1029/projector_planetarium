#pragma once
#include <memory>
#include <string>

// 上映の音: 繰り返し流す BGM と、場面ごとのナレーション（WAV）。
// ナレーションが流れている間は BGM の音量を自動で下げる（ダッキング）。miniaudio を使う
class AudioPlayer {
public:
    AudioPlayer();
    ~AudioPlayer();
    AudioPlayer(const AudioPlayer&) = delete;
    AudioPlayer& operator=(const AudioPlayer&) = delete;

    bool ok() const { return ok_; }

    // 繰り返し再生。duck はナレーション中の音量の割合
    void playBgm(const std::string& path, float volume, float duck = 0.4f);
    void stopBgm();
    void playNarration(const std::string& path);           // 前のナレーションは止める
    void stopNarration();
    bool narrating() const;
    void setMuted(bool muted);
    bool muted() const { return muted_; }

    // 毎フレーム呼ぶ（BGM の音量をなめらかに上げ下げする）
    void update(double dt);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool ok_ = false;
    bool muted_ = false;
    float bgmVolume_ = 0.3f, bgmDuck_ = 0.4f, bgmCurrent_ = 0.0f;
};
