#pragma once
#include <vulkan/vulkan.h>
#include "aurora_pch.h"
namespace vks
{
  class PhysicalDeviceSelector 
  {
    public:
      struct QueueFamilyIndices
      {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;
        bool is_complete()
        {
          return graphicsFamily.has_value() && presentFamily.has_value();
        }
      };

    public:
      PhysicalDeviceSelector(VkInstance instance);
      PhysicalDeviceSelector& set_minimum_version(int major, int minor);
      PhysicalDeviceSelector& set_required_extensions(const std::vector<const char*>& extensions);
      std::tuple<VkPhysicalDevice, QueueFamilyIndices> select();
    private:
      VkInstance h_Instance;
      std::optional<uint32_t> m_MajorVersion;
      std::optional<uint32_t> m_MinorVersion;
      std::optional<std::vector<const char*>> m_RequiredExtensions;
      
    private:
      bool check_extension_support(VkPhysicalDevice device);
      bool is_device_suitable(VkPhysicalDevice);
      uint32_t rate_device(VkPhysicalDevice device);
      
  };

  class LogicalDeviceBuilder
  {
    public:
    public:
    private:
    private:
  };

} // namespace vkstartup 


// move debug utils fn here ? use volk
