#include "window.h"
#include "log.h"
//FIX: should glfw be init in window class or application?
// for simplicity, there will only be one window at a time 
// should it be static?
#include "engine.h"

namespace Aurora {


void iconify_callback(GLFWwindow* window, int iconify)
{
  Engine::get().stop_rendering = iconify == 0 ? false : true;
  AR_CORE_INFO("Window {}", iconify == 0 ? "MAXIMISED" : "MINIMISED");
}

void framebuffer_resize_callback(GLFWwindow* window, int width, int height)
{
  AR_CORE_INFO("Window resized to {}x{}", width, height);
  Engine::get().resize_requested = true;
}

Window::Window(int width, int height, const std::string& name)
{
  glfwSetErrorCallback(glfw_error_callback);
  glfwInit();

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  //glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

  h_Window = glfwCreateWindow(width, height, name.c_str(), nullptr, nullptr);
  glfwSetWindowIconifyCallback(h_Window , iconify_callback);
  glfwSetFramebufferSizeCallback(h_Window, framebuffer_resize_callback);
}

Window::~Window()
{
  glfwDestroyWindow(h_Window);
  glfwTerminate();
}

void Window::create_window_surface(VkInstance instance, VkSurfaceKHR& surface) const
{
  VK_CHECK_RESULT(glfwCreateWindowSurface(instance, h_Window, nullptr, &surface));
}

void Window::glfw_error_callback(int error, const char* description)
{
  AR_CORE_ERROR("GLFW Error ({}): {}", error, description);
}


}
