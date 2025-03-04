#include "vk_device.h"
#include "lucerna_pch.h"
#include "la_asserts.h"
#include "vulkan/vulkan_core.h"

namespace Lucerna {

DeviceContextBuilder::DeviceContextBuilder(VkInstance instance, VkSurfaceKHR surface)
  : m_Instance{instance}, m_Surface{surface} 
{
  features.f1.wideLines = VK_TRUE;
  features.f1.fillModeNonSolid = VK_TRUE;
  features.f1.multiDrawIndirect = VK_TRUE;
  features.f1.drawIndirectFirstInstance = VK_TRUE;
  
  features.f11.shaderDrawParameters = VK_TRUE;

  features.f12.descriptorIndexing = VK_TRUE;
  features.f12.descriptorBindingPartiallyBound = VK_TRUE;
  features.f12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
  features.f12.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE;
  features.f12.drawIndirectCount = VK_TRUE;
  
  features.f12.runtimeDescriptorArray = VK_TRUE;
  features.f12.bufferDeviceAddress = VK_TRUE;
  features.f12.scalarBlockLayout = VK_TRUE;
  features.f13.dynamicRendering = VK_TRUE;
  features.f13.synchronization2 = VK_TRUE;

  // out of date
  LA_LOG_WARN("Required Features: ");
  LA_LOG_INFO("\twideLines");
  LA_LOG_INFO("\tmultiDrawIndirect");
  LA_LOG_INFO("\tfillModeNonSolid");
  LA_LOG_INFO("\tshaderDrawParameters");
  LA_LOG_INFO("\tdescriptorIndexing");
  LA_LOG_INFO("\tdescriptorBindingPartiallyBound");
  LA_LOG_INFO("\tdescriptorBindingSampledImageUpdateAfterBind");
  LA_LOG_INFO("\tdescriptorBindingStorageImageUpdateAfterBind");
  LA_LOG_INFO("\truntimeDescriptorArray");
  LA_LOG_INFO("\tbufferDeviceAddress");
  LA_LOG_INFO("\tscalarBlockLayout");
  LA_LOG_INFO("\tdynamicRendering");
  LA_LOG_INFO("\tsynchronization2");

}

bool DeviceContextBuilder::check_feature_support(VkPhysicalDevice device)
{
  Features query{};
  vkGetPhysicalDeviceFeatures2(device, &query.get());

  // VkPhysicalDeviceSubgroupProperties subgroupProperties{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES, .pNext = nullptr};
  // VkPhysicalDeviceProperties2 deviceProperties2{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &subgroupProperties };
  // vkGetPhysicalDeviceProperties2(device, &deviceProperties2);

  
  
  return
    query.features.features.fillModeNonSolid &&
    query.features.features.wideLines &&
    query.features.features.multiDrawIndirect &&
    query.features.features.drawIndirectFirstInstance &&
    query.f11.shaderDrawParameters &&
    query.f12.descriptorIndexing &&
    query.f12.descriptorBindingPartiallyBound &&
    query.f12.descriptorBindingSampledImageUpdateAfterBind &&
    query.f12.descriptorBindingStorageImageUpdateAfterBind &&
    query.f12.runtimeDescriptorArray &&
    query.f12.bufferDeviceAddress &&
    query.f12.scalarBlockLayout &&
    query.f12.drawIndirectCount &&
    query.f13.dynamicRendering &&
    query.f13.synchronization2;
}

DeviceContextBuilder& DeviceContextBuilder::set_minimum_version(int major, int minor)
{
  m_MajorVersion = major;
  m_MinorVersion = minor;
  return *this;
}

DeviceContextBuilder& DeviceContextBuilder::set_required_extensions(std::span<const char*> extensions)
{
  m_RequiredExtensions = extensions;
  return *this;
}

DeviceContextBuilder& DeviceContextBuilder::set_preferred_gpu_type(VkPhysicalDeviceType type)
{
  LA_LOG_ASSERT(type != VK_PHYSICAL_DEVICE_TYPE_MAX_ENUM, "Invalid preferred physical device type");
  m_PreferredDeviceType = type;
  return *this;
}

VkPhysicalDevice DeviceContextBuilder::select_physical_device()
{

  uint32_t deviceCount;
  vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);
  LA_ASSERT(deviceCount != 0);


  LA_LOG_DEBUG("device count {}", deviceCount);
  
  std::vector<VkPhysicalDevice> devices(deviceCount);
  vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());
  
  int highscore = 0;
  VkPhysicalDevice selected{};
  for (const auto& device : devices)
  {
    int score = rate_physical_device(device);
    LA_LOG_DEBUG("score... {}", score);
    if (score > highscore)
    {
      highscore = score;
      selected = device;
    }
  }
  LA_LOG_ASSERT(selected != VK_NULL_HANDLE, "Failed to find a suitable physical device!");
  
  VkPhysicalDeviceProperties properties{};
  vkGetPhysicalDeviceProperties(selected, &properties);
  uint32_t version = properties.apiVersion;
  return selected;
}

int DeviceContextBuilder::rate_physical_device(VkPhysicalDevice device)
{
  if (!is_device_suitable(device))
    return -1;

  
  VkPhysicalDeviceProperties properties;
  vkGetPhysicalDeviceProperties(device, &properties);
  
  if (properties.deviceType == m_PreferredDeviceType)
  {
    return INT_MAX;
  }
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

bool DeviceContextBuilder::is_device_suitable(VkPhysicalDevice device)
{
  return check_feature_support(device) && check_extension_support(device);
}

bool DeviceContextBuilder::check_extension_support(VkPhysicalDevice device)
{
  LA_LOG_ASSERT(m_RequiredExtensions.size() > 0, "Required Swapchain Extensions not requested");

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

QueueFamilyIndices DeviceContextBuilder::find_queue_indices(VkPhysicalDevice device)
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
  
  LA_ASSERT(indices.is_complete());
  return indices;
}

DeviceContext DeviceContextBuilder::build()
{
  LA_ASSERT(m_Instance != VK_NULL_HANDLE);
  LA_ASSERT(m_Surface != VK_NULL_HANDLE);
  LA_ASSERT(m_RequiredExtensions.size() > 0);
 
  LA_LOG_WARN("Required Device Extensions: ");
  for (auto& extension : m_RequiredExtensions)
  {
    LA_LOG_INFO("\t{}", extension);
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
   
  volkLoadDevice(logicalDevice);

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
}



} // namespace Lucerna
