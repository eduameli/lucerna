#include "window.h"

#include <GLFW/glfw3.h>
#include "engine.h"
#include "logger.h"
#include "la_asserts.h"

namespace Lucerna {
  
  static Window* s_Instance;

  void Window::init(std::string_view name, int width, int height)
  {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
      // AR_CORE_ERROR("Failed to initialise glfw"); 
    
    LA_ASSERT(s_Instance == nullptr);

    s_Instance = new Window();
    auto& win = s_Instance->m_Window;
  
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    win = glfwCreateWindow(width, height, name.data(), nullptr, nullptr);
    glfwSetWindowIconifyCallback(win, iconify_callback);
    glfwSetWindowSizeCallback(win, resize_callback);
  }
  
  void Window::shutdown()
  {
    glfwDestroyWindow(s_Instance->m_Window);
    glfwTerminate();

    delete s_Instance;
    s_Instance = nullptr;
  }
  
  GLFWwindow* Window::get()
  {
    LA_ASSERT(s_Instance != nullptr);
    return s_Instance->m_Window;
  }
  
  VkExtent2D Window::get_extent()
  {
    int w, h;
    glfwGetFramebufferSize(Window::get(), &w, &h);
    return {static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
  }

  void Window::glfw_error_callback(int error, const char* description)
  {
    // AR_CORE_ERROR("GLFW Error ({}): {}", error, description);
  }

  void Window::iconify_callback(GLFWwindow* window, int iconify)
  {
    Engine::get()->stopRendering = iconify == 0 ? false : true;
    // AR_CORE_WARN("{} Rendering", iconify == 0 ? "Resumed" : "Suspended");
  }

  void Window::resize_callback(GLFWwindow* window, int width, int height)
  {
    if (width > 0 && height > 0)
    {
      Engine::get()->resize_swapchain(width, height);
    }
  }

} // namespace Lucerna
