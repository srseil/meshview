#version 460 core

layout (std140, binding = 0) uniform PerFrameData {
    uniform mat4 model;
    uniform mat4 MVP;
    uniform vec4 cameraPos;
    uniform int isWireframe;
};

layout (binding = 1) uniform samplerCube texture1;

layout (location = 0) in vec3 dir;

layout (location = 0) out vec4 out_FragColor;

void main()
{
    out_FragColor = texture(texture1, dir);
}
