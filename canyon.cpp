#include <string>
#include <chrono>
#include <iostream>
#include <fstream>

#include "GL.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <imgui.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "shader.h"
#include "cubemap.h"
#include "mesh.h"
#include "camera.h"
#include "fps.h"

static void saveScreenshot(std::string filename);

struct PerFrameData {
    glm::mat4 model;
    glm::mat4 mvp;
    glm::vec4 cameraPos;
    int isWireframe;
};

GL4API api;

static GLFWwindow* window;

static struct MouseState {
    glm::vec2 pos = glm::vec2(0.0f);
    bool pressedLeft = false;
} mouseState;

CameraPositionerFirstPerson positioner(glm::vec3(-3.0f, 1.0f, -3.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
Camera camera(positioner);

static struct RenderState {
    bool fill = true;
    bool wireframe = false;
    bool rotate = false;
} renderState;

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

    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460 core");

    GetAPI4(&api, [](const char* func) -> void* {
        return (void*)glfwGetProcAddress(func);
    });
    InjectAPITracer4(&api);

    api.glEnable(GL_DEPTH_TEST);
    api.glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    api.glEnable(GL_POLYGON_OFFSET_LINE);

    // Create new scope to facilitate RAII for shaders/meshes before GL context is destroyed
    {
        GLShader modelVertex("data/basic.vert");
        GLShader modelFragment("data/basic.frag");
        GLProgram modelProgram(modelVertex, modelFragment);

        GLShader cubemapVertex("data/cubemap.vert");
        GLShader cubemapFragment("data/cubemap.frag");
        GLProgram cubemapProgram(cubemapVertex, cubemapFragment);

        Cubemap cubemap("data/piazza_bologni_1k.hdr");
        cubemap.bindTexture();

        Mesh mesh("data/duck/scene.gltf");
        mesh.bind();

        GLuint perFrameDataBuf;
        api.glCreateBuffers(1, &perFrameDataBuf);
        api.glNamedBufferStorage(perFrameDataBuf, sizeof(PerFrameData), nullptr, GL_DYNAMIC_STORAGE_BIT);
        api.glBindBufferRange(GL_UNIFORM_BUFFER, 0, perFrameDataBuf, 0, sizeof(PerFrameData));

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

            const float ratio = width / (float)height;
            const glm::mat4 projection = glm::perspective(45.0f, ratio, 0.1f, 1000.0f);
            const glm::mat4 view = camera.getViewMatrix();

            // Render mesh
            {
                const glm::mat4 model = renderState.rotate
                    ? glm::rotate(glm::identity<glm::mat4>(), (float)glfwGetTime(), glm::vec3(1.0f, 1.0f, 1.0f))
                    : glm::identity<glm::mat4>();
                PerFrameData perFrameData = {
                    .model = model,
                    .mvp = projection * view * model,
                    .cameraPos = glm::vec4(camera.getPosition(), 1.0f)
                };
                modelProgram.useProgram();
                if (renderState.fill) {
                    perFrameData.isWireframe = false;
                    api.glNamedBufferSubData(perFrameDataBuf, 0, sizeof(PerFrameData), &perFrameData);
                    api.glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                    mesh.draw();
                }
                if (renderState.wireframe) {
                    perFrameData.isWireframe = true;
                    api.glNamedBufferSubData(perFrameDataBuf, 0, sizeof(PerFrameData), &perFrameData);
                    api.glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                    mesh.draw();
                }
            }

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
                api.glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                api.glDrawArrays(GL_TRIANGLES, 0, 36);
            }

            // Render imgui
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            ImGui::Begin("Info", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("FPS: %.1f", fpsCounter.getFPS());
            ImGui::Separator();
            ImGui::Checkbox("Fill", &renderState.fill);
            ImGui::Checkbox("Wireframe", &renderState.wireframe);
            ImGui::Separator();
            ImGui::Checkbox("Rotate", &renderState.rotate);
            ImGui::End();

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glfwSwapBuffers(window);
        }

        api.glDeleteBuffers(1, &perFrameDataBuf);
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
