#version 460 core

struct PerVertex {
    vec2 uv;
    vec3 normal;
    vec3 worldPos;
};

layout (std140, binding = 0) uniform PerFrameData {
    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 proj;
    uniform vec4 cameraPos;
    uniform int isWireframe;
};

layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec2 uv;

layout (location = 0) out PerVertex vtx;

void main()
{
    mat4 mvp = proj * view * model;
    gl_Position = mvp * vec4(pos, 1.0);
    mat3 normalMatrix = mat3(transpose(inverse(model)));
    vtx.uv = uv;
    vtx.normal = normalize(normalMatrix * normal); //normal * normalMatrix;
    vtx.worldPos = (model * vec4(pos, 1.0)).xyz;
}
