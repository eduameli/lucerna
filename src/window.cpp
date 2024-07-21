#include "window.h"

//FIX: should glfw be init in window class or application?
// for simplicity, there will only be one window at a time 
// should it be static?

namespace Aurora {

Window::Window(int width, int height, const std::string& name)
  : m_Width{width}, m_Height{height}, m_Window{nullptr}
{
  glfwSetErrorCallback(glfwErrorCallback);
  glfwInit();

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

  m_Window = glfwCreateWindow(800, 600, name.c_str(), nullptr, nullptr);

}

Window::~Window()
{
  glfwDestroyWindow(m_Window);
  glfwTerminate();
  // FIXME: since application destroys the instance should i create the instance in
  // main? and then use destructors normally..
}

void Window::CreateSurface(VkInstance instance)
{
  VkResult result = glfwCreateWindowSurface(instance, m_Window, nullptr, &h_Surface);
  AR_ASSERT(result == VK_SUCCESS, "Failed to create window surface!");
}

void Window::glfwErrorCallback(int error, const char* description)
{
  AR_CORE_ERROR("GLFW Error ({}): {}", error, description);
}


}
