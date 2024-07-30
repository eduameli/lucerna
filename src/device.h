#pragma once
#include <vulkan/vulkan.h>
#include "window.h"
namespace Aurora {
class Device 
{
  public:
    Device(Window& window);
    ~Device();
    inline VkPhysicalDevice GetPhysicalDevice() { return h_PhysicalDevice; }
    inline VkDevice GetLogicalDevice() { return h_Device; }
    inline VkQueue GetGraphicsQueue() { return h_GraphicsQueue; }
    inline VkSwapchainKHR& GetSwapchain() { return h_Swapchain; }
  public:
    struct QueueFamilyIndices
    {
      std::optional<uint32_t> graphicsFamily;
      std::optional<uint32_t> presentFamily;
      bool IsComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
      }
    } indices;
    
    //NOTE: might be a good idea to make a swapchain class?
    struct SwapChainSupportDetails {
      VkSurfaceCapabilitiesKHR capabilities;
      std::vector<VkSurfaceFormatKHR> formats;
      std::vector<VkPresentModeKHR> presentModes;
    };
  private:
    //void InitialiseVulkan();
    void CheckInstanceExtensionSupport();
    void CheckValidationLayersSupport();
    bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
    void CreateInstance();
    void SetupValidationLayerCallback();
    void PickPhysicalDevice();
    uint32_t RateDeviceSuitability(VkPhysicalDevice device);
    bool IsDeviceSuitable(VkPhysicalDevice device);
    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
    void CreateLogicalDevice();
    void CreateSwapchain();
    void CreateImageViews();
    SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);
    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& supportedFormats);
    VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& supportedModes);
    VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
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
    Window& m_Window;

    VkInstance h_Instance{};
    VkDebugUtilsMessengerEXT h_DebugMessenger{};
    VkSurfaceKHR h_Surface{};
    VkPhysicalDevice h_PhysicalDevice{};
    VkDevice h_Device{};
    VkQueue h_GraphicsQueue{};
    VkQueue h_PresentQueue{};
    VkSwapchainKHR h_Swapchain{};
  public:
    std::vector<VkImage> m_SwapchainImages;
  public:
    std::vector<VkImageView> m_SwapchainImageViews;
  private:
    VkExtent2D m_SwapchainExtent;
    VkFormat m_SwapchainImageFormat;

    std::vector<const char*> m_InstanceExtensions = {
      // empty for now (instance extensions)
    };
    const std::vector<const char*> m_ValidationLayers = {
      "VK_LAYER_KHRONOS_validation",
    };

    const std::vector<const char*> m_DeviceExtensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    }; 

    #ifdef DEBUG
      const bool m_UseValidationLayers = false;
    #else
      const bool m_UseValidationLayers = false;
    #endif
    
};

}
