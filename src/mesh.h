#pragma once

#include <string>

struct VertexData {
	glm::vec3 pos;
	glm::vec3 normal;
	glm::vec2 uv;
};

class Mesh {
public:
	explicit Mesh(std::string_view fileName);
	~Mesh();

	Mesh(const Mesh&) = delete;
	Mesh& operator=(const Mesh&) = delete;

	Mesh(Mesh&& other) noexcept;
	Mesh& operator=(Mesh&& other) noexcept;

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
