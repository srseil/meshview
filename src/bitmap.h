#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>

class Bitmap {
public:
    Bitmap() : width(0), height(0), depth(0) {}
    explicit Bitmap(std::string_view fileName);
    Bitmap(int width, int height, int depth);

    Bitmap(const Bitmap&) = delete;
    Bitmap& operator=(const Bitmap&) = delete;

    Bitmap(Bitmap&& other) noexcept;
    Bitmap& operator=(Bitmap&& other) noexcept;

    int getWidth() const { return width; }
    int getHeight() const { return height; }
    int getDepth() const { return depth; }
    const float* getData() const { return data.data(); }
    glm::vec3 getPixel(int x, int y, int z) const;
    void setPixel(int x, int y, int z, const glm::vec3& color);

    static Bitmap convertEquirectangularMapToVerticalCross(const Bitmap& bitmap);
    static Bitmap convertVerticalCrossToCubeMapFaces(const Bitmap& bitmap);
    static Bitmap convertDiffuseToIrradiance(const Bitmap& input, int srcW, int srcH, int dstW, int dstH, int numMonteCarloSamples);
private:
    int width;
    int height;
    int depth;
    std::vector<float> data;
};
