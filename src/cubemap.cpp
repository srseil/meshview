#include <stdexcept>
#include <filesystem>
#include <stb_image.h>
#include <stb_image_write.h>
#include <stb_image_resize2.h>
#include <glm/ext.hpp>

#include "cubemap.h"

static Bitmap convertEquirectangularMapToVerticalCross(const Bitmap& bitmap);
static Bitmap convertVerticalCrossToCubeMapFaces(const Bitmap& bitmap);
static Bitmap convertDiffuseToIrradiance(const Bitmap& input, int srcW, int srcH, int dstW, int dstH, int numMonteCarloSamples);

Cubemap::Cubemap(std::string_view fileName)
{
    Bitmap diffuse(fileName);
    Bitmap diffuseCross = convertEquirectangularMapToVerticalCross(diffuse);
    Bitmap diffuseFaces = convertVerticalCrossToCubeMapFaces(diffuseCross);

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
        irradiance = convertDiffuseToIrradiance(diffuse, diffuse.getWidth(), diffuse.getHeight(), 256, 128, 1024);
        stbi_write_hdr(irradianceFileName.c_str(), irradiance.getWidth(), irradiance.getHeight(), 3, irradiance.getData());
    }
    Bitmap irradianceCross = convertEquirectangularMapToVerticalCross(irradiance);
    Bitmap irradianceFaces = convertVerticalCrossToCubeMapFaces(irradianceCross);

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

void Cubemap::bind() const
{
    api.glBindTextures(5, 1, &handleDiffuse);
    api.glBindTextures(6, 1, &handleIrradiance);
}

Bitmap::Bitmap(std::string_view fileName) : depth(1)
{
    std::string fileNameString(fileName);
    const float* imageData = stbi_loadf(fileNameString.c_str(), &width, &height, nullptr, 3);
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

static float radicalInverseVdC(uint32_t bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10f;
}

static glm::vec2 hammersley2d(uint32_t i, uint32_t N)
{
    return glm::vec2(float(i) / float(N), radicalInverseVdC(i));
}

static Bitmap convertDiffuseToIrradiance(const Bitmap& input, int srcW, int srcH, int dstW, int dstH, int numMonteCarloSamples)
{
    assert(srcW == 2 * srcH);
    Bitmap result(dstW, dstH, 1);
    std::vector<glm::vec3> tmp(dstW * dstH);
    stbir_resize(
        input.getData(), srcW, srcH, 0,
        reinterpret_cast<float*>(tmp.data()), dstW, dstH, 0,
        static_cast<stbir_pixel_layout>(3), STBIR_TYPE_FLOAT, STBIR_EDGE_CLAMP, STBIR_FILTER_CUBICBSPLINE);
    const glm::vec3* scratch = tmp.data();
    srcW = dstW;
    srcH = dstH;
    for (int y = 0; y < dstH; y++) {
        const float theta1 = float(y) / float(dstH) * glm::pi<float>();
        for (int x = 0; x < dstW; x++) {
            const float phi1 = float(x) / float(dstW) * glm::two_pi<float>();
            const glm::vec3 v1 = glm::vec3(sin(theta1) * cos(phi1), sin(theta1) * sin(phi1), cos(theta1));
            glm::vec3 color = glm::vec3(0.0f);
            float weight = 0.0f;
            for (int i = 0; i < numMonteCarloSamples; i++) {
                const glm::vec2 h = hammersley2d(i, numMonteCarloSamples);
                const int x1 = int(floor(h.x * srcW));
                const int y1 = int(floor(h.y * srcH));
                const float theta2 = float(y1) / float(srcH) * glm::pi<float>();
                const float phi2 = float(x1) / float(srcW) * glm::two_pi<float>();
                const glm::vec3 v2 = glm::vec3(sin(theta2) * cos(phi2), sin(theta2) * sin(phi2), cos(theta2));
                const float nDotL = std::fmax(0.0f, glm::dot(v1, v2));
                if (nDotL > 0.01f) {
                    color += scratch[y1 * srcW + x1] * nDotL;
                    weight += nDotL;
                }
            }
            result.setPixel(x, y, 0, color / weight);
        }
    }
    return result;
}
