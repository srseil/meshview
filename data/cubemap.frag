#version 460 core

layout (std140, binding = 0) uniform PerFrameData {
    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 proj;
    uniform vec4 cameraPos;
    uniform int isWireframe;
};

layout (binding = 5) uniform samplerCube texEnvironment;

layout (location = 0) in vec3 dir;

layout (location = 0) out vec4 out_FragColor;

void main()
{
    out_FragColor = texture(texEnvironment, dir);
}
