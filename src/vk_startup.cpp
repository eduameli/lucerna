#include "vk_startup.h"

namespace vks
{

PhysicalDeviceSelector::PhysicalDeviceSelector(VkInstance instance)
{

}

PhysicalDeviceSelector& PhysicalDeviceSelector::set_minimum_version(int major, int minor)
{
  m_MajorVersion = major;
  m_MinorVersion = minor;
  return *this;
}

PhysicalDeviceSelector& PhysicalDeviceSelector::set_required_extensions(const std::vector<const char*>& extensions)
{
  m_RequiredExtensions = extensions;
  return *this;
}


std::tuple<VkPhysicalDevice, PhysicalDeviceSelector::QueueFamilyIndices> PhysicalDeviceSelector::select()
{
  uint32_t deviceCount;
  vkEnumeratePhysicalDevices(h_Instance, &deviceCount, nullptr);
  AR_ASSERT(deviceCount != 0, "Failed to find any supported Physical Device!");

  std::vector<VkPhysicalDevice> devices(deviceCount);
  vkEnumeratePhysicalDevices(h_Instance, &deviceCount, devices.data());

  std::multimap<int, VkPhysicalDevice> candidates;

  VkPhysicalDevice selected{};
  for (const auto& device : devices)
  {
    uint32_t score = rate_device(device);
    candidates.insert({score, device});
  }

  if (candidates.rbegin()->first > 0)
  {
    // FIXME: stopped working here, query families and return here! to make api more lean
    return {candidates.rbegin()->second, {0, 0}};
  }
  
  // not found :(
  return {};
}

bool PhysicalDeviceSelector::check_extension_support(VkPhysicalDevice device)
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

bool PhysicalDeviceSelector::is_device_suitable(VkPhysicalDevice device)
{
  return check_extension_support(device);
}

uint32_t PhysicalDeviceSelector::rate_device(VkPhysicalDevice device)
{
  if (!is_device_suitable(device))
    return 0;

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

} // namespace vkstartup
