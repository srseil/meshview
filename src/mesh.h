#pragma once

#include <string>

struct VertexData {
	glm::vec3 pos;
	glm::vec3 normal;
	glm::vec2 uv;
};

class Mesh {
public:
	Mesh(std::string_view fileName);
	~Mesh();
	void bind() const;
	void draw() const;
private:
	GLuint vao;
	GLuint vertexData;
	GLuint indexData;
	GLuint textureAlbedo;
	GLuint textureMetallicRougness;
	GLuint textureAmbientOcclusion;
	GLuint textureEmissive;
	GLuint textureNormals;
	std::vector<unsigned int> indices;
	std::vector<VertexData> vertices;
};
