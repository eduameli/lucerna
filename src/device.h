#pragma once

#include "aurora_pch.h"
#include <vulkan/vulkan.h>

namespace Aurora {
class Device 
{
  public:
    Device(VkInstance instance);
    ~Device();
  public:
    struct QueueFamilyIndices
    {
      std::optional<uint32_t> graphicsFamily;
    };

  private:
    uint32_t PickPhysicalDevice();
    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
  private:
    VkPhysicalDevice m_PhysicalDevice;
};

}
