#pragma once

#include "aurora_pch.h"
#include <vulkan/vulkan.h>

namespace Aurora {
class Device 
{
  public:
    Device();
    ~Device();
  public:
    struct QueueFamilyIndices
    {
      std::optional<uint32_t> graphicsFamily;

      bool IsComplete() {
        return graphicsFamily.has_value();
      }
    };

  private:
    //void InitialiseVulkan();
    void CheckInstanceExtensionSupport();
    void CheckValidationLayersSupport();
    void CreateInstance();
    void SetupValidationLayerCallback();
    void CreateSurface();
    void PickPhysicalDevice();
    uint32_t RateDeviceSuitability(VkPhysicalDevice device);
    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
    void CreateLogicalDevice();
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
    VkInstance h_Instance{};
    VkDebugUtilsMessengerEXT h_DebugMessenger{};
    VkSurfaceKHR h_Surface{};
    VkPhysicalDevice h_PhysicalDevice{};
    VkDevice h_Device{};
    VkQueue h_GraphicsQueue{};
    
    std::vector<const char*> m_RequiredExtensions = {
      // empty for now (instance extensions)
    };
    const std::vector<const char*> m_ValidationLayers = {
      "VK_LAYER_KHRONOS_validation",
    };

    #ifdef DEBUG
      const bool m_UseValidationLayers = true;
    #else
      const bool m_UseValidationLayers = false;
    #endif
    
};

}
