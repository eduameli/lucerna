#include "aurora_pch.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace Aurora {

// NOTE: maybe window should handle the swapchain!
// for that it needs access to VkInstance?
class Window
{
  public:
    Window(int width, int height, const std::string& name);
    ~Window();
    inline GLFWwindow* GetWindow() { return m_Window; } 
  public:

  private:
    static void glfwErrorCallback(int error, const char* description);
  private: 
    GLFWwindow* m_Window;
    int m_Width;
    int m_Height;
};

} // namespace Aurora
