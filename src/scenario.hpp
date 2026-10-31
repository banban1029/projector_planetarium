#pragma once
#include <string>
#include <vector>

// シナリオの 1 場面。空欄・負の値の項目は「変更しない」
struct ScenarioStep {
    double duration_sec = 10.0;  // この場面を見せる実時間
    std::string location;        // 観測地の id
    std::string time;            // "YYYY-MM-DD HH:MM"（地方時）
    double speed = -1.0;         // 時間倍率
    std::string mode;            // 表示モードの id
    std::string message;         // 画面に表示する文字
    int constellations = -1;     // 星座線 0/1
    int labels = -1;             // 星・惑星・月の名前 0/1
    int constellationNames = -1; // 星座名 0/1
    // 上映用（場面ごとに指定。書かなければ消える／既定に戻る）
    std::string subtitle;                // 画面下の字幕（\n で改行）
    std::string narration;               // 読み上げる文（空なら字幕を読む）
    std::vector<std::string> asterisms;  // 季節の目印の id（data/asterisms.csv）
    std::vector<std::string> point;      // 光る円で指す星・惑星の名前
    double fadeSec = -1.0;               // この場面に入るときのフェードの秒数（既定 1 秒）
    double eyeAdaptSec = -1.0;           // 目が暗さに慣れる演出の秒数
    double meteorRate = -1.0;            // 流れ星の数 [個/秒]（負ならモードの既定）
    std::vector<double> meteorRadiant;   // 流れ星の放射点 [赤経, 赤緯]（度）
    int credits = -1;                    // クレジット表示 0/1
    int compass = -1;                    // 方位の文字（北・東・南・西） 0/1
};

// 上映の音（scripts/make_show_audio.ps1 が作るファイルの場所と、読み上げの声）
struct ScenarioAudio {
    std::string narrationDir;  // step_01.wav … を置くフォルダ（リポジトリからの相対パス）
    std::string voice;         // Windows の音声合成の声の名前
    int rate = 0;              // 話す速さ（-10〜10）
    std::string bgm;           // 繰り返し流す BGM（WAV）
    double bgmVolume = 0.3;
    double bgmDuck = 0.4;      // ナレーション中の BGM の音量（bgmVolume に掛ける）
    double narrationGain = 1.0;  // ナレーションを作るときに掛ける音量（割れないようリミッタをかける）
};

struct Scenario {
    bool loop = true;
    std::string title;
    // 上映の日時と本編の観測地。場面では time: start / "start+5:00"、location: main で参照する
    std::string date;
    std::string location;
    ScenarioAudio audio;
    std::vector<ScenarioStep> steps;
};

// 場面の読み上げ文（narration、無ければ字幕。改行は除く）
std::string narrationText(const ScenarioStep& step);

// 実時間でシナリオを進める。場面が切り替わった瞬間だけ、その場面を返す
class ScenarioPlayer {
public:
    explicit ScenarioPlayer(Scenario scenario) : scenario_(std::move(scenario)) {}

    // 再生開始（先頭の場面を返す）。場面が無ければ nullptr
    const ScenarioStep* start();
    void stop() { playing_ = paused_ = false; }
    // 一時停止中は場面が進まない（再開すると続きから）
    void setPaused(bool paused) { paused_ = paused && playing_; }
    // 次の場面に切り替わったらその場面、それ以外は nullptr。最後まで行き loop でなければ停止
    const ScenarioStep* update(double realDt);

    bool playing() const { return playing_; }
    bool paused() const { return paused_; }
    const Scenario& scenario() const { return scenario_; }
    size_t index() const { return index_; }
    size_t size() const { return scenario_.steps.size(); }

private:
    Scenario scenario_;
    bool playing_ = false;
    bool paused_ = false;
    size_t index_ = 0;
    double elapsed_ = 0.0;
};
