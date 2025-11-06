#include <string>
#include <chrono>
#include <iostream>

#include "GL.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <imgui.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/cimport.h>
#include <assimp/version.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

static void save_screenshot(std::string filename);

static const char* shaderCodeVertex = R"(
    #version 460 core

    layout (std140, binding = 0) uniform PerFrameData {
        uniform mat4 MVP;
        uniform int isWireframe;
    };
    layout (location = 0) in vec3 pos;
    layout (location = 0) out vec3 color;

    void main()
    {
        gl_Position = MVP * vec4(pos, 1.0);
        color = isWireframe > 0 ? vec3(0.0) : pos.xyz;
    }
)";

static const char* shaderCodeFragment = R"(
    #version 460 core

    layout (location = 0) in vec3 color;
    layout (location = 0) out vec4 out_FragColor;

    void main()
    {
        out_FragColor = vec4(color, 1.0);
    }
)";

struct PerFrameData {
    glm::mat4 mvp;
    int isWireframe;
};

static GLFWwindow* window;
static GL4API api;

int main()
{
    glfwSetErrorCallback(
        [](int error, const char* description) {
            fprintf(stderr, "GLFW Error: %s\n", description);
        }
    );

    if (!glfwInit()) {
        exit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    window = glfwCreateWindow(1024, 768, "Canyon", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glfwSetKeyCallback(window,
        [](GLFWwindow* window, int key, int scancode, int action, int mods) {
            if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }
            else if (key == GLFW_KEY_F12 && action == GLFW_PRESS) {
                auto now = std::chrono::system_clock::now();
                auto nowLocal = std::chrono::current_zone()->to_local(now);
                std::string filename = std::format("{:%Y-%m-%d_%H_%M_%S}.png", nowLocal);
                save_screenshot(filename);
            }
        }
    );

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    GetAPI4(&api, [](const char* func) -> void* {
        return (void*)glfwGetProcAddress(func);
    });
    InjectAPITracer4(&api);

    int shaderSuccess;
    char shaderInfoLog[1024];

    const GLuint shaderVertex = api.glCreateShader(GL_VERTEX_SHADER);
    api.glShaderSource(shaderVertex, 1, &shaderCodeVertex, nullptr);
    api.glCompileShader(shaderVertex);
    api.glGetShaderiv(shaderVertex, GL_COMPILE_STATUS, &shaderSuccess);
    if (!shaderSuccess) {
        api.glGetShaderInfoLog(shaderVertex, sizeof(shaderInfoLog), NULL, shaderInfoLog);
        std::cerr << "Vertex shader compilation failed: " << std::endl << shaderInfoLog << std::endl;
        
        exit(EXIT_FAILURE);
    }

    const GLuint shaderFragment = api.glCreateShader(GL_FRAGMENT_SHADER);
    api.glShaderSource(shaderFragment, 1, &shaderCodeFragment, nullptr);
    api.glCompileShader(shaderFragment);
    api.glGetShaderiv(shaderFragment, GL_COMPILE_STATUS, &shaderSuccess);
    if (!shaderSuccess) {
        api.glGetShaderInfoLog(shaderFragment, sizeof(shaderInfoLog), NULL, shaderInfoLog);
        std::cerr << "Fragment shader compilation failed:" << std::endl << shaderInfoLog << std::endl;
        exit(EXIT_FAILURE);
    }

    const GLuint shaderProgram = api.glCreateProgram();
    api.glAttachShader(shaderProgram, shaderVertex);
    api.glAttachShader(shaderProgram, shaderFragment);
    api.glLinkProgram(shaderProgram);
    api.glGetProgramiv(shaderProgram, GL_LINK_STATUS, &shaderSuccess);
    if (!shaderSuccess) {
        api.glGetProgramInfoLog(shaderProgram, sizeof(shaderInfoLog), NULL, shaderInfoLog);
        std::cerr << "Shader program linking failed: " << std::endl << shaderInfoLog << std::endl;
        exit(EXIT_FAILURE);
    }
    
    const aiScene* scene = aiImportFile("duck/scene.gltf", aiProcess_Triangulate);
    if (!scene || !scene->HasMeshes()) {
        std::cerr << "Unable to load model: " << "duck/scene.gltf" << std::endl;
        exit(EXIT_FAILURE);
    }

    std::vector<glm::vec3> positions;
    const aiMesh* mesh = scene->mMeshes[0];
    for (unsigned int i = 0; i != mesh->mNumFaces; i++) {
        const aiFace& face = mesh->mFaces[i];
        const unsigned int idx[3] = {
            face.mIndices[0],
            face.mIndices[1],
            face.mIndices[2]
        };
        for (int j = 0; j < 3; j++) {
            const aiVector3D v = mesh->mVertices[idx[j]];
            positions.push_back(glm::vec3(v.x, v.z, v.y));
        }
    }
    aiReleaseImport(scene);

    GLuint vao;
    api.glCreateVertexArrays(1, &vao);
    api.glBindVertexArray(vao);
    GLuint meshData;
    api.glCreateBuffers(1, &meshData);
    api.glNamedBufferStorage(meshData, sizeof(glm::vec3) * positions.size(), positions.data(), 0);
    api.glVertexArrayVertexBuffer(vao, 0, meshData, 0, sizeof(glm::vec3));
    api.glEnableVertexArrayAttrib(vao, 0);
    api.glVertexArrayAttribFormat(vao, 0, 3, GL_FLOAT, GL_FALSE, 0);
    api.glVertexArrayAttribBinding(vao, 0, 0);
    const int numVertices = static_cast<int>(positions.size());

    const GLsizeiptr kBufferSize = sizeof(PerFrameData);
    GLuint perFrameDataBuf;
    api.glCreateBuffers(1, &perFrameDataBuf);
    api.glNamedBufferStorage(perFrameDataBuf, kBufferSize, nullptr, GL_DYNAMIC_STORAGE_BIT);

    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460 core");

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        api.glViewport(0, 0, width, height);
        
        api.glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        api.glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        api.glEnable(GL_DEPTH_TEST);
        api.glEnable(GL_POLYGON_OFFSET_LINE);
        //api.glPolygonOffset(-1.0f, -1.0f);

        api.glBindBufferRange(GL_UNIFORM_BUFFER, 0, perFrameDataBuf, 0, kBufferSize);
        api.glBindVertexArray(vao);
        api.glUseProgram(shaderProgram);

        const float ratio = width / (float)height;
        const glm::mat4 model = glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -3.5f)), (float)glfwGetTime(), glm::vec3(1.0f, 1.0f, 1.0f));
        const glm::mat4 perspective = glm::perspective(45.0f, ratio, 0.1f, 1000.0f);

        // Render filled cube
        PerFrameData perFrameData = {
            .mvp = perspective * model,
            .isWireframe = false
        };
        api.glNamedBufferSubData(perFrameDataBuf, 0, kBufferSize, &perFrameData);
        api.glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        api.glDrawArrays(GL_TRIANGLES, 0, numVertices);

        // Render wireframe cube
        perFrameData.isWireframe = true;
        api.glNamedBufferSubData(perFrameDataBuf, 0, kBufferSize, &perFrameData);
        api.glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        api.glDrawArrays(GL_TRIANGLES, 0, numVertices);

        // Render imgui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::ShowDemoWindow();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    api.glDeleteBuffers(1, &perFrameDataBuf);
    api.glDeleteProgram(shaderProgram);
    api.glDeleteShader(shaderFragment);
    api.glDeleteShader(shaderVertex);
    api.glDeleteVertexArrays(1, &vao);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

static void save_screenshot(std::string filename)
{
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    uint8_t* data = (uint8_t*)malloc(width * height * 4);
    api.glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);
    stbi_write_png(filename.c_str(), width, height, 4, data, 0);
    free(data);
}
