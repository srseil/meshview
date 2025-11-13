#include <stdexcept>
#include <filesystem>
#include <stb_image.h>
#include <stb_image_write.h>
#include <stb_image_resize2.h>
#include <glm/ext.hpp>

#include "bitmap.h"

#include "cubemap.h"

Cubemap::Cubemap(std::string_view fileName)
{
    Bitmap diffuse(fileName);
    Bitmap diffuseCross = Bitmap::convertEquirectangularMapToVerticalCross(diffuse);
    Bitmap diffuseFaces = Bitmap::convertVerticalCrossToCubeMapFaces(diffuseCross);

    api.glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &handleDiffuse);
    api.glTextureParameteri(handleDiffuse, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    api.glTextureParameteri(handleDiffuse, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    api.glTextureParameteri(handleDiffuse, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
    api.glTextureParameteri(handleDiffuse, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    api.glTextureParameteri(handleDiffuse, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    api.glTextureStorage2D(handleDiffuse, 1, GL_RGB32F, diffuseFaces.getWidth(), diffuseFaces.getHeight());
    const float* diffuseData = diffuseFaces.getData();
    for (unsigned int face = 0; face < 6; face++) {
        api.glTextureSubImage3D(handleDiffuse, 0, 0, 0, face, diffuseFaces.getWidth(), diffuseFaces.getHeight(), 1, GL_RGB, GL_FLOAT, diffuseData);
        diffuseData += diffuseFaces.getWidth() * diffuseFaces.getHeight() * 3;
    }

    Bitmap irradiance;
    std::filesystem::path path(fileName);
    std::string irradianceFileName = path.stem().string() + "_irradiance" + path.extension().string();
    if (std::filesystem::exists(irradianceFileName)) {
        irradiance = Bitmap(irradianceFileName);
    }
    else {
        irradiance = Bitmap::convertDiffuseToIrradiance(diffuse, diffuse.getWidth(), diffuse.getHeight(), 256, 128, 1024);
        stbi_write_hdr(irradianceFileName.c_str(), irradiance.getWidth(), irradiance.getHeight(), 3, irradiance.getData());
    }
    Bitmap irradianceCross = Bitmap::convertEquirectangularMapToVerticalCross(irradiance);
    Bitmap irradianceFaces = Bitmap::convertVerticalCrossToCubeMapFaces(irradianceCross);

    api.glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &handleIrradiance);
    api.glTextureParameteri(handleIrradiance, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    api.glTextureParameteri(handleIrradiance, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    api.glTextureParameteri(handleIrradiance, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
    api.glTextureParameteri(handleIrradiance, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    api.glTextureParameteri(handleIrradiance, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    api.glTextureStorage2D(handleIrradiance, 1, GL_RGB32F, irradianceFaces.getWidth(), irradianceFaces.getHeight());
    const float* irradianceData = irradianceFaces.getData();
    for (unsigned int face = 0; face < 6; face++) {
        api.glTextureSubImage3D(handleIrradiance, 0, 0, 0, face, irradianceFaces.getWidth(), irradianceFaces.getHeight(), 1, GL_RGB, GL_FLOAT, irradianceData);
        irradianceData += irradianceFaces.getWidth() * irradianceFaces.getHeight() * 3;
    }
}

Cubemap::~Cubemap()
{
    api.glDeleteTextures(1, &handleIrradiance);
    api.glDeleteTextures(1, &handleDiffuse);
}

Cubemap::Cubemap(Cubemap&& other) noexcept : handleDiffuse(other.handleDiffuse), handleIrradiance(other.handleIrradiance)
{
    other.handleDiffuse = 0;
    other.handleIrradiance = 0;
}

Cubemap& Cubemap::operator=(Cubemap&& other) noexcept
{
    if (this != &other) {
        if (handleDiffuse) {
            api.glDeleteTextures(1, &handleDiffuse);
        }
        if (handleIrradiance) {
            api.glDeleteTextures(1, &handleIrradiance);
        }
        handleDiffuse = other.handleDiffuse;
        other.handleDiffuse = 0;
        handleIrradiance = other.handleIrradiance;
        other.handleIrradiance = 0;
    }
    return *this;
}

void Cubemap::bind() const
{
    api.glBindTextures(5, 1, &handleDiffuse);
    api.glBindTextures(6, 1, &handleIrradiance);
}

glm::vec3 Cubemap::faceCoordsToXYZ(int x, int y, CubemapFace face, int faceSize)
{
    const float a = 2.0f * float(x) / faceSize;
    const float b = 2.0f * float(y) / faceSize;
    switch (face) {
    case CubemapFace::PositiveX:
        return glm::vec3(-1.0f, a - 1.0f, b - 1.0f);
    case CubemapFace::NegativeX:
        return glm::vec3(a - 1.0f, -1.0f, 1.0f - b);
    case CubemapFace::PositiveY:
        return glm::vec3(1.0f, a - 1.0f, 1.0f - b);
    case CubemapFace::NegativeY:
        return glm::vec3(1.0f - a, 1.0f, 1.0f - b);
    case CubemapFace::PositiveZ:
        return glm::vec3(b - 1.0f, a - 1.0f, 1.0f);
    case CubemapFace::NegativeZ:
        return glm::vec3(1.0f - b, a - 1.0f, -1.0f);
    default:
        throw std::invalid_argument("invalid face");
    }
}
