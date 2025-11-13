#include <string>
#include <chrono>
#include <iostream>
#include <fstream>

#include "gl/gl.h"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <gli/gli.hpp>
#include <gli/texture2d.hpp>
#include <gli/load_ktx.hpp>
#include <imgui.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize2.h>

#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include "shader.h"
#include "cubemap.h"
#include "mesh.h"
#include "camera.h"
#include "fps.h"

static void saveScreenshot(std::string_view fileName);

struct PerFrameData {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec4 cameraPos;
    int isWireframe;
};

GL4API api;

static GLFWwindow* window;

static struct MouseState {
    glm::vec2 pos = glm::vec2(0.0f);
    bool pressedLeft = false;
} mouseState;

CameraPositionerFirstPerson positioner(glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
Camera camera(positioner);

static struct RenderState {
    bool fill = true;
    bool wireframe = false;
    bool rotate = false;
    bool transform = false;
    float translation[3];
    float rotation[3];
    float scale[3] = { 1.0f, 1.0f, 1.0f };
} renderState;

int main()
{
    glfwSetErrorCallback(
        [](int error, const char* description) {
            fprintf(stderr, "GLFW Error: %s\n", description);
        }
    );

    if (!glfwInit()) {
        throw std::runtime_error("Could not initialize GLFW");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    window = glfwCreateWindow(1024, 768, "meshview", nullptr, nullptr);
    if (!window) {
        throw std::runtime_error("Could not create GLFW window");
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
        GLShader modelVertex("data/mesh.vert");
        GLShader modelFragment("data/mesh.frag");
        GLProgram modelProgram(modelVertex, modelFragment);

        GLShader cubemapVertex("data/cubemap.vert");
        GLShader cubemapFragment("data/cubemap.frag");
        GLProgram cubemapProgram(cubemapVertex, cubemapFragment);

        Cubemap cubemap("data/piazza_bologni_1k.hdr");
        cubemap.bind();

        Mesh mesh("data/DamagedHelmet/DamagedHelmet.gltf");
        mesh.bind();

        GLuint brdfLutHandle;
        {
            gli::texture tex = gli::load_ktx("data/brdf_lut.ktx");
            gli::gl GL(gli::gl::PROFILE_KTX);
            gli::gl::format const format = GL.translate(tex.format(), tex.swizzles());
            glm::tvec3<GLsizei> extent(tex.extent(0));
            int width = extent.x;
            int height = extent.y;
            int numMipmaps = 1;
            while ((width | height) >> numMipmaps) {
                numMipmaps += 1;
            }
            api.glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            api.glCreateTextures(GL_TEXTURE_2D, 1, &brdfLutHandle);
            api.glTextureParameteri(brdfLutHandle, GL_TEXTURE_MAX_LEVEL, 0);
            api.glTextureParameteri(brdfLutHandle, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            api.glTextureParameteri(brdfLutHandle, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            api.glTextureParameteri(brdfLutHandle, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            api.glTextureParameteri(brdfLutHandle, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            api.glTextureStorage2D(brdfLutHandle, numMipmaps, format.Internal, width, height);
            api.glTextureSubImage2D(brdfLutHandle, 0, 0, 0, width, height, format.External, format.Type, tex.data(0, 0, 0));
        }
        api.glBindTextureUnit(7, brdfLutHandle);

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

            if (!ImGui::GetIO().WantCaptureMouse) {
                positioner.update(deltaSeconds, mouseState.pos, mouseState.pressedLeft);
            }

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
                glm::mat4 model = glm::identity<glm::mat4>();
                if (renderState.rotate) {
                    model = glm::rotate(model, (float)glfwGetTime(), glm::vec3(1.0f, 1.0f, 1.0f));
                }
                else if (renderState.transform) {
                    glm::mat4 translation = glm::translate(glm::identity<glm::mat4>(), glm::vec3(renderState.translation[0], renderState.translation[1], renderState.translation[2]));
                    glm::mat4 rotation = glm::mat4(glm::quat(
                        glm::angleAxis(glm::radians(renderState.rotation[0]), glm::vec3(1.0f, 0.0f, 0.0f)) *
                        glm::angleAxis(glm::radians(renderState.rotation[1]), glm::vec3(0.0f, 1.0f, 0.0f)) *
                        glm::angleAxis(glm::radians(renderState.rotation[2]), glm::vec3(0.0f, 0.0f, 1.0f))
                    ));
                    glm::mat4 scale = glm::scale(glm::identity<glm::mat4>(), glm::vec3(renderState.scale[0], renderState.scale[1], renderState.scale[2]));
                    model = translation * rotation * scale;
                }
                PerFrameData perFrameData = {
                    .model = model,
                    .view = view,
                    .proj = projection,
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
                    .view = view,
                    .proj = projection,
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
            ImGui::Checkbox("Transform", &renderState.transform);
            if (renderState.transform) {
                ImGui::Separator();
                ImGui::SliderFloat3("Translation", renderState.translation, -10.0f, 10.0f, "%.1f");
                ImGui::SliderFloat3("Rotation", renderState.rotation, -180.0f, 180.0f, "%.1f");
                ImGui::SliderFloat3("Scale", renderState.scale, 0.0f, 10.0f, "%.1f");
            }
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

static void saveScreenshot(std::string_view fileName)
{
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    std::vector<uint8_t> data(static_cast<size_t>(width * height * 4));
    api.glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
    std::string fileNameString(fileName);
    stbi_flip_vertically_on_write(1);
    stbi_write_png(fileNameString.c_str(), width, height, 4, data.data(), 0);
}
