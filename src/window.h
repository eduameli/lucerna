#pragma once
#include "aurora_pch.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace Aurora {

// NOTE: maybe window should handle the swapchain!
// for that it needs access to VkInstance?
class Window
{
  public:
    //static inline GLFWwindow* h_Window;  //NOTE: only one window for every inst of this class? should only be one anyways...
  public:
    Window(int width, int height, const std::string& name);
    ~Window();
    inline bool should_close() const { return glfwWindowShouldClose(h_Window); };
    inline GLFWwindow* get_handle() { return h_Window; };
    void create_window_surface(VkInstance instance, VkSurfaceKHR& surface) const; 

  private: 
    GLFWwindow* h_Window;
  private:
    static void glfw_error_callback(int error, const char* description);
};


} // namespace Aurora
