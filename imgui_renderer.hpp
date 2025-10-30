#ifndef IMGUI_RENDERER_HPP
#define IMGUI_RENDERE_HPP

#include <GLFW/glfw3.h>

namespace imgui_renderer {

void setup(GLFWwindow *window);
void update(int width, int height);
void destroy();

}

#endif
