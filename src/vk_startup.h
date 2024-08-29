#pragma once
#include <volk.h>
#include "aurora_pch.h"
#include <GLFW/glfw3.h>
#include "vk_types.h"
#include "vk_device.h"
namespace vks
{
  //FIXME: check features.
  
  class SwapchainBuilder
  {
    public:
      struct SwapchainSupportDetails
      {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
      };
    public:
      SwapchainBuilder(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, Aurora::QueueFamilyIndices indices);
      void set_preferred_present_mode(VkPresentModeKHR present);
      void set_preferred_format(VkSurfaceFormatKHR format);
      // FIXME: optionally build swapchain from previous one
      void build(VkSwapchainKHR& swapchain, std::vector<VkImage>& images, VkSurfaceFormatKHR& format, VkExtent2D& extent);
      
    private:
      VkPhysicalDevice h_PhysicalDevice;
      VkDevice h_Device;
      VkSurfaceKHR h_Surface;
      Aurora::QueueFamilyIndices m_Indices;

      VkPresentModeKHR preferredPresent;
      VkSurfaceFormatKHR preferredFormat;
      //GLFWwindow* h_Window;
    private:
      SwapchainSupportDetails query_swapchain_support(VkPhysicalDevice device);
      VkSurfaceFormatKHR choose_surface_format(const std::vector<VkSurfaceFormatKHR>& supportedFormats);
      VkPresentModeKHR choose_present_mode(const std::vector<VkPresentModeKHR>& supportedModes);
      VkExtent2D choose_extent(const VkSurfaceCapabilitiesKHR& capabilities);
      
  };
  
  void setup_validation_layer_callback(VkInstance instance, VkDebugUtilsMessengerEXT& messenger, PFN_vkDebugUtilsMessengerCallbackEXT pfnUserCallback);

  VkResult create_debug_messenger(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger
  );
  void destroy_debug_messenger(
    VkInstance instance,
    VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks* pAllocator
  );
} // namespace vkstartup 


// move debug utils fn here ? use volk
