#include "gl_util.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

std::string readTextFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open " + path);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

namespace {
GLuint compile(GLenum type, const std::string& path, const std::string& common) {
    std::string src = readTextFile(path);
    if (!common.empty()) {
        const auto eol = src.find('\n');
        src.insert(eol + 1, common + "\n#line 2\n");
    }
    const GLuint shader = glCreateShader(type);
    const char* p = src.c_str();
    glShaderSource(shader, 1, &p, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        glDeleteShader(shader);
        throw std::runtime_error("shader compile failed: " + path + "\n" + log);
    }
    return shader;
}
}  // namespace

GLuint buildProgram(const std::string& vertPath, const std::string& fragPath, const std::string& commonPath) {
    const std::string common = commonPath.empty() ? "" : readTextFile(commonPath);
    const GLuint vs = compile(GL_VERTEX_SHADER, vertPath, common);
    const GLuint fs = compile(GL_FRAGMENT_SHADER, fragPath, common);
    const GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        glDeleteProgram(prog);
        throw std::runtime_error("program link failed: " + vertPath + "\n" + log);
    }
    return prog;
}
