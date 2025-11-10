#include <iostream>

#include <glm/glm.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/cimport.h>
#include <assimp/version.h>
#include <stb_image.h>

#include "GL.h"

#include "mesh.h"

Mesh::Mesh(std::string fileName)
{
    const aiScene* scene = aiImportFile(fileName.c_str(), aiProcessPreset_TargetRealtime_Quality);
    if (!scene || !scene->HasMeshes()) {
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
            .pos = glm::vec3(pos.x, pos.z, pos.y),
            .normal = glm::vec3(normal.x, normal.y, normal.z),
            .uv = glm::vec2(uv.x, uv.y)
        });
    }
    aiReleaseImport(scene);

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
    glm::vec3 centerOffset = boundsMin - boundsMax;
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

    int texWidth, texHeight;
    const uint8_t* texData = stbi_load("data/duck/textures/Duck_baseColor.png", &texWidth, &texHeight, nullptr, 3);
    api.glCreateTextures(GL_TEXTURE_2D, 1, &texture);
    api.glTextureParameteri(texture, GL_TEXTURE_MAX_LEVEL, 0);
    api.glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    api.glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    api.glTextureStorage2D(texture, 1, GL_RGB8, texWidth, texHeight);
    api.glTextureSubImage2D(texture, 0, 0, 0, texWidth, texHeight, GL_RGB, GL_UNSIGNED_BYTE, texData);
    stbi_image_free((void*)texData);
}

Mesh::~Mesh()
{
    api.glDeleteTextures(1, &texture);
    api.glDeleteBuffers(1, &indexData);
    api.glDeleteBuffers(1, &vertexData);
    api.glDeleteVertexArrays(1, &vao);    
}

void Mesh::bind() const
{
    api.glBindVertexArray(vao);
    api.glBindTextures(0, 1, &texture);
}

void Mesh::draw() const
{
    api.glDrawElements(GL_TRIANGLES, static_cast<unsigned int>(indices.size()), GL_UNSIGNED_INT, nullptr);
}
