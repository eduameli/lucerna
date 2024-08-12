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
      
      struct QueueFamilyIndices
      {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;
        bool is_complete() {
          return graphicsFamily.has_value() && presentFamily.has_value();
        }
      };
      
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
      DeviceBuilder& select_physical_device();
      DeviceBuilder& set_surface(VkSurfaceKHR surface);
      void build();
      
      VkPhysicalDevice get_physical_device();
      VkDevice get_logical_device();
      VkQueue get_graphics_queue();
      VkQueue get_present_queue();
    private:
      VkInstance h_Instance{};
      VkDevice h_Device{};
      VkPhysicalDevice h_PhysicalDevice{};
      VkSurfaceKHR h_Surface{};
      std::optional<uint32_t> m_MajorVersion;
      std::optional<uint32_t> m_MinorVersion;
      std::optional<std::vector<const char*>> m_RequiredExtensions;
      QueueFamilyIndices m_QueueIndices{};
      DeviceFeatures m_EnabledFeatures{}; 
      
      VkQueue h_GraphicsQueue{};
      VkQueue h_PresentQueue{};
      
    private:
      bool check_extension_support(VkPhysicalDevice device);
      bool is_device_suitable(VkPhysicalDevice);
      int rate_device(VkPhysicalDevice device);
      QueueFamilyIndices find_queue_families(VkPhysicalDevice device);
      void get_queue_families();
  };

} // namespace vkstartup 


// move debug utils fn here ? use volk
