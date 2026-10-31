#pragma once
#include <GL/glew.h>

#include <string>

std::string readTextFile(const std::string& path);

// common（共通の GLSL 関数）を #version 行の直後に差し込んでからコンパイル・リンクする。
// 失敗時はログ付きで std::runtime_error
GLuint buildProgram(const std::string& vertPath, const std::string& fragPath,
                    const std::string& commonPath = "");
