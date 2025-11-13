#include <iostream>
#include <filesystem>

#include <glm/glm.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/cimport.h>
#include <assimp/version.h>
#include <stb_image.h>

#include "gl/gl.h"

#include "mesh.h"

static void loadTexture(const char* filePath, GLuint* handle);

Mesh::Mesh(std::string fileName)
{
    const aiScene* scene = aiImportFile(fileName.c_str(), aiProcessPreset_TargetRealtime_Quality);
    if (!scene || !scene->HasMeshes() || !scene->HasMaterials()) {
        std::cerr << "Unable to load mesh: " << fileName << std::endl;
        exit(EXIT_FAILURE);
    }
    
    const aiMesh* mesh = scene->mMeshes[0];
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        const aiFace& face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++) {
            indices.push_back(face.mIndices[j]);
        }
    }
    for (int i = 0; i < mesh->mNumVertices; i++) {
        const aiVector3D pos = mesh->mVertices[i];
        const aiVector3D normal = mesh->mNormals[i];
        const aiVector3D uv = mesh->mTextureCoords[0][i];
        vertices.push_back({
            .pos = glm::vec3(pos.x, pos.y, pos.z),
            .normal = glm::vec3(normal.x, normal.y, normal.z),
            .uv = glm::vec2(uv.x, -uv.y)
        });
    }

    std::filesystem::path path(fileName);
    std::string dataPath = path.parent_path().string() + "/";
    aiMaterial* material = scene->mMaterials[0];
    
    aiString albedoFileName;
    if (material->GetTexture(aiTextureType_BASE_COLOR, 0, &albedoFileName) != AI_SUCCESS) {
        std::cerr << "Missing BASE_COLOR (albedo) texture" << std::endl;
        exit(EXIT_FAILURE);
    }
    std::string albedoPath = dataPath + albedoFileName.C_Str();

    aiString metallicRoughnessFileName;
    if (material->GetTexture(aiTextureType_METALNESS, 0, &metallicRoughnessFileName) != AI_SUCCESS) {
        std::cerr << "Missing METALNESS (metallic roughness) texture" << std::endl;
        exit(EXIT_FAILURE);
    }
    std::string metallicRoughnessPath = dataPath + metallicRoughnessFileName.C_Str();

    aiString ambientOcclusionFileName;
    if (material->GetTexture(aiTextureType_LIGHTMAP, 0, &ambientOcclusionFileName) != AI_SUCCESS) {
        std::cerr << "Missing LIGHTMAP (ambient occlusion) texture" << std::endl;
        exit(EXIT_FAILURE);
    }
    std::string ambientOcclusionPath = dataPath + ambientOcclusionFileName.C_Str();

    aiString emissiveFileName;
    if (material->GetTexture(aiTextureType_EMISSIVE, 0, &emissiveFileName) != AI_SUCCESS) {
        std::cerr << "Missing EMISSIVE (emissive) texture" << std::endl;
        exit(EXIT_FAILURE);
    }
    std::string emissivePath = dataPath + emissiveFileName.C_Str();

    aiString normalsFileName;
    if (material->GetTexture(aiTextureType_NORMALS, 0, &normalsFileName) != AI_SUCCESS) {
        std::cerr << "Missing NORMALS (normals) texture" << std::endl;
        exit(EXIT_FAILURE);
    }
    std::string normalsPath = dataPath + normalsFileName.C_Str();

    aiReleaseImport(scene);

    // Center mesh at (0, 0, 0)
    glm::vec3 boundsMin = glm::vec3(0.0f);
    glm::vec3 boundsMax = glm::vec3(0.0f);
    for (int i = 0; i < vertices.size(); i++) {
        boundsMin.x = std::fmin(boundsMin.x, vertices[i].pos.x);
        boundsMin.y = std::fmin(boundsMin.y, vertices[i].pos.y);
        boundsMin.z = std::fmin(boundsMin.z, vertices[i].pos.z);

        boundsMax.x = std::fmax(boundsMax.x, vertices[i].pos.x);
        boundsMax.y = std::fmax(boundsMax.y, vertices[i].pos.y);
        boundsMax.z = std::fmax(boundsMax.z, vertices[i].pos.z);
    }
    glm::vec3 centerOffset = (boundsMin + boundsMax) / 2.0f;
    for (int i = 0; i < vertices.size(); i++) {
        vertices[i].pos += centerOffset;
    }

    api.glCreateVertexArrays(1, &vao);
    
    api.glCreateBuffers(1, &vertexData);
    api.glNamedBufferStorage(vertexData, sizeof(VertexData) * vertices.size(), vertices.data(), 0);
    api.glVertexArrayVertexBuffer(vao, 0, vertexData, 0, sizeof(VertexData));
    // pos
    api.glEnableVertexArrayAttrib(vao, 0);
    api.glVertexArrayAttribFormat(vao, 0, 3, GL_FLOAT, GL_FALSE, offsetof(VertexData, pos));
    api.glVertexArrayAttribBinding(vao, 0, 0);
    // normal
    api.glEnableVertexArrayAttrib(vao, 1);
    api.glVertexArrayAttribFormat(vao, 1, 3, GL_FLOAT, GL_FALSE, offsetof(VertexData, normal));
    api.glVertexArrayAttribBinding(vao, 1, 0);
    // uv
    api.glEnableVertexArrayAttrib(vao, 2);
    api.glVertexArrayAttribFormat(vao, 2, 2, GL_FLOAT, GL_FALSE, offsetof(VertexData, uv));
    api.glVertexArrayAttribBinding(vao, 2, 0);

    api.glCreateBuffers(1, &indexData);
    api.glNamedBufferStorage(indexData, sizeof(unsigned int) * indices.size(), indices.data(), 0);
    api.glVertexArrayElementBuffer(vao, indexData);

    loadTexture(albedoPath.c_str(), &textureAlbedo);
    loadTexture(metallicRoughnessPath.c_str(), &textureMetallicRougness);
    loadTexture(ambientOcclusionPath.c_str(), &textureAmbientOcclusion);
    loadTexture(emissivePath.c_str(), &textureEmissive);
    loadTexture(normalsPath.c_str(), &textureNormals);
}

Mesh::~Mesh()
{
    api.glDeleteTextures(1, &textureNormals);
    api.glDeleteTextures(1, &textureEmissive);
    api.glDeleteTextures(1, &textureAmbientOcclusion);
    api.glDeleteTextures(1, &textureMetallicRougness);
    api.glDeleteTextures(1, &textureAlbedo);
    api.glDeleteBuffers(1, &indexData);
    api.glDeleteBuffers(1, &vertexData);
    api.glDeleteVertexArrays(1, &vao);    
}

void Mesh::bind() const
{
    api.glBindVertexArray(vao);
    api.glBindTextures(0, 1, &textureAlbedo);
    api.glBindTextures(1, 1, &textureMetallicRougness);
    api.glBindTextures(2, 1, &textureAmbientOcclusion);
    api.glBindTextures(3, 1, &textureEmissive);
    api.glBindTextures(4, 1, &textureNormals);
}

void Mesh::draw() const
{
    api.glDrawElements(GL_TRIANGLES, static_cast<unsigned int>(indices.size()), GL_UNSIGNED_INT, nullptr);
}

static void loadTexture(const char* filePath, GLuint* handle)
{
    int width, height;
    const uint8_t* data = stbi_load(filePath, &width, &height, nullptr, STBI_rgb_alpha);
    api.glCreateTextures(GL_TEXTURE_2D, 1, handle);
    api.glTextureParameteri(*handle, GL_TEXTURE_MAX_LEVEL, 0);
    api.glTextureParameteri(*handle, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    api.glTextureParameteri(*handle, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    api.glTextureStorage2D(*handle, 1, GL_RGBA8, width, height);
    api.glTextureSubImage2D(*handle, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);
    stbi_image_free((void*)data);
}
