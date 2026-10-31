#include "line_renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "gl_util.hpp"

namespace {
void setupVao(GLuint vao, GLuint vbo) {
    glVertexArrayVertexBuffer(vao, 0, vbo, 0, sizeof(LineVertex));
    const struct {
        GLuint loc;
        GLint size;
        size_t offset;
    } attribs[] = {{0, 3, offsetof(LineVertex, dir)},
                   {1, 1, offsetof(LineVertex, anchor)},
                   {2, 4, offsetof(LineVertex, color)}};
    for (const auto& a : attribs) {
        glEnableVertexArrayAttrib(vao, a.loc);
        glVertexArrayAttribFormat(vao, a.loc, a.size, GL_FLOAT, GL_FALSE, static_cast<GLuint>(a.offset));
        glVertexArrayAttribBinding(vao, a.loc, 0);
    }
}
}  // namespace

void appendArc(std::vector<LineVertex>& out, const astro::Vec3& a, const astro::Vec3& b, float anchor,
               const float colorA[4], const float colorB[4], double stepDeg) {
    const double dot = std::clamp(a.x * b.x + a.y * b.y + a.z * b.z, -1.0, 1.0);
    const double angle = std::acos(dot);
    const int n = std::max(1, static_cast<int>(std::ceil(angle / (stepDeg * astro::kDeg))));
    auto point = [&](int i) {
        // 球面線形補間（slerp）
        const double t = double(i) / n;
        const double s = std::sin(angle);
        const double wa = s < 1e-9 ? 1.0 - t : std::sin((1.0 - t) * angle) / s;
        const double wb = s < 1e-9 ? t : std::sin(t * angle) / s;
        LineVertex v{};
        v.dir[0] = float(wa * a.x + wb * b.x);
        v.dir[1] = float(wa * a.y + wb * b.y);
        v.dir[2] = float(wa * a.z + wb * b.z);
        v.anchor = anchor;
        for (int k = 0; k < 4; ++k) v.color[k] = float(colorA[k] + (colorB[k] - colorA[k]) * t);
        return v;
    };
    for (int i = 0; i < n; ++i) {
        out.push_back(point(i));
        out.push_back(point(i + 1));
    }
}

LineRenderer::LineRenderer(const std::string& root) {
    const std::string dir = root + "/shaders/";
    program_ = buildProgram(dir + "lines.vert", dir + "lines.frag", dir + "projection.glsl", dir + "lines.geom");
    glCreateVertexArrays(1, &staticVao_);
    glCreateVertexArrays(1, &dynamicVao_);
    glCreateBuffers(1, &staticVbo_);
    glCreateBuffers(1, &dynamicVbo_);
    setupVao(staticVao_, staticVbo_);
    setupVao(dynamicVao_, dynamicVbo_);
}

LineRenderer::~LineRenderer() {
    glDeleteProgram(program_);
    glDeleteVertexArrays(1, &staticVao_);
    glDeleteVertexArrays(1, &dynamicVao_);
    glDeleteBuffers(1, &staticVbo_);
    glDeleteBuffers(1, &dynamicVbo_);
}

void LineRenderer::setStatic(const std::vector<LineVertex>& vertices) {
    glNamedBufferData(staticVbo_, static_cast<GLsizeiptr>(vertices.size() * sizeof(LineVertex)), vertices.data(),
                      GL_STATIC_DRAW);
    staticCount_ = static_cast<GLsizei>(vertices.size());
}

void LineRenderer::drawStatic(int width, int height, float widthPx, float alpha, const float color[3]) {
    draw(staticVao_, staticCount_, width, height, widthPx, alpha, color);
}

void LineRenderer::drawDynamic(const std::vector<LineVertex>& vertices, int width, int height, float widthPx,
                               float alpha) {
    if (vertices.empty()) return;
    const size_t bytes = vertices.size() * sizeof(LineVertex);
    if (bytes > dynamicCapacity_) {
        dynamicCapacity_ = bytes * 2;
        glNamedBufferData(dynamicVbo_, static_cast<GLsizeiptr>(dynamicCapacity_), nullptr, GL_DYNAMIC_DRAW);
    }
    glNamedBufferSubData(dynamicVbo_, 0, static_cast<GLsizeiptr>(bytes), vertices.data());
    const float white[3] = {1.0f, 1.0f, 1.0f};
    draw(dynamicVao_, static_cast<GLsizei>(vertices.size()), width, height, widthPx, alpha, white);
}

void LineRenderer::draw(GLuint vao, GLsizei count, int width, int height, float widthPx, float alpha,
                        const float color[3]) {
    if (count == 0) return;
    const float viewport[2] = {float(width), float(height)};
    glProgramUniform2fv(program_, glGetUniformLocation(program_, "u_viewport"), 1, viewport);
    glProgramUniform1f(program_, glGetUniformLocation(program_, "u_widthPx"), widthPx);
    glProgramUniform1f(program_, glGetUniformLocation(program_, "u_alpha"), alpha);
    glProgramUniform3fv(program_, glGetUniformLocation(program_, "u_color"), 1, color);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glUseProgram(program_);
    glBindVertexArray(vao);
    glDrawArrays(GL_LINES, 0, count);
    glDisable(GL_BLEND);
}
