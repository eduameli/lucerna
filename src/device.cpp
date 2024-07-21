#include "aurora_pch.h"
#include "device.h"

namespace Aurora {
void Device::ChooseDevice(VkInstance instance)
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

  //FIX: vkGetDeviceQueue - index is hardcoded at 0 right now because we are only creating one queue
  // from this family!
  CreateLogicalDevice();
  auto indices = FindQueueFamilies(m_PhysicalDevice);
  vkGetDeviceQueue(h_Device, indices.graphicsFamily.value(), 0, &h_GraphicsQueue);
}

Device::QueueFamilyIndices Device::FindQueueFamilies(VkPhysicalDevice device)
{
  QueueFamilyIndices indices;
  
  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
  
  int i = 0;
  for (const auto& queueFamily : queueFamilies)
  {
    if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
    {
      indices.graphicsFamily = i;
    }
    if(indices.IsComplete())
      break;
    
  }
  
  AR_ASSERT(indices.IsComplete(), "Succesfully found all required Queue Families!");
  return indices;
}

void Device::CreateLogicalDevice()
{

  QueueFamilyIndices indices = FindQueueFamilies(m_PhysicalDevice);

  VkDeviceQueueCreateInfo queueCreateInfo{};
  queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
  queueCreateInfo.queueCount = 1;
  
  // NOTE: im guessing this can be a float array? size = queueCount.
  float queuePriority = 1.0f;
  queueCreateInfo.pQueuePriorities = &queuePriority;

  VkPhysicalDeviceFeatures deviceFeatures{};
  VkDeviceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  
  // NOTE: this could also be the case for this you would have an array of DeviceQueueCreateInfo
  //  to allow a device to have different queue for different type of cmd's (Graphics, Compute, Raytracing?)
  createInfo.pQueueCreateInfos = &queueCreateInfo;
  createInfo.queueCreateInfoCount = 1;
  createInfo.pEnabledFeatures = &deviceFeatures;

  VkResult result = vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &h_Device);
  AR_ASSERT(result == VK_SUCCESS, "Failed to create VkDevice from Physical Device!");
}
}
