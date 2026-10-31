#include "audio.hpp"

#include <algorithm>
#include <cstdio>

#include "miniaudio.h"

struct AudioPlayer::Impl {
    ma_engine engine{};
    ma_sound bgm{}, narration{};
    bool hasBgm = false, hasNarration = false;
};

AudioPlayer::AudioPlayer() : impl_(std::make_unique<Impl>()) {
    if (ma_engine_init(nullptr, &impl_->engine) != MA_SUCCESS) {
        std::fprintf(stderr, "[audio] no audio device; sound is disabled\n");
        return;
    }
    ok_ = true;
}

AudioPlayer::~AudioPlayer() {
    if (!ok_) return;
    stopNarration();
    stopBgm();
    ma_engine_uninit(&impl_->engine);
}

void AudioPlayer::playBgm(const std::string& path, float volume, float duck) {
    if (!ok_) return;
    stopBgm();
    if (ma_sound_init_from_file(&impl_->engine, path.c_str(), MA_SOUND_FLAG_STREAM, nullptr, nullptr, &impl_->bgm) !=
        MA_SUCCESS) {
        std::fprintf(stderr, "[audio] cannot open %s\n", path.c_str());
        return;
    }
    impl_->hasBgm = true;
    bgmVolume_ = volume;
    bgmDuck_ = duck;
    bgmCurrent_ = 0.0f;  // 0 からゆっくり上げる（フェードイン）
    ma_sound_set_looping(&impl_->bgm, MA_TRUE);
    ma_sound_set_volume(&impl_->bgm, 0.0f);
    ma_sound_start(&impl_->bgm);
    std::printf("[audio] bgm %s\n", path.c_str());
}

void AudioPlayer::stopBgm() {
    if (!impl_->hasBgm) return;
    ma_sound_uninit(&impl_->bgm);
    impl_->hasBgm = false;
}

void AudioPlayer::playNarration(const std::string& path) {
    if (!ok_) return;
    stopNarration();
    if (ma_sound_init_from_file(&impl_->engine, path.c_str(), 0, nullptr, nullptr, &impl_->narration) != MA_SUCCESS) {
        std::fprintf(stderr, "[audio] cannot open %s\n", path.c_str());
        return;
    }
    impl_->hasNarration = true;
    ma_sound_set_volume(&impl_->narration, muted_ ? 0.0f : 1.0f);
    ma_sound_start(&impl_->narration);
    std::printf("[audio] narration %s\n", path.c_str());
}

void AudioPlayer::stopNarration() {
    if (!impl_->hasNarration) return;
    ma_sound_uninit(&impl_->narration);
    impl_->hasNarration = false;
}

bool AudioPlayer::narrating() const {
    return impl_->hasNarration && ma_sound_is_playing(&impl_->narration);
}

void AudioPlayer::setMuted(bool muted) {
    muted_ = muted;
    if (impl_->hasNarration) ma_sound_set_volume(&impl_->narration, muted ? 0.0f : 1.0f);
}

void AudioPlayer::setPaused(bool paused) {
    if (!ok_ || paused == paused_) return;
    paused_ = paused;
    // ma_sound_stop は再生位置を保つので、ma_sound_start で続きから鳴る。
    // 止めた時点で終わっていたナレーションは、もう一度鳴らさない（終わった音を start すると頭から鳴るため）
    if (paused) {
        narrationWasPlaying_ = narrating();
        if (impl_->hasBgm) ma_sound_stop(&impl_->bgm);
        if (narrationWasPlaying_) ma_sound_stop(&impl_->narration);
    } else {
        if (impl_->hasBgm) ma_sound_start(&impl_->bgm);
        if (narrationWasPlaying_ && impl_->hasNarration) ma_sound_start(&impl_->narration);
        narrationWasPlaying_ = false;
    }
}

void AudioPlayer::update(double dt) {
    if (!impl_->hasBgm || paused_) return;
    // ナレーション中は BGM を bgmDuck_ 倍に下げる。上げ下げは 1 秒で音量の 1.5 倍分まで（急に変わらないように）
    const float target = muted_ ? 0.0f : bgmVolume_ * (narrating() ? bgmDuck_ : 1.0f);
    const float step = static_cast<float>(1.5 * bgmVolume_ * dt);
    bgmCurrent_ += std::clamp(target - bgmCurrent_, -step, step);
    ma_sound_set_volume(&impl_->bgm, bgmCurrent_);
}
