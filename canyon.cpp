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
#include "cubemap.h"
#include "camera.h"
#include "fps.h"

static void saveScreenshot(std::string filename);

struct PerFrameData {
    glm::mat4 model;
    glm::mat4 mvp;
    glm::vec4 cameraPos;
    int isWireframe;
};

struct VertexData {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
};

GL4API api;

static GLFWwindow* window;

static struct MouseState {
    glm::vec2 pos = glm::vec2(0.0f);
    bool pressedLeft = false;
} mouseState;

CameraPositionerFirstPerson positioner(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
Camera camera(positioner);

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
            else {
                const bool press = action != GLFW_RELEASE;
                if (key == GLFW_KEY_W) positioner.movement.forward = press;
                if (key == GLFW_KEY_S) positioner.movement.backward = press;
                if (key == GLFW_KEY_A) positioner.movement.left = press;
                if (key == GLFW_KEY_D) positioner.movement.right = press;
                if (key == GLFW_KEY_SPACE) positioner.movement.up = press;
                if (key == GLFW_KEY_LEFT_CONTROL) positioner.movement.down = press;
                if (key == GLFW_KEY_LEFT_SHIFT) positioner.movement.fastSpeed = press;
                if (key == GLFW_KEY_F) positioner.setUpVector(glm::vec3(0.0f, 1.0f, 0.0f));
            }
        }
    );
    glfwSetCursorPosCallback(window,
        [](GLFWwindow* window, double x, double y) {
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);
            mouseState.pos.x = static_cast<float>(x / width);
            mouseState.pos.y = static_cast<float>(y / height);
        }
    );
    glfwSetMouseButtonCallback(window,
        [](GLFWwindow* window, int button, int action, int mods) {
            if (button == GLFW_MOUSE_BUTTON_LEFT) {
                mouseState.pressedLeft = action == GLFW_PRESS;
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
        GLShader modelVertex("data/basic.vert");
        GLShader modelFragment("data/basic.frag");
        GLProgram modelProgram(modelVertex, modelFragment);

        GLShader cubemapVertex("data/cubemap.vert");
        GLShader cubemapFragment("data/cubemap.frag");
        GLProgram cubemapProgram(cubemapVertex, cubemapFragment);

        Cubemap cubemap("data/piazza_bologni_1k.hdr");

        std::string sceneFile = "data/duck/scene.gltf";
        const aiScene* scene = aiImportFile(sceneFile.c_str(), aiProcess_Triangulate);
        if (!scene || !scene->HasMeshes()) {
            std::cerr << "Unable to load model: " << sceneFile << std::endl;
            exit(EXIT_FAILURE);
        }

        std::vector<unsigned int> indices;
        const aiMesh* mesh = scene->mMeshes[0];
        for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
            const aiFace& face = mesh->mFaces[i];
            for (unsigned int j = 0; j < face.mNumIndices; j++) {
                indices.push_back(face.mIndices[j]);
            }
        }
        std::vector<VertexData> vertices;
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

        GLuint vao;
        api.glCreateVertexArrays(1, &vao);

        GLuint vertexData;
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

        GLuint indexData;
        api.glCreateBuffers(1, &indexData);
        api.glNamedBufferStorage(indexData, sizeof(unsigned int) * indices.size(), indices.data(), 0);
        api.glVertexArrayElementBuffer(vao, indexData);

        int texWidth, texHeight;
        const uint8_t* texData = stbi_load("data/duck/textures/Duck_baseColor.png", &texWidth, &texHeight, nullptr, 3);
        GLuint texture;
        api.glCreateTextures(GL_TEXTURE_2D, 1, &texture);
        api.glTextureParameteri(texture, GL_TEXTURE_MAX_LEVEL, 0);
        api.glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        api.glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        api.glTextureStorage2D(texture, 1, GL_RGB8, texWidth, texHeight);
        api.glTextureSubImage2D(texture, 0, 0, 0, texWidth, texHeight, GL_RGB, GL_UNSIGNED_BYTE, texData);
        api.glBindTextures(0, 1, &texture);
        stbi_image_free((void*)texData);

        cubemap.bindTexture();

        GLuint perFrameDataBuf;
        api.glCreateBuffers(1, &perFrameDataBuf);
        api.glNamedBufferStorage(perFrameDataBuf, sizeof(PerFrameData), nullptr, GL_DYNAMIC_STORAGE_BIT);

        ImGui::CreateContext();
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 460 core");

        double timestamp = glfwGetTime();
        float deltaSeconds = 0.0f;
        FramesPerSecondCounter fpsCounter(0.5f);

        while (!glfwWindowShouldClose(window)) {
            std::cout << std::endl;

            const double newTimestamp = glfwGetTime();
            deltaSeconds = static_cast<float>(newTimestamp - timestamp);
            timestamp = newTimestamp;
            fpsCounter.tick(deltaSeconds);
            fprintf(stdout, "FPS: %f\n", fpsCounter.getFPS());

            glfwPollEvents();

            positioner.update(deltaSeconds, mouseState.pos, mouseState.pressedLeft);

            int width, height;
            glfwGetFramebufferSize(window, &width, &height);
            if (width == 0 || height == 0) {
                continue;
            }
            api.glViewport(0, 0, width, height);

            api.glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
            api.glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            api.glEnable(GL_DEPTH_TEST);
            api.glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
            api.glEnable(GL_POLYGON_OFFSET_LINE);
            //api.glPolygonOffset(-1.0f, -1.0f);

            api.glBindBufferRange(GL_UNIFORM_BUFFER, 0, perFrameDataBuf, 0, sizeof(PerFrameData));
            api.glBindVertexArray(vao);

            const float ratio = width / (float)height;
            const glm::mat4 projection = glm::perspective(45.0f, ratio, 0.1f, 1000.0f);
            const glm::mat4 view = camera.getViewMatrix();

            // Render filled cube
            {
                const glm::mat4 model = glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -3.5f)), (float)glfwGetTime(), glm::vec3(1.0f, 1.0f, 1.0f));
                const PerFrameData perFrameData = {
                    .model = model,
                    .mvp = projection * view * model,
                    .cameraPos = glm::vec4(camera.getPosition(), 1.0f),
                    .isWireframe = false
                };
                modelProgram.useProgram();
                api.glNamedBufferSubData(perFrameDataBuf, 0, sizeof(PerFrameData), &perFrameData);
                api.glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                api.glDrawElements(GL_TRIANGLES, static_cast<unsigned int>(indices.size()), GL_UNSIGNED_INT, nullptr);
            }

            // Render wireframe cube
            /*
            perFrameData.isWireframe = true;
            api.glNamedBufferSubData(perFrameDataBuf, 0, sizeof(PerFrameData), &perFrameData);
            api.glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            api.glDrawElements(GL_TRIANGLES, static_cast<unsigned int>(indices.size()), GL_UNSIGNED_INT, nullptr);
            */

            // Render cubemap
            {
                const glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(100.0f));
                const PerFrameData perFrameData = {
                    .model = model,
                    .mvp = projection * view * model,
                    .cameraPos = glm::vec4(camera.getPosition(), 1.0f),
                    .isWireframe = false
                };
                cubemapProgram.useProgram();
                api.glNamedBufferSubData(perFrameDataBuf, 0, sizeof(PerFrameData), &perFrameData);
                api.glDrawArrays(GL_TRIANGLES, 0, 36);
            }

            // Render imgui
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            //ImGui::ShowDemoWindow();
            ImGui::Begin("Info", nullptr, 0);
            ImGui::Text("FPS: %.1f", fpsCounter.getFPS());
            ImGui::End();

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
