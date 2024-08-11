#include "vk_startup.h"
#include "utilities.h"
namespace vks
{

DeviceBuilder::DeviceBuilder(VkInstance instance)
  : h_Instance{instance}, m_MajorVersion{0}, m_MinorVersion{0}, m_EnabledFeatures{}
{}

DeviceBuilder& DeviceBuilder::set_required_extensions(const std::vector<const char*>& extensions)
{
  m_RequiredExtensions = extensions;
  return *this;
}


DeviceBuilder& DeviceBuilder::select_physical_device()
{
  uint32_t deviceCount;
  vkEnumeratePhysicalDevices(h_Instance, &deviceCount, nullptr);
  AR_ASSERT(deviceCount != 0, "Failed to find any supported Physical Device!");

  std::vector<VkPhysicalDevice> devices(deviceCount);
  vkEnumeratePhysicalDevices(h_Instance, &deviceCount, devices.data());
  
  int maxScore = 0;
  for (const auto& device : devices)
  {
    int score = rate_device(device);
    if (score > maxScore)
    {
      h_PhysicalDevice = device;
      maxScore = score;
    }
  }

  AR_ASSERT(h_PhysicalDevice != VK_NULL_HANDLE, "No suitable device found!");
  return *this;
}

DeviceBuilder& DeviceBuilder::set_required_features(const DeviceBuilder::DeviceFeatures& features)
{
  m_EnabledFeatures = features;
  return *this;
}

std::tuple<VkDevice, VkPhysicalDevice, Aurora::QueueFamilies> DeviceBuilder::build()
{
  auto indices = find_queue_families(h_PhysicalDevice);
  m_QueueIndices = indices; 

  std::set<uint32_t> uniqueQueueFamilies = 
  {
    indices.graphicsFamily.value(), indices.presentFamily.value()
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

  VkPhysicalDeviceFeatures2 features = m_EnabledFeatures.get_features();

  VkDeviceCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  info.pQueueCreateInfos = queueCreateInfos.data();
  info.queueCreateInfoCount = (uint32_t) queueCreateInfos.size();

  info.pNext = &features;
  info.pEnabledFeatures = nullptr;
  

  AR_ASSERT(m_RequiredExtensions.has_value(), "Required Extensions is empty! Even though you need to create a swapchain!");

  info.enabledExtensionCount = (uint32_t) m_RequiredExtensions.value().size();
  info.ppEnabledExtensionNames = m_RequiredExtensions.value().data();

  info.enabledLayerCount = 0;
  
  VK_CHECK_RESULT(vkCreateDevice(h_PhysicalDevice, &info, nullptr, &h_Device));
  
  return {h_Device, h_PhysicalDevice, get_queue_families()};
}

Aurora::QueueFamilies DeviceBuilder::get_queue_families()
{
  Aurora::QueueFamilies families{};
  vkGetDeviceQueue(h_Device, m_QueueIndices.graphicsFamily.value(), 0, &families.Graphics);
  vkGetDeviceQueue(h_Device, m_QueueIndices.presentFamily.value(), 0, &families.Present);
  return families;
}

DeviceBuilder& DeviceBuilder::set_surface(VkSurfaceKHR surface)
{
  h_Surface = surface;
  return *this;
}

Aurora::QueueFamilyIndices DeviceBuilder::find_queue_families(VkPhysicalDevice device)
{
  Aurora::QueueFamilyIndices indices{};

  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
  
  for (int i = 0; i < queueFamilyCount; i++)
  {
    auto queueFamily = queueFamilies[i];
    if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
    {
      indices.graphicsFamily = i;
    }
  
    //NOTE: could add logic to explicitly prefer a physical device that supports
    // both drawing and presentation in the same queue for improved performance!
    VkBool32 presentSupport = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, h_Surface, &presentSupport);
    if (presentSupport) {
      indices.presentFamily = i;
    }

    if(indices.is_complete())
      break;
    
  }
  
  AR_ASSERT(indices.is_complete(), "Succesfully found all required Queue Families!");
  return indices;
}

bool DeviceBuilder::check_extension_support(VkPhysicalDevice device)
{
  if (!m_RequiredExtensions.has_value())
    return true;
  
  auto& extensions = m_RequiredExtensions.value();

  uint32_t extensionCount;
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
  std::vector<VkExtensionProperties> supportedExtensions(extensionCount);
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, supportedExtensions.data());
  std::set<std::string> requiredExtensions(extensions.begin(), extensions.end());
  for (const auto& extension : supportedExtensions)
  {
    requiredExtensions.erase(extension.extensionName);
  }
  return requiredExtensions.empty();
}


bool DeviceBuilder::is_device_suitable(VkPhysicalDevice device)
{
  return check_extension_support(device);
}

int DeviceBuilder::rate_device(VkPhysicalDevice device)
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
  
  AR_CORE_INFO("{} has score {}", properties.deviceName, score);
  return score;
}

} // namespace vkstartup
