#include "aurora_pch.h"
#include "window.h"
#include "device.h"
#include <vulkan/vulkan.h>

namespace Aurora {
  class Application
  {
    public:
      Application();
      ~Application();
      void Run();
      inline bool ShouldClose() { return glfwWindowShouldClose(m_MainWindow.GetWindow()); }
      inline void PollEvents() { return glfwPollEvents(); }
    public:
      static constexpr int WIDTH = 640;
      static constexpr int HEIGHT = 480;
    private:
      // Vulkan Functions   
      void InitialiseVulkan();
      void CheckInstanceExtensionSupport();
      void CheckValidationLayersSupport();
      void CreateInstance();
      void SetupValidationLayerCallback();
      void PickPhysicalDevice();
      void CreateSwapchain();

      VkResult CreateDebugUtilsMessengerEXT(
        VkInstance instance,
        const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkDebugUtilsMessengerEXT* pDebugMessenger);
      
      void DestroyDebugUtilsMessengerEXT(
        VkInstance instance,
        VkDebugUtilsMessengerEXT debugMessenger,
        const VkAllocationCallbacks* pAllocator);

      static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);    

    private:
      Window m_MainWindow;
      VkInstance m_Instance;
      Device m_Device;
      VkDebugUtilsMessengerEXT m_DebugMessenger{};
      std::vector<const char*> m_RequiredExtensions = {
        // empty for now
      };

      const std::vector<const char*> m_ValidationLayers = {
        "VK_LAYER_KHRONOS_validation"
      };

      #ifdef DEBUG
        const bool m_UseValidationLayers = true;
      #else
        const bool m_UseValidationLayers = false;
      #endif

  };
}
