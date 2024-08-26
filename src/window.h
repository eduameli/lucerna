#pragma once
#include "aurora_pch.h"
#include <GLFW/glfw3.h>

namespace Aurora {
  class Window
  {
    public:
      struct Config
      {
        int width, height;
        std::string name;
      };

      void init(Config& config);
      void shutdown();
      inline GLFWwindow* get_handle() { return m_Window; }
    public:
    private:
      static void glfw_error_callback(int error, const char* description);
      static void iconify_callback(GLFWwindow* window, int iconify);
      static void framebuffer_resize_callback(GLFWwindow* window, int width, int height);
    private:
      GLFWwindow* m_Window{};
  };
} // namespace Aurora
