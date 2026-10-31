#include "operator_window.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <stdexcept>

#include "gl_util.hpp"

OperatorWindow::OperatorWindow(GLFWwindow* shareWith, const std::string& root, const std::string& fontPath) {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    window_ = glfwCreateWindow(960, 720, "操作パネル - Projector Planetarium", nullptr, shareWith);
    if (!window_) throw std::runtime_error("cannot create the operator window");
    // PC 本体の画面（主モニタ）の左上に出す
    int mx = 0, my = 0;
    glfwGetMonitorPos(glfwGetPrimaryMonitor(), &mx, &my);
    glfwSetWindowPos(window_, mx + 40, my + 60);

    GLFWwindow* previous = glfwGetCurrentContext();
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(0);  // 出力ウィンドウの垂直同期だけで速度が決まるように
    // VAO はコンテキストごとに作る必要がある（テクスチャとシェーダは共有される）
    const std::string dir = root + "/shaders/";
    program_ = buildProgram(dir + "fullscreen.vert", dir + "preview.frag");
    glCreateVertexArrays(1, &vao_);
    text_ = std::make_unique<TextRenderer>(root, fontPath);
    if (text_->ok()) {
        const float identity[4] = {1, 0, 0, 1};
        glProgramUniformMatrix2fv(text_->program(), glGetUniformLocation(text_->program(), "u_screen"), 1, GL_FALSE,
                                  identity);
    }
    glfwMakeContextCurrent(previous);
}

OperatorWindow::~OperatorWindow() {
    GLFWwindow* previous = glfwGetCurrentContext();
    glfwMakeContextCurrent(window_);
    text_.reset();
    glDeleteProgram(program_);
    glDeleteVertexArrays(1, &vao_);
    glfwMakeContextCurrent(previous);
    glfwDestroyWindow(window_);
}

void OperatorWindow::framebufferSize(int& width, int& height) const { glfwGetFramebufferSize(window_, &width, &height); }

void OperatorWindow::render(GLuint outputTexture, int outputWidth, int outputHeight,
                            const std::vector<std::string>& lines) {
    glfwMakeContextCurrent(window_);
    int w, h;
    framebufferSize(w, h);
    if (w <= 0 || h <= 0) return;
    glViewport(0, 0, w, h);
    glClearColor(0.07f, 0.08f, 0.10f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // 上にプレビュー（出力の縦横比のまま、高さは最大 60%）
    const int margin = 16;
    int pw = w - 2 * margin;
    int ph = outputWidth > 0 ? pw * outputHeight / outputWidth : 0;
    if (ph > h * 6 / 10) {
        ph = h * 6 / 10;
        pw = ph * outputWidth / std::max(outputHeight, 1);
    }
    if (outputTexture && pw > 0 && ph > 0) {
        glViewport(margin, h - margin - ph, pw, ph);
        glProgramUniform1i(program_, glGetUniformLocation(program_, "u_image"), 0);
        glBindTextureUnit(0, outputTexture);
        glUseProgram(program_);
        glBindVertexArray(vao_);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glViewport(0, 0, w, h);
    }

    // 下に状態とキー一覧（左上を基準に 1 行ずつ）
    if (text_->ok()) {
        const astro::Vec3 topLeft{-1.0, 1.0, 0.0};
        const float white[4] = {0.88f, 0.90f, 0.95f, 1.0f}, gray[4] = {0.60f, 0.64f, 0.72f, 1.0f};
        float y = -(margin + ph + 30.0f);
        for (const auto& line : lines) {
            const bool heading = !line.empty() && line[0] == '#';
            text_->add(TextRenderer::Screen, topLeft, heading ? line.substr(1) : line, 18.0f, heading ? white : gray,
                       TextRenderer::Left, float(margin), y);
            y -= 26.0f;
        }
        text_->flush(w, h);
    }
}

void OperatorWindow::present(GLFWwindow* restore) {
    glfwSwapBuffers(window_);
    glfwMakeContextCurrent(restore);
}
