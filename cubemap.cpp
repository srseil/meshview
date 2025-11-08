#include <stdexcept>
#include <stb_image.h>
#include <stb_image_write.h>
#include <glm/ext.hpp>

#include "cubemap.h"

static Bitmap convertEquirectangularMapToVerticalCross(const Bitmap& bitmap);
static Bitmap convertVerticalCrossToCubeMapFaces(const Bitmap& bitmap);

Cubemap::Cubemap(std::string fileName)
{
    Bitmap bitmap(fileName);
    Bitmap cross = convertEquirectangularMapToVerticalCross(bitmap);
    //stbi_write_hdr("cross.hdr", cross.getWidth(), cross.getHeight(), 3, cross.getData());
    Bitmap faces = convertVerticalCrossToCubeMapFaces(cross);

    api.glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &handle);
    api.glTextureParameteri(handle, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    api.glTextureParameteri(handle, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    api.glTextureParameteri(handle, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
    api.glTextureParameteri(handle, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    api.glTextureParameteri(handle, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    api.glTextureStorage2D(handle, 1, GL_RGB32F, faces.getWidth(), faces.getHeight());
    const float* data = faces.getData();
    for (unsigned int face = 0; face < 6; face++) {
        api.glTextureSubImage3D(handle, 0, 0, 0, face, faces.getWidth(), faces.getHeight(), 1, GL_RGB, GL_FLOAT, data);
        data += faces.getWidth() * faces.getHeight() * 3;
    }
}

Cubemap::~Cubemap()
{
    api.glDeleteTextures(1, &handle);
}

void Cubemap::bindTexture() const
{
    api.glBindTextures(1, 1, &handle);
}

Bitmap::Bitmap(std::string fileName) : depth(1)
{
    const float* imageData = stbi_loadf(fileName.c_str(), &width, &height, nullptr, 3);
    data.reserve(width * height * depth * sizeof(float));
    for (unsigned int i = 0; i < width * height * 3; i++) {
        data.push_back(imageData[i]);
    }
    stbi_image_free((void*)imageData);
}

Bitmap::Bitmap(int width, int height, int depth) : width(width), height(height), depth(depth)
{
    data.resize(width * height * depth * sizeof(float));
}

glm::vec3 Bitmap::getPixel(int x, int y, int z) const
{
    int index = (x + y * width + z * width * height) * 3;
    return glm::vec3(data[index], data[index + 1], data[index + 2]);
}

void Bitmap::setPixel(int x, int y, int z, const glm::vec3& color)
{
    int index = (x + y * width + z * width * height) * 3;
    data[index + 0] = color.r;
    data[index + 1] = color.g;
    data[index + 2] = color.b;
}

static glm::vec3 faceCoordsToXYZ(int x, int y, int face, int faceSize)
{
    const float a = 2.0f * float(x) / faceSize;
    const float b = 2.0f * float(y) / faceSize;
    switch (face) {
    case 0: // GL_TEXTURE_CUBE_MAP_POSITIVE_X
        return glm::vec3(-1.0f, a - 1.0f, b - 1.0f);
    case 1: // GL_TEXTURE_CUBE_MAP_NEGATIVE_X
        return glm::vec3(a - 1.0f, -1.0f, 1.0f - b);
    case 2: // GL_TEXTURE_CUBE_MAP_POSITIVE_Y
        return glm::vec3(1.0f, a - 1.0f, 1.0f - b);
    case 3: // GL_TEXTURE_CUBE_MAP_NEGATIVE_Y
        return glm::vec3(1.0f - a, 1.0f, 1.0f - b);
    case 4: // GL_TEXTURE_CUBE_MAP_POSITIVE_Z
        return glm::vec3(b - 1.0f, a - 1.0f, 1.0f);
    case 5: // GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
        return glm::vec3(1.0f - b, a - 1.0f, -1.0f);
    default:
        throw std::invalid_argument("invalid face");
    }
}

static Bitmap convertEquirectangularMapToVerticalCross(const Bitmap& bitmap)
{
    const int faceSize = bitmap.getWidth() / 4;
    const int width = faceSize * 3;
    const int height = faceSize * 4;
    Bitmap result(width, height, 1);

    const glm::ivec2 faceOffsets[] = {
        glm::ivec2(faceSize, faceSize * 3),
        glm::ivec2(0, faceSize),
        glm::ivec2(faceSize, faceSize),
        glm::ivec2(faceSize * 2, faceSize),
        glm::ivec2(faceSize, 0),
        glm::ivec2(faceSize, faceSize * 2)
    };
    const int clampWidth = bitmap.getWidth() - 1;
    const int clampHeight = bitmap.getHeight() - 1;

    for (int face = 0; face < 6; face++) {
        for (int x = 0; x < faceSize; x++) {
            for (int y = 0; y < faceSize; y++) {
                const glm::vec3 p = faceCoordsToXYZ(x, y, face, faceSize);
                const float r = hypot(p.x, p.y);
                const float theta = atan2(p.y, p.x);
                const float phi = atan2(p.z, r);
                constexpr float pi = glm::pi<float>();

                const float uf = float(2.0f * faceSize * (theta + pi) / pi);
                const float vf = float(2.0f * faceSize * (pi / 2.0f - phi) / pi);

                const int u1 = glm::clamp(int(floor(uf)), 0, clampWidth);
                const int v1 = glm::clamp(int(floor(vf)), 0, clampHeight);
                const int u2 = glm::clamp(u1 + 1, 0, clampWidth);
                const int v2 = glm::clamp(v1 + 1, 0, clampHeight);

                const float s = uf - u1;
                const float t = vf - v1;
                const glm::vec3 a = bitmap.getPixel(u1, v1, 0);
                const glm::vec3 b = bitmap.getPixel(u2, v1, 0);
                const glm::vec3 c = bitmap.getPixel(u1, v2, 0);
                const glm::vec3 d = bitmap.getPixel(u2, v2, 0);

                // Biliniear interpolation
                const glm::vec3 color = a * (1 - s) * (1 - t) + b * s * (1 - t) + c * (1 - s) * t + d * s * t;
                result.setPixel(x + faceOffsets[face].x, y + faceOffsets[face].y, 0, color);
            }
        }
    }

    return result;
}

static Bitmap convertVerticalCrossToCubeMapFaces(const Bitmap& bitmap)
{
    const int faceWidth = bitmap.getWidth() / 3;
    const int faceHeight = bitmap.getHeight() / 4;
    Bitmap cubemap(faceWidth, faceHeight, 6);
    for (int face = 0; face < 6; face++) {
        for (int y = 0; y < faceHeight; y++) {
            for (int x = 0; x < faceWidth; x++) {
                int srcX = 0;
                int srcY = 0;
                switch (face) {
                case 0: // GL_TEXTURE_CUBE_MAP_POSITIVE_X
                    srcX = x;
                    srcY = faceHeight + y;
                    break;
                case 1: // GL_TEXTURE_CUBE_MAP_NEGATIVE_X
                    srcX = 2 * faceWidth + x;
                    srcY = faceHeight + y;
                    break;
                case 2: // GL_TEXTURE_CUBE_MAP_POSITIVE_Y
                    srcX = 2 * faceWidth - x - 1;
                    srcY = faceHeight - y - 1;
                    break;
                case 3: // GL_TEXTURE_CUBE_MAP_NEGATIVE_Y
                    srcX = 2 * faceWidth - x - 1;
                    srcY = 3 * faceHeight - y - 1;
                    break;
                case 4: // GL_TEXTURE_CUBE_MAP_POSITIVE_Z
                    srcX = 2 * faceWidth - x - 1;
                    srcY = bitmap.getHeight() - y - 1;
                    break;
                case 5: // GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
                    srcX = faceWidth + x;
                    srcY = faceHeight + y;
                    break;
                }
                cubemap.setPixel(x, y, face, bitmap.getPixel(srcX, srcY, 0));
            }
        }
    }
    return cubemap;
}
