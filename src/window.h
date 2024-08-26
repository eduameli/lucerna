#pragma once

#include "aurora_pch.h"
class GLFWwindow;

namespace Aurora {
  class Window
  {
    public:
      static void init(std::string_view name, int width, int height);
      static void shutdown();
      static Window& get();
      static GLFWwindow* get_handle();
    private:
      static void glfw_error_callback(int error, const char* description);
      static void iconify_callback(GLFWwindow* window, int iconify);
      static void framebuffer_resize_callback(GLFWwindow* window, int width, int height);
    private:
      GLFWwindow* m_Window;
  };
} // namespace Aurora
