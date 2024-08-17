#pragma once
#include <vulkan/vulkan.h>
#include "aurora_pch.h"

// NOTE: forward declare??
/*namespace Aurora {
  struct QueueFamilyIndices;
}*/

#include "engine.h"

namespace vks
{
  class DeviceBuilder
  {
    public:
      /* 
      struct QueueFamilyIndices
      {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;
        bool is_complete() {
          return graphicsFamily.has_value() && presentFamily.has_value();
        }
      };*/
      
      // FIXME: check if this works, because i want the engine to run on older versions of the api as long as they have
      // the required extensions but if to reference VkPhysicalDeviceVulkan13Features and actually have it in the pNext chain you need the version
      // it sort of defeats the point of all the stuff i did to query support, when i couldve just crashed if it doesnt have it! :(

    public:
      DeviceBuilder(VkInstance instance, VkSurfaceKHR surface);
      void set_required_extensions(const std::vector<const char*>& extensions);
      void set_required_features(VkPhysicalDeviceFeatures2 features);
      void build(VkPhysicalDevice& physicalDevice, VkDevice& device, Aurora::QueueFamilyIndices& indices);
      void get_queues(VkQueue& graphics, VkQueue& present); // NOTE: graphics == present? 
    private:
      VkInstance m_Instance{};
      VkDevice m_Device{};
      //VkPhysicalDevice m_PhysicalDevice{};
      VkSurfaceKHR m_Surface{};
      std::optional<std::vector<const char*>> m_RequiredExtensions;
      Aurora::QueueFamilyIndices m_QueueIndices{};
      VkPhysicalDeviceFeatures2 m_EnabledFeatures{}; 
      //VkQueue m_GraphicsQueue{};
      //VkQueue m_PresentQueue{};
      
    private:
      VkPhysicalDevice select_physical_device();
      bool check_extension_support(VkPhysicalDevice device);
      bool is_device_suitable(VkPhysicalDevice);
      int rate_device(VkPhysicalDevice device);
      Aurora::QueueFamilyIndices find_queue_families(VkPhysicalDevice device);
      //void get_queue_families();
  };

  
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
      SwapchainBuilder& set_devices(VkPhysicalDevice physicalDevice, VkDevice device);
      SwapchainBuilder& set_surface(VkSurfaceKHR surface);
      SwapchainBuilder& set_queue_indices(Aurora::QueueFamilyIndices indices);
      SwapchainBuilder& set_preferred_present_mode();
      SwapchainBuilder& set_preferred_format();
      void build();
      void build_from(VkSwapchainKHR old);  // FIXME: not implemented yet
      
      void get_swapchain_images(std::vector<VkImage>& images);
      void get_image_views(std::vector<VkImageView>& views, const std::vector<VkImage>& images);
      VkSwapchainKHR get_swapchain() { return h_Swapchain; }
      // cancer API need to fix this later..., less classes thing more functions init_swapchain -> call functions from vkstartup::...
      // dont abstract away too much 
      VkExtent2D get_window_extent() { return m_WindowExtent; }
      
    private:
      VkPhysicalDevice h_PhysicalDevice;
      VkDevice h_Device;
      VkSurfaceKHR h_Surface;
      Aurora::QueueFamilyIndices m_Indices;
      GLFWwindow* h_Window;
      VkSwapchainKHR h_Swapchain;
      VkFormat m_SurfaceFormat;
      VkExtent2D m_WindowExtent;
    private:
      SwapchainSupportDetails query_swapchain_support(VkPhysicalDevice device);
      VkSurfaceFormatKHR choose_surface_format(const std::vector<VkSurfaceFormatKHR>& supportedFormats);
      VkPresentModeKHR choose_present_mode(const std::vector<VkPresentModeKHR>& supportedModes);
      VkExtent2D choose_extent(const VkSurfaceCapabilitiesKHR& capabilities);
      
  };

} // namespace vkstartup 


// move debug utils fn here ? use volk
