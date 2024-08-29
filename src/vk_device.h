#pragma once
#include <volk.h>
#include "aurora_pch.h"

namespace Aurora 
{
  struct QueueFamilyIndices
  {
    std::optional<uint32_t> graphics;
    std::optional<uint32_t> present;
    bool is_complete() {
      return graphics.has_value() && present.has_value();
    }
  };
  
  struct Features
    {
      VkPhysicalDeviceFeatures2 features{};
      VkPhysicalDeviceFeatures f1{};
      VkPhysicalDeviceVulkan11Features f11{};
      VkPhysicalDeviceVulkan12Features f12{};
      VkPhysicalDeviceVulkan13Features f13{};

      VkPhysicalDeviceFeatures2& get()
      {
        f11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
        f12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        f13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

        f11.pNext = &f12;
        f12.pNext = &f13;

        features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features.pNext = &f11;
        features.features = f1;

        return features;
      }
  };

  struct Device
  {
      VkDevice logical;
      VkPhysicalDevice physical;
      QueueFamilyIndices indices;
      VkQueue graphics;
      VkQueue present;
      uint32_t graphicsIndex;
      uint32_t presentIndex;
  };
  
  class DeviceBuilder 
  {
    public:
      DeviceBuilder(VkInstance instance, VkSurfaceKHR surface);
      DeviceBuilder& set_minimum_version(int major, int minor);
      DeviceBuilder& set_required_extensions(std::span<const char*> extensions);
      /*
       * set_preferred_gpu_type
       * set_preferred_swapchain_format
       * set_preferred_present_mode
       * set_preferred_distinct_present
      */
      Device build();
    public:
    private:
      VkPhysicalDevice select_physical_device();
      bool check_extension_support(VkPhysicalDevice device);
      bool check_feature_support(VkPhysicalDevice device);
      bool is_device_suitable(VkPhysicalDevice device);
      int rate_physical_device(VkPhysicalDevice device);
      QueueFamilyIndices find_queue_indices(VkPhysicalDevice device);
    private:
      Device m_Device;
      VkInstance m_Instance;
      VkSurfaceKHR m_Surface;
      Features features{};
      int m_MajorVersion, m_MinorVersion;
      std::span<const char*> m_RequiredExtensions;
      
  };
} // namespace Aurora
