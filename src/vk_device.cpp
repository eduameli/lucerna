#include "vk_device.h"
#include "aurora_pch.h"
#include "ar_asserts.h"

namespace Aurora 
{

DeviceBuilder::DeviceBuilder(VkInstance instance, VkSurfaceKHR surface)
  : m_Instance{instance}, m_Surface{surface} 
{
  features.f12.bufferDeviceAddress = VK_TRUE;
  features.f13.dynamicRendering = VK_TRUE;
  features.f13.synchronization2 = VK_TRUE;
}

bool DeviceBuilder::check_feature_support(VkPhysicalDevice device)
{
  Features query{};
  vkGetPhysicalDeviceFeatures2(device, &query.get());
  AR_LOG_ASSERT(query.f12.bufferDeviceAddress == VK_TRUE, "Feature bufferDeviceAddress not supported!");
  AR_LOG_ASSERT(query.f13.dynamicRendering == VK_TRUE, "Feature dynamicRendering not supported!");
  AR_LOG_ASSERT(query.f13.synchronization2 == VK_TRUE, "Feature synchronization2 not supported!");
  
  return true;
}

DeviceBuilder& DeviceBuilder::set_minimum_version(int major, int minor)
{
  m_MajorVersion = major;
  m_MinorVersion = minor;
  return *this;
}

DeviceBuilder& DeviceBuilder::set_required_extensions(std::span<const char*> extensions)
{
  m_RequiredExtensions = extensions;
  return *this;
}

VkPhysicalDevice DeviceBuilder::select_physical_device()
{

  uint32_t deviceCount;
  vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);
  AR_ASSERT(deviceCount != 0);

  std::vector<VkPhysicalDevice> devices(deviceCount);
  vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());
  
  int highscore = 0;
  VkPhysicalDevice selected{};
  for (const auto& device : devices)
  {
    int score = rate_physical_device(device);
    if (score > highscore)
    {
      highscore = score;
      selected = device;
    }
  }
  
  AR_LOG_ASSERT(selected != VK_NULL_HANDLE, "Failed to find a suitable physical device!");
  
  VkPhysicalDeviceProperties properties{};
  vkGetPhysicalDeviceProperties(selected, &properties);
  uint32_t version = properties.apiVersion;
  AR_CORE_INFO("Using {} [version {}.{}.{}]", properties.deviceName, VK_VERSION_MAJOR(version), VK_VERSION_MINOR(version), VK_VERSION_PATCH(version));

  return selected;
}

int DeviceBuilder::rate_physical_device(VkPhysicalDevice device)
{
  if (!is_device_suitable(device))
    return -1;

  
  VkPhysicalDeviceProperties properties;
  vkGetPhysicalDeviceProperties(device, &properties);
  
  uint32_t score = 0;
  switch (properties.deviceType)
  {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
      score += 5;
      break;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
      score += 4;
      break;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
      score += 3;
      break;
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
      score += 2;
      break;
    case VK_PHYSICAL_DEVICE_TYPE_OTHER:
      score += 1;
      break;
    default:
      break;
  }
  
  return score;
}

bool DeviceBuilder::is_device_suitable(VkPhysicalDevice device)
{
  return check_extension_support(device) && check_feature_support(device);
}

bool DeviceBuilder::check_extension_support(VkPhysicalDevice device)
{
  AR_LOG_ASSERT(m_RequiredExtensions.size() > 0, "Required Swapchain Extensions not requested");

  uint32_t extensionCount;
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
  std::vector<VkExtensionProperties> supportedExtensions(extensionCount);
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, supportedExtensions.data());
  
  for (const char* extensionName : m_RequiredExtensions)
  {
    bool found = false;
    for (const auto& extensionProperties : supportedExtensions)
    {
      if (strcmp(extensionName, extensionProperties.extensionName) == 0)
      {
        found = true;
        break;
      }
    }
    if (!found)
      return false;
  }
  return true; 
}

QueueFamilyIndices DeviceBuilder::find_queue_indices(VkPhysicalDevice device)
{
  QueueFamilyIndices indices{};

  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
  
  for (int i = 0; i < queueFamilyCount; i++)
  {
    auto queueFamily = queueFamilies[i];
    if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
    {
      indices.graphics = i;
    }
    // NOTE: should this find combined graphics&present,  distinct present, async compute?
    VkBool32 presentSupport = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_Surface, &presentSupport);
    if (presentSupport) {
      indices.present = i;
    }

    if(indices.is_complete())
      break;
  }
  
  AR_ASSERT(indices.is_complete());
  return indices;
}

DeviceContext DeviceBuilder::build()
{
  AR_ASSERT(m_Instance != VK_NULL_HANDLE);
  AR_ASSERT(m_Surface != VK_NULL_HANDLE);
  AR_ASSERT(m_RequiredExtensions.size() > 0);
 
  AR_CORE_WARN("Required Device Extensions:");
  for (auto& extension : m_RequiredExtensions)
  {
    AR_CORE_WARN("\t{}", extension);
  }

  VkDevice logicalDevice;
  VkPhysicalDevice physicalDevice = select_physical_device();
  QueueFamilyIndices familyIndices = find_queue_indices(physicalDevice);
  VkQueue graphicsQueue;
  VkQueue presentQueue;

  std::set<uint32_t> uniqueQueueFamilies = 
  {
    familyIndices.graphics.value(), familyIndices.present.value()
  };
  std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
  queueCreateInfos.reserve(uniqueQueueFamilies.size());

  float queuePriority = 1.0f;
  for (uint32_t queueFamily : uniqueQueueFamilies)
  {
    VkDeviceQueueCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    info.queueFamilyIndex = queueFamily;
    info.queueCount = 1;
    info.pQueuePriorities = &queuePriority;
    queueCreateInfos.push_back(info);
  }
  
  VkDeviceCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  info.pQueueCreateInfos = queueCreateInfos.data();
  info.queueCreateInfoCount = (uint32_t) queueCreateInfos.size();
  
  info.pNext = &features.get();
  info.pEnabledFeatures = nullptr;
  

  info.enabledExtensionCount = (uint32_t) m_RequiredExtensions.size();
  info.ppEnabledExtensionNames = m_RequiredExtensions.data();

  info.enabledLayerCount = 0;
  
  VK_CHECK_RESULT(vkCreateDevice(physicalDevice, &info, nullptr, &logicalDevice));
  vkGetDeviceQueue(logicalDevice, familyIndices.graphics.value(), 0, &graphicsQueue);
  vkGetDeviceQueue(logicalDevice, familyIndices.present.value(), 0, &presentQueue);
  
  return
  {
    .logical = logicalDevice,
    .physical = physicalDevice,
    .indices = familyIndices,
    .graphics = graphicsQueue,
    .present = presentQueue,
    .graphicsIndex = familyIndices.graphics.value(),
    .presentIndex = familyIndices.present.value(),
  };

  /* TODO: remove this comment!
   * select gpu device (rate?, suitable?, features?, extensions?) 
   * create queues (queue family indices, distinct present?, async?)
  */

}



} // namespace Aurora
