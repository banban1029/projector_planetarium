#pragma once
#include <GL/glew.h>

#include <string>
#include <vector>

#include "astro.hpp"

struct LineVertex {
    float dir[3];
    float anchor;  // 0 = J2000 赤道座標, 1 = 地平座標
    float color[4];
};

// 大円に沿って a -> b を分割した線分を out に足す（魚眼で曲がって見えるように）。2 頂点 = 1 線分
void appendArc(std::vector<LineVertex>& out, const astro::Vec3& a, const astro::Vec3& b, float anchor,
               const float colorA[4], const float colorB[4], double stepDeg = 1.5);

// 太さのある線を描く（ジオメトリシェーダで線分を四角形に広げる）
class LineRenderer {
public:
    explicit LineRenderer(const std::string& root);
    ~LineRenderer();
    LineRenderer(const LineRenderer&) = delete;
    LineRenderer& operator=(const LineRenderer&) = delete;

    GLuint program() const { return program_; }
    void setStatic(const std::vector<LineVertex>& vertices);  // 星座線など変わらない線
    // 投影の uniform は呼び出し側で program() に設定しておく
    void drawStatic(int width, int height, float widthPx, float alpha, const float color[3]);
    void drawDynamic(const std::vector<LineVertex>& vertices, int width, int height, float widthPx, float alpha);

private:
    void draw(GLuint vao, GLsizei count, int width, int height, float widthPx, float alpha, const float color[3]);

    GLuint program_ = 0;
    GLuint staticVao_ = 0, staticVbo_ = 0, dynamicVao_ = 0, dynamicVbo_ = 0;
    GLsizei staticCount_ = 0;
    size_t dynamicCapacity_ = 0;
};
