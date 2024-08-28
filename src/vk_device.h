#pragma once
#include "vk_types.h"
#include <volk.h>
namespace Aurora 
{
  struct Device
  {
    public:
      VkDevice get() { return logicalDevice; }
    public:
      VkPhysicalDevice gpu;
      VkQueue graphics;
      VkQueue present;
      uint32_t graphicsIndex;
      uint32_t presentIndex;
    private:
      VkDevice logicalDevice;
  };
  
  class DeviceBuilder 
  {
    public:
      DeviceBuilder();
      DeviceBuilder& set_minimum_version(int major, int minor);
      DeviceBuilder& set_required_features10(std::initializer_list<VkPhysicalDeviceFeatures> names);
      DeviceBuilder& set_required_features11(char* names[]);
      DeviceBuilder& set_required_features12(char* names[]);
      DeviceBuilder& set_required_features13(char* names[]);
    public:
    private:
      Device m_Device{};
    private:
  };
} // namespace Aurora
