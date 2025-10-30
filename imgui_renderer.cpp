#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/common.hpp>
#include <glm/ext.hpp>
#include <imgui.h>

#include "imgui_renderer.hpp"

namespace imgui_renderer {

static const GLchar* shaderCodeVertex = R"(
    #version 460 core

    layout (location = 0) in vec2 Position;
    layout (location = 1) in vec2 UV;
    layout (location = 2) in vec4 Color;

    layout (std140, binding = 0) uniform PerFrameData {
        uniform mat4 MVP;
    };

    out vec2 Frag_UV;
    out vec4 Frag_Color;

    void main()
    {
        Frag_UV = UV;
        Frag_Color = Color;
        gl_Position = MVP * vec4(Position.xy,0,1);
    }
)";

static const GLchar* shaderCodeFragment = R"(
    #version 460 core

    in vec2 Frag_UV;
    in vec4 Frag_Color;

    layout (binding = 0) uniform sampler2D Texture;

    layout (location = 0) out vec4 out_Color;

    void main()
    {
        out_Color = Frag_Color * texture(Texture, Frag_UV.st);
    }
)";

struct PerFrameData {
    glm::mat4 mvp;
};

static GLuint perFrameDataBuf;
static GLuint vao;
static GLuint handleVbo;
static GLuint handleElements;
static GLuint shaderVertex;
static GLuint shaderFragment;
static GLuint program;

void setup(GLFWwindow *window)
{
    glCreateBuffers(1, &perFrameDataBuf);
    glNamedBufferStorage(perFrameDataBuf, sizeof(PerFrameData), nullptr, GL_DYNAMIC_STORAGE_BIT);

    glCreateVertexArrays(1, &vao);

    glCreateBuffers(1, &handleVbo);
    glNamedBufferStorage(handleVbo, 256 * 1024, nullptr, GL_DYNAMIC_STORAGE_BIT);

    glCreateBuffers(1, &handleElements);
    glNamedBufferStorage(handleElements, 256 * 1024, nullptr, GL_DYNAMIC_STORAGE_BIT);

    glVertexArrayElementBuffer(vao, handleElements);
    glVertexArrayVertexBuffer(vao, 0, handleVbo, 0, sizeof(ImDrawVert));
    glEnableVertexArrayAttrib(vao, 0);
    glEnableVertexArrayAttrib(vao, 1);
    glEnableVertexArrayAttrib(vao, 2);

    glVertexArrayAttribFormat(vao, 0, 2, GL_FLOAT, GL_FALSE, IM_OFFSETOF(ImDrawVert, pos));
    glVertexArrayAttribFormat(vao, 1, 2, GL_FLOAT, GL_FALSE, IM_OFFSETOF(ImDrawVert, uv));
    glVertexArrayAttribFormat(vao, 2, 4, GL_UNSIGNED_BYTE, GL_FALSE, IM_OFFSETOF(ImDrawVert, col));

    glVertexArrayAttribBinding(vao, 0, 0);
    glVertexArrayAttribBinding(vao, 1, 0);
    glVertexArrayAttribBinding(vao, 2, 0);

    shaderVertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(shaderVertex, 1, &shaderCodeVertex, nullptr);
    glCompileShader(shaderVertex);

    shaderFragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(shaderFragment, 1, &shaderCodeFragment, nullptr);
    glCompileShader(shaderFragment);

    program = glCreateProgram();
    glAttachShader(program, shaderVertex);
    glAttachShader(program, shaderFragment);
    glLinkProgram(program);

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

    ImFontConfig cfg = ImFontConfig();
    cfg.FontDataOwnedByAtlas = false;
    // Brighten up the font a bit
    cfg.RasterizerMultiply = 1.5f;
    // TODO: Figure out a better approach?
    cfg.SizePixels = 768.0f / 32.0f;
    cfg.PixelSnapH = true;
    cfg.OversampleH = 4;
    cfg.OversampleV = 4;
    ImFont* font = io.Fonts->AddFontFromFileTTF("OpenSans-Light.ttf", cfg.SizePixels, &cfg);

    unsigned char* pixels = nullptr;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    GLuint texture;
    glCreateTextures(GL_TEXTURE_2D, 1, &texture);
    glTextureParameteri(texture, GL_TEXTURE_MAX_LEVEL, 0);
    glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureStorage2D(texture, 1, GL_RGBA8, width, height);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTextureSubImage2D(texture, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindTextures(0, 1, &texture);

    io.Fonts->TexID = (ImTextureID)(intptr_t)texture;
    io.FontDefault = font;
    io.DisplayFramebufferScale = ImVec2(1, 1);

    glfwSetCursorPosCallback(window,
        [](GLFWwindow* window, double x, double y) {
            ImGui::GetIO().MousePos = ImVec2(x, y);
        }
    );

    glfwSetMouseButtonCallback(window,
        [](GLFWwindow* window, int button, int action, int mods) {
            auto& io = ImGui::GetIO();
            int idx = button == GLFW_MOUSE_BUTTON_LEFT ? 0 : (button == GLFW_MOUSE_BUTTON_RIGHT ? 2 : 1);
            io.MouseDown[idx] = action == GLFW_PRESS;
        }
    );
}

void update(int width, int height)
{
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);

    glBindBufferRange(GL_UNIFORM_BUFFER, 0, perFrameDataBuf, 0, sizeof(PerFrameData));
    glBindVertexArray(vao);
    glUseProgram(program);

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2((float)width, (float)height);
    ImGui::NewFrame();
    ImGui::ShowDemoWindow();
    ImGui::Render();
    const ImDrawData* draw_data = ImGui::GetDrawData();
    const float left = draw_data->DisplayPos.x;
    const float right = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
    const float top = draw_data->DisplayPos.y;
    const float bottom = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
    PerFrameData perFrameData = {
        .mvp = glm::ortho(left, right, bottom, top)
    };
    glNamedBufferSubData(perFrameDataBuf, 0, sizeof(PerFrameData), &perFrameData);

    for (int n = 0; n < draw_data->CmdListsCount; n++) {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        glNamedBufferSubData(handleVbo, 0, (GLsizeiptr)cmd_list->VtxBuffer.Size * sizeof(ImDrawVert), cmd_list->VtxBuffer.Data);
        glNamedBufferSubData(handleElements, 0, (GLsizeiptr)cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx), cmd_list->IdxBuffer.Data);

        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            const ImVec4 cr = pcmd->ClipRect;
            glScissor((int)cr.x, (int)(height - cr.w), (int)(cr.z - cr.x), (int)(cr.w - cr.y));
            glBindTextureUnit(0, (GLuint)(intptr_t)pcmd->TextureId);
            glDrawElementsBaseVertex(GL_TRIANGLES, (GLsizei)pcmd->ElemCount, GL_UNSIGNED_SHORT, (void*)(intptr_t)(pcmd->IdxOffset * sizeof(ImDrawIdx)), (GLint)pcmd->VtxOffset);
        }
    }

    glScissor(0, 0, width, height);
}

void destroy()
{
    glDeleteProgram(program);
    glDeleteShader(shaderFragment);
    glDeleteShader(shaderVertex);
    glDeleteBuffers(1, &handleElements);
    glDeleteBuffers(1, &handleVbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &perFrameDataBuf);
}

}