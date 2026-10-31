// miniaudio の実装を 1 か所でコンパイルする（ヘッダオンリーライブラリの作法）。
// 使うのは音声ファイル（WAV）の再生だけなので、録音・エンコードなどは外す
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
