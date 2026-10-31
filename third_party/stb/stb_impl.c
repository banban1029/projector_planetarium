// stb の実装を 1 か所でコンパイルする（ヘッダオンリーライブラリの作法）
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
