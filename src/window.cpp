#include "window.h"

#include <GLFW/glfw3.h>
#include "engine.h"
#include "logger.h"
#include "ar_asserts.h"

namespace Aurora {
  
  static Window* s_Instance;

  void Window::init(std::string_view name, int width, int height)
  {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
      AR_CORE_ERROR("Failed to initialise glfw"); 
    
    AR_ASSERT(s_Instance == nullptr);

    s_Instance = new Window();
    auto& win = s_Instance->m_Window;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    win = glfwCreateWindow(width, height, name.data(), nullptr, nullptr);
    glfwSetWindowIconifyCallback(win, iconify_callback);
  }
  
  void Window::shutdown()
  {
    glfwDestroyWindow(s_Instance->m_Window);
    glfwTerminate();

    delete s_Instance;
    s_Instance = nullptr;
  }
  
  Window& Window::get()
  {
    AR_ASSERT(s_Instance != nullptr);
    return *s_Instance;
  }
  
  GLFWwindow* Window::get_handle()
  {
    AR_ASSERT(s_Instance != nullptr);
    return s_Instance->m_Window;
  }
  
  VkExtent2D Window::get_extent()
  {
    int w, h;
    glfwGetFramebufferSize(Window::get_handle(), &w, &h);
    return {static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
  }

  void Window::glfw_error_callback(int error, const char* description)
  {
    AR_CORE_ERROR("GLFW Error ({}): {}", error, description);
  }

  void Window::iconify_callback(GLFWwindow* window, int iconify)
  {
    Engine::get().stopRendering = iconify == 0 ? false : true;
    AR_CORE_INFO("Window {}", iconify == 0 ? "MAXIMISED" : "MINIMISED");
  }

} // namespace aurora
