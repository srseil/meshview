#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>

#include "GL.h"

class Cubemap {
public:
    Cubemap(std::string fileName);
    ~Cubemap();
    GLuint getHandle() const { return handle; }
    void bindTexture() const;
private:
    GLuint handle;
};

class Bitmap {
public:
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
