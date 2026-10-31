#pragma once
#include <GL/glew.h>

#include <memory>
#include <string>
#include <vector>

#include "text_renderer.hpp"

struct GLFWwindow;

// 操作する人のための 2 つ目のウィンドウ（PC の画面に出す）。
// プロジェクタに出している画像の縮小プレビューと、今の状態・キー一覧を表示する。
// 出力ウィンドウとコンテキストを共有するので、出力テクスチャをそのまま表示できる。
class OperatorWindow {
public:
    OperatorWindow(GLFWwindow* shareWith, const std::string& root, const std::string& fontPath);
    ~OperatorWindow();
    OperatorWindow(const OperatorWindow&) = delete;
    OperatorWindow& operator=(const OperatorWindow&) = delete;

    GLFWwindow* window() const { return window_; }

    // このウィンドウのコンテキストに切り替えて描く（画面への表示は present()）
    void render(GLuint outputTexture, int outputWidth, int outputHeight, const std::vector<std::string>& lines);
    void framebufferSize(int& width, int& height) const;
    // 画面に表示し、restore のコンテキストに戻す
    void present(GLFWwindow* restore);

private:
    GLFWwindow* window_ = nullptr;
    std::unique_ptr<TextRenderer> text_;
    GLuint program_ = 0, vao_ = 0;
};
