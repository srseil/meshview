#include <string>
#include <chrono>
#include <iostream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/common.hpp>
#include <glm/ext.hpp>
#include <imgui.h>

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
        uniform mat4 mvp;
        uniform int isWireframe;
    };
    layout (location = 0) out vec3 color;

    const vec3 pos[8] = vec3[8](
        vec3(-1.0, -1.0,  1.0),
        vec3( 1.0, -1.0,  1.0),
        vec3( 1.0,  1.0,  1.0),
        vec3(-1.0,  1.0,  1.0),
        vec3(-1.0, -1.0, -1.0),
        vec3( 1.0, -1.0, -1.0),
        vec3( 1.0,  1.0, -1.0),
        vec3(-1.0,  1.0, -1.0)
    );
    const vec3 col[8] = vec3[8](
        vec3(1.0, 0.0, 0.0),
        vec3(0.0, 1.0, 0.0),
        vec3(0.0, 0.0, 1.0),
        vec3(1.0, 1.0, 0.0),
        vec3(1.0, 1.0, 0.0),
        vec3(0.0, 0.0, 1.0),
        vec3(0.0, 1.0, 0.0),
        vec3(1.0, 0.0, 0.0)
    );
    const int indices[36] = int[36](
        0, 1, 2, 2, 3, 0,
        1, 5, 6, 6, 2, 1,
        7, 6, 5, 5, 4, 7,
        4, 0, 3, 3, 7, 4,
        4, 5, 1, 1, 0, 4,
        3, 2, 6, 6, 7, 3
    );

    void main() {
        int idx = indices[gl_VertexID];
        gl_Position = mvp * vec4(pos[idx], 1.0);
        color = isWireframe > 0 ? vec3(0.0) : col[idx];
    }
)";

static const char* shaderCodeFragment = R"(
    #version 460 core

    layout (location = 0) in vec3 color;
    layout (location = 0) out vec4 out_FragColor;asdf

    void main() {
        out_FragColor = vec4(color, 1.0);
    }
)";

struct PerFrameData {
    glm::mat4 mvp;
    int isWireframe;
};

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
                save_screenshot(filename);
            }
        }
    );

    glfwMakeContextCurrent(window);
    gladLoadGL();
    glfwSwapInterval(1);

    int shaderSuccess;
    char shaderInfoLog[1024];

    const GLuint shaderVertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(shaderVertex, 1, &shaderCodeVertex, nullptr);
    glCompileShader(shaderVertex);
    glGetShaderiv(shaderVertex, GL_COMPILE_STATUS, &shaderSuccess);
    if (!shaderSuccess) {
        glGetShaderInfoLog(shaderVertex, sizeof(shaderInfoLog), NULL, shaderInfoLog);
        std::cerr << "Vertex shader compilation failed: " << std::endl << shaderInfoLog << std::endl;
        
        exit(EXIT_FAILURE);
    }

    const GLuint shaderFragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(shaderFragment, 1, &shaderCodeFragment, nullptr);
    glCompileShader(shaderFragment);
    glGetShaderiv(shaderFragment, GL_COMPILE_STATUS, &shaderSuccess);
    if (!shaderSuccess) {
        glGetShaderInfoLog(shaderFragment, sizeof(shaderInfoLog), NULL, shaderInfoLog);
        std::cerr << "Fragment shader compilation failed:" << std::endl << shaderInfoLog << std::endl;
        exit(EXIT_FAILURE);
    }

    const GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, shaderVertex);
    glAttachShader(shaderProgram, shaderFragment);
    glLinkProgram(shaderProgram);
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &shaderSuccess);
    if (!shaderSuccess) {
        glGetProgramInfoLog(shaderProgram, sizeof(shaderInfoLog), NULL, shaderInfoLog);
        std::cerr << "Shader program linking failed: " << std::endl << shaderInfoLog << std::endl;
        exit(EXIT_FAILURE);
    }
    
    GLuint vao;
    glCreateVertexArrays(1, &vao);

    const GLsizeiptr kBufferSize = sizeof(PerFrameData);
    GLuint perFrameDataBuf;
    glCreateBuffers(1, &perFrameDataBuf);
    glNamedBufferStorage(perFrameDataBuf, kBufferSize, nullptr, GL_DYNAMIC_STORAGE_BIT);

    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460 core");

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_POLYGON_OFFSET_LINE);
        glPolygonOffset(-1.0f, -1.0f);

        glBindBufferRange(GL_UNIFORM_BUFFER, 0, perFrameDataBuf, 0, kBufferSize);
        glBindVertexArray(vao);
        glUseProgram(shaderProgram);

        const float ratio = width / (float)height;
        const glm::mat4 model = glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -3.5f)), (float)glfwGetTime(), glm::vec3(1.0f, 1.0f, 1.0f));
        const glm::mat4 perspective = glm::perspective(45.0f, ratio, 0.1f, 1000.0f);

        // Render filled cube
        PerFrameData perFrameData = {
            .mvp = perspective * model,
            .isWireframe = false
        };
        glNamedBufferSubData(perFrameDataBuf, 0, kBufferSize, &perFrameData);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // Render wireframe cube
        perFrameData.isWireframe = true;
        glNamedBufferSubData(perFrameDataBuf, 0, kBufferSize, &perFrameData);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glDrawArrays(GL_TRIANGLES, 0, 36);

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

    glDeleteBuffers(1, &perFrameDataBuf);
    glDeleteProgram(shaderProgram);
    glDeleteShader(shaderFragment);
    glDeleteShader(shaderVertex);
    glDeleteVertexArrays(1, &vao);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

static void save_screenshot(std::string filename)
{
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    uint8_t* data = (uint8_t*)malloc(width * height * 4);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);
    stbi_write_png(filename.c_str(), width, height, 4, data, 0);
    free(data);
}
