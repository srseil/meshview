#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>

#include "gl/gl.h"

class Cubemap {
public:
    Cubemap(std::string fileName);
    ~Cubemap();
    GLuint getHandleDiffuse() const { return handleDiffuse; }
    GLuint getHandleIrradiance() const { return handleDiffuse; }
    void bind() const;
private:
    GLuint handleDiffuse;
    GLuint handleIrradiance;
};

class Bitmap {
public:
    Bitmap() : width(0), height(0), depth(0) {}
    Bitmap(std::string fileName);
    Bitmap(int width, int height, int depth);
    int getWidth() const { return width; }
    int getHeight() const { return height; }
    int getDepth() const { return depth; }
    const float* getData() const { return data.data(); }
    glm::vec3 getPixel(int x, int y, int z) const;
    void setPixel(int x, int y, int z, const glm::vec3& color);
private:
    int width;
    int height;
    int depth;
    std::vector<float> data;
};
