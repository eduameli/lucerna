#include "aurora_pch.h"
#include "device.h"

namespace Aurora {
Device::Device(VkInstance instance)
{
  uint32_t deviceCount;
  vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
  AR_ASSERT(deviceCount != 0, "Failed to find GPUs with Vulkan support!");
  
  std::vector<VkPhysicalDevice> devices(deviceCount);
  vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
  

  m_PhysicalDevice = devices[1];
  VkPhysicalDeviceProperties deviceProperties;
  vkGetPhysicalDeviceProperties(m_PhysicalDevice, &deviceProperties);
  AR_CORE_INFO("Using {} as the Physical Device", deviceProperties.deviceName);
}

Device::~Device()
{
  // invalid? because VkInstance is already deleted or smth?
}

Device::QueueFamilyIndices Device::FindQueueFamilies(VkPhysicalDevice device)
{
  QueueFamilyIndices indices;
  
  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);

  return indices;
}
}
