#pragma once

#include "lucerna_pch.h"
#include <volk.h>

class GLFWwindow;

namespace Aurora {
  class Window
  {
    public:
      static void init(std::string_view name, int width, int height);
      static void shutdown();
      static GLFWwindow* get();
      static VkExtent2D get_extent();
    private:
      static void glfw_error_callback(int error, const char* description);
      static void iconify_callback(GLFWwindow* window, int iconify);
      static void resize_callback(GLFWwindow* window, int width, int height);
    private:
      GLFWwindow* m_Window{nullptr};
  };
} // namespace Aurora
