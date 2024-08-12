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
  
      struct DeviceFeatures
      {
        VkPhysicalDeviceFeatures features1{};
        VkPhysicalDeviceVulkan11Features features11{};
        VkPhysicalDeviceVulkan12Features features12{};
        VkPhysicalDeviceVulkan13Features features13{};
        
        DeviceFeatures()
        {
          features11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
          features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
          features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        }
        VkPhysicalDeviceFeatures2 get_features()
        {
          VkPhysicalDeviceFeatures2 features{};
          features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
          features.features = features1;
          
          features.pNext = &features11;
          features11.pNext = &features12;
          features13.pNext = &features13;
          
          return features;
        }
      };
    public:
      DeviceBuilder(VkInstance instance);
      DeviceBuilder& set_required_extensions(const std::vector<const char*>& extensions);
      DeviceBuilder& set_required_features(const DeviceFeatures& features);
      DeviceBuilder& set_required_type(); // not implemented, eg. can required DEDICATED GPU
      DeviceBuilder& select_physical_device();
      DeviceBuilder& set_surface(VkSurfaceKHR surface);
      void build();
      
      VkPhysicalDevice get_physical_device();
      VkDevice get_logical_device();
      VkQueue get_graphics_queue();
      VkQueue get_present_queue();
      Aurora::QueueFamilyIndices get_queue_indices() { return m_QueueIndices; };
    private:
      VkInstance h_Instance{};
      VkDevice h_Device{};
      VkPhysicalDevice h_PhysicalDevice{};
      VkSurfaceKHR h_Surface{};
      std::optional<uint32_t> m_MajorVersion;
      std::optional<uint32_t> m_MinorVersion;
      std::optional<std::vector<const char*>> m_RequiredExtensions;
      Aurora::QueueFamilyIndices m_QueueIndices{};
      DeviceFeatures m_EnabledFeatures{}; 
      VkQueue h_GraphicsQueue{};
      VkQueue h_PresentQueue{};
      
    private:
      bool check_extension_support(VkPhysicalDevice device);
      bool is_device_suitable(VkPhysicalDevice);
      int rate_device(VkPhysicalDevice device);
      Aurora::QueueFamilyIndices find_queue_families(VkPhysicalDevice device);
      void get_queue_families();
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
      
      void get_swapchain_images(std::vector<VkImage> images) const;
      void create_image_views(std::vector<VkImageView>& views, const std::vector<VkImage>& images) const;
      VkSwapchainKHR get_swapchain() { return h_Swapchain; }
      
    private:
      VkPhysicalDevice h_PhysicalDevice;
      VkDevice h_Device;
      VkSurfaceKHR h_Surface;
      Aurora::QueueFamilyIndices m_Indices;
      GLFWwindow* h_Window;
      VkSwapchainKHR h_Swapchain;
    private:
      SwapchainSupportDetails query_swapchain_support(VkPhysicalDevice device);
      VkSurfaceFormatKHR choose_surface_format(const std::vector<VkSurfaceFormatKHR>& supportedFormats);
      VkPresentModeKHR choose_present_mode(const std::vector<VkPresentModeKHR>& supportedModes);
      VkExtent2D choose_extent(const VkSurfaceCapabilitiesKHR& capabilities);
      
  };

} // namespace vkstartup 


// move debug utils fn here ? use volk
