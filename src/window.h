#include "aurora_pch.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace Aurora {

class Window
{
  public:
    Window(int width, int height, const std::string& name);
    ~Window();

    inline bool ShouldClose() { return glfwWindowShouldClose(m_Window); }
    inline void PollEvents() { glfwPollEvents(); }
  public:

  private:
    static void glfwErrorCallback(int error, const char* description);
  private: 
    GLFWwindow* m_Window;
    int m_Width;
    int m_Height;
};

} // namespace Aurora
