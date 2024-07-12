#include "aurora_pch.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

void glfwErrorCallback(int error, const char* description)
{
    AR_CORE_ERROR("GLFW Error ({}): {}", error, description);
}

int main()
{
  Aurora::Log::Init();
  AR_CORE_INFO("Initialised Logging!");
    
  glfwSetErrorCallback(glfwErrorCallback);
  if(!glfwInit())
  {
    AR_CORE_FATAL("Failed to initialise glfw");
    return -1;
  }
  
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow* window = glfwCreateWindow(800, 600, "Aurora", nullptr, nullptr);
    
  if(!window)
  {
    AR_CORE_FATAL("Failed to create glfw window");
    glfwTerminate();
    return -1;
  }


  uint32_t extensionCount = 0;
  vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
  
  AR_CORE_INFO("{} VULKAN EXTENSIONS supported", extensionCount);

  while(!glfwWindowShouldClose(window)) {
    glfwPollEvents();
  }

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
