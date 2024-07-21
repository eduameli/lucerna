#pragma once

#include "aurora_pch.h"
#include <vulkan/vulkan.h>

namespace Aurora {
class Device 
{
  public:
    //Device(VkInstance instance);
    void ChooseDevice(VkInstance instane);
    inline VkDevice GetDevice() { return h_Device; }
  public:
    struct QueueFamilyIndices
    {
      std::optional<uint32_t> graphicsFamily;

      bool IsComplete() {
        return graphicsFamily.has_value();
      }
    };

  private:
    uint32_t PickPhysicalDevice();
    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
    void CreateLogicalDevice();
  private:
    VkPhysicalDevice m_PhysicalDevice;
    VkDevice h_Device;
    VkQueue h_GraphicsQueue;
};

}
