#pragma once

#include "GL.h"

class GLShader {
public:
    GLShader(const std::string fileName);
    ~GLShader();
    GLenum getType() const { return type; }
    GLuint getHandle() const { return handle; }
private:
    GLenum type;
    GLuint handle;
};

class GLProgram {
public:
    GLProgram(const GLShader& a, const GLShader& b);
    ~GLProgram();
    void useProgram() const;
    GLuint getHandle() const { return handle; }
private:
    GLuint handle;
};
