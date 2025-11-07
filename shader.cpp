#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

#include "shader.h"

static GLenum glShaderTypeFromFileName(std::string fileName);

GLShader::GLShader(const std::string fileName) : type(glShaderTypeFromFileName(fileName)), handle(api.glCreateShader(type))
{
    std::ifstream stream(fileName);
    if (!stream) {
        std::cerr << "Cannot open file " << fileName << std::endl;
        exit(EXIT_FAILURE);
    }
    std::stringstream buffer;
    buffer << stream.rdbuf();
    std::string text = buffer.str();
    const char* textCharPtr = text.c_str();
    api.glShaderSource(handle, 1, &textCharPtr, nullptr);
    api.glCompileShader(handle);
    GLint success = false;
    api.glGetShaderiv(handle, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[8192];
        api.glGetShaderInfoLog(handle, sizeof(infoLog), nullptr, infoLog);
        std::cerr << "Shader compilation failed:" << std::endl << infoLog << std::endl;
        exit(EXIT_FAILURE);
    }
}

GLShader::~GLShader()
{
    api.glDeleteShader(handle);
}

GLProgram::GLProgram(const GLShader& a, const GLShader& b) : handle(api.glCreateProgram())
{
    api.glAttachShader(handle, a.getHandle());
    api.glAttachShader(handle, b.getHandle());
    api.glLinkProgram(handle);
    GLint success = false;
    api.glGetProgramiv(handle, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[8192];
        api.glGetProgramInfoLog(handle, sizeof(infoLog), NULL, infoLog);
        std::cerr << "Shader program linking failed: " << std::endl << infoLog << std::endl;
        exit(EXIT_FAILURE);
    }
}

GLProgram::~GLProgram()
{
    api.glDeleteProgram(handle);
}

void GLProgram::useProgram() const
{
    api.glUseProgram(handle);
}

static GLenum glShaderTypeFromFileName(std::string fileName)
{
    if (fileName.ends_with(".vert")) {
        return GL_VERTEX_SHADER;
    }
    else if (fileName.ends_with(".frag")) {
        return GL_FRAGMENT_SHADER;
    }
    else {
        throw std::invalid_argument("unsupported shader type");
    }
}
