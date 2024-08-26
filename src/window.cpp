#include "window.h"
#include "logger.h"
//FIX: should glfw be init in window class or application?
// for simplicity, there will only be one window at a time 
// should it be static?
#include "engine.h"

namespace Aurora {

  void Window::init(Config& config)
  {
    glfwSetErrorCallback(glfw_error_callback);
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    m_Window = glfwCreateWindow(config.width, config.height, config.name.c_str(), nullptr, nullptr);
    glfwSetWindowIconifyCallback(m_Window , iconify_callback);
    glfwSetFramebufferSizeCallback(m_Window, framebuffer_resize_callback);
  }

  void Window::iconify_callback(GLFWwindow* window, int iconify)
  {
    Engine::get().stop_rendering = iconify == 0 ? false : true;
    AR_CORE_INFO("Window {}", iconify == 0 ? "MAXIMISED" : "MINIMISED");
  }

  void Window::framebuffer_resize_callback(GLFWwindow* window, int width, int height)
  {
    AR_CORE_INFO("Window resized to {}x{}", width, height);
    Engine::get().resize_requested = true;
  }



  void Window::shutdown()
  {
    glfwDestroyWindow(m_Window);
    glfwTerminate();
  }

  void Window::glfw_error_callback(int error, const char* description)
  {
    AR_CORE_ERROR("GLFW Error ({}): {}", error, description);
  }

} // namespace aurora
