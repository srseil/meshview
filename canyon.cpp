#include <string>
#include <chrono>
#include <iostream>
#include <fstream>

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

#include "shader.h"

static void saveScreenshot(std::string filename);

struct PerFrameData {
    glm::mat4 mvp;
    int isWireframe;
};

GL4API api;

static GLFWwindow* window;

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
                saveScreenshot(filename);
            }
        }
    );

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    GetAPI4(&api, [](const char* func) -> void* {
        return (void*)glfwGetProcAddress(func);
    });
    InjectAPITracer4(&api);

    // Create new scope to facilitate RAII for shaders/meshes before GL context is destroyed
    {
        GLShader vertexShader("data/basic.vert");
        GLShader fragmentShader("data/basic.frag");
        GLProgram shaderProgram(vertexShader, fragmentShader);

        std::string sceneFile = "data/duck/scene.gltf";
        const aiScene* scene = aiImportFile(sceneFile.c_str(), aiProcess_Triangulate);
        if (!scene || !scene->HasMeshes()) {
            std::cerr << "Unable to load model: " << sceneFile << std::endl;
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
            shaderProgram.useProgram();

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

        api.glDeleteBuffers(1, &perFrameDataBuf);
        api.glDeleteVertexArrays(1, &vao);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

static void saveScreenshot(std::string fileName)
{
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    uint8_t* data = (uint8_t*)malloc(width * height * 4);
    api.glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);
    stbi_write_png(fileName.c_str(), width, height, 4, data, 0);
    free(data);
}
