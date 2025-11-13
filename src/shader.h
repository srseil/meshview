#pragma once

#include "gl/gl.h"

class GLShader {
public:
    explicit GLShader(const std::string_view fileName);
    ~GLShader();

    GLShader(const GLShader&) = delete;
    GLShader& operator=(const GLShader&) = delete;

    GLShader(GLShader&& other) noexcept;
    GLShader& operator=(GLShader&& other) noexcept;

    GLenum getType() const { return type; }
    GLuint getHandle() const { return handle; }
private:
    GLenum type;
    GLuint handle;
};

class GLProgram {
public:
    explicit GLProgram(const GLShader& a, const GLShader& b);
    ~GLProgram();

    GLProgram(const GLProgram&) = delete;
    GLProgram& operator=(const GLProgram&) = delete;

    GLProgram(GLProgram&& other) noexcept;
    GLProgram& operator=(GLProgram&& other) noexcept;

    void useProgram() const;
    GLuint getHandle() const { return handle; }
private:
    GLuint handle;
};
