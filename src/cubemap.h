#pragma once

#include <string>

#include "gl/gl.h"

enum class CubemapFace : int {
    PositiveX = 0,
    NegativeX,
    PositiveY,
    NegativeY,
    PositiveZ,
    NegativeZ
};

class Cubemap {
public:
    explicit Cubemap(std::string_view fileName);
    ~Cubemap();

    Cubemap(const Cubemap&) = delete;
    Cubemap& operator=(const Cubemap&) = delete;

    Cubemap(Cubemap&& other) noexcept;
    Cubemap& operator=(Cubemap&& other) noexcept;

    GLuint getHandleDiffuse() const { return handleDiffuse; }
    GLuint getHandleIrradiance() const { return handleIrradiance; }

    void bind() const;

    static glm::vec3 faceCoordsToXYZ(int x, int y, CubemapFace face, int faceSize);
private:
    GLuint handleDiffuse;
    GLuint handleIrradiance;
};
