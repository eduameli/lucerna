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

DeviceBuilder& DeviceBuilder::set_required_features(const VkPhysicalDeviceFeatures2& features)
{
  m_EnabledFeatures = features;
  return *this;
}

void DeviceBuilder::build()
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

  /*VkPhysicalDeviceFeatures2 f2 = m_EnabledFeatures.get_features();
  VkPhysicalDeviceVulkan13Features f13{};
  f13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  f13.dynamicRendering = VK_TRUE;
  f13.synchronization2 = VK_TRUE;
  
  VkPhysicalDeviceFeatures f{};

  VkPhysicalDeviceFeatures2 features{};
  features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  features.pNext = &f13;
  features.features = f;
  */
  VkDeviceCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  info.pQueueCreateInfos = queueCreateInfos.data();
  info.queueCreateInfoCount = (uint32_t) queueCreateInfos.size();

  info.pNext = &m_EnabledFeatures;
  info.pEnabledFeatures = nullptr;
  

  AR_ASSERT(m_RequiredExtensions.has_value(), "Required Extensions is empty! Even though you need to create a swapchain!");

  info.enabledExtensionCount = (uint32_t) m_RequiredExtensions.value().size();
  info.ppEnabledExtensionNames = m_RequiredExtensions.value().data();

  info.enabledLayerCount = 0;
  
  VK_CHECK_RESULT(vkCreateDevice(h_PhysicalDevice, &info, nullptr, &h_Device));
  get_queue_families();
}

VkPhysicalDevice DeviceBuilder::get_physical_device()
{
  AR_ASSERT(h_PhysicalDevice != VK_NULL_HANDLE, "Physical device is VK_NULL_HANDLE!");
  return h_PhysicalDevice;
}

VkDevice DeviceBuilder::get_logical_device()
{
  AR_ASSERT(h_Device != VK_NULL_HANDLE, "Logical device is VK_NULL_HANDLE!");
  return h_Device;
}

VkQueue DeviceBuilder::get_graphics_queue()
{
  return h_GraphicsQueue;
}

VkQueue DeviceBuilder::get_present_queue()
{
  return h_PresentQueue; 
}

void DeviceBuilder::get_queue_families()
{
  vkGetDeviceQueue(h_Device, m_QueueIndices.graphicsFamily.value(), 0, &h_GraphicsQueue);
  vkGetDeviceQueue(h_Device, m_QueueIndices.presentFamily.value(), 0, &h_PresentQueue);
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

SwapchainBuilder& SwapchainBuilder::set_devices(VkPhysicalDevice physicalDevice, VkDevice device)
{
  h_PhysicalDevice = physicalDevice;
  h_Device = device;
  return *this;
}

SwapchainBuilder& SwapchainBuilder::set_surface(VkSurfaceKHR surface)
{
  h_Surface = surface;
  return *this;
}

SwapchainBuilder& SwapchainBuilder::set_queue_indices(Aurora::QueueFamilyIndices indices)
{
  m_Indices = indices;
  return *this;
}

void SwapchainBuilder::build()
{
  SwapchainSupportDetails details = query_swapchain_support(h_PhysicalDevice);
  VkSurfaceFormatKHR surfaceFormat = choose_surface_format(details.formats);
  VkPresentModeKHR presentMode = choose_present_mode(details.presentModes);
  VkExtent2D extent = choose_extent(details.capabilities);

  uint32_t imageCount = details.capabilities.minImageCount;
  if (details.capabilities.maxImageCount > 0 && imageCount > details.capabilities.maxImageCount)
  {
    imageCount = details.capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  createInfo.surface = h_Surface;
  createInfo.minImageCount = imageCount;
  createInfo.imageFormat = surfaceFormat.format;
  createInfo.imageColorSpace = surfaceFormat.colorSpace;
  createInfo.imageExtent = extent;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

  uint32_t queueFamilyIndices[] = {m_Indices.graphicsFamily.value(), m_Indices.presentFamily.value()};

  if (m_Indices.graphicsFamily != m_Indices.presentFamily)
  {
    createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices = queueFamilyIndices;
  }
  else
  {
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;
    createInfo.pQueueFamilyIndices = nullptr;
  }

  createInfo.preTransform = details.capabilities.currentTransform;
  createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  
  createInfo.presentMode = presentMode;
  createInfo.clipped = VK_TRUE;
  createInfo.oldSwapchain = VK_NULL_HANDLE;
  
  VK_CHECK_RESULT(vkCreateSwapchainKHR(h_Device, &createInfo, nullptr, &h_Swapchain));
  /*
  vkGetSwapchainImagesKHR(h_Device, h_Swapchain, &imageCount, nullptr);
  m_SwapchainImages.resize(imageCount);
  vkGetSwapchainImagesKHR(h_Device, h_Swapchain, &imageCount, m_SwapchainImages.data());

  m_SwapchainExtent = extent;
  m_SwapchainImageFormat = surfaceFormat.format;
  */
  m_SurfaceFormat = surfaceFormat.format;
}

SwapchainBuilder::SwapchainSupportDetails SwapchainBuilder::query_swapchain_support(VkPhysicalDevice device)
{
  SwapchainSupportDetails details;

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, h_Surface, &details.capabilities);
  uint32_t formatCount;
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, h_Surface, &formatCount, nullptr);
  if (formatCount != 0)
  {
    details.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, h_Surface, &formatCount, details.formats.data());
  }

  uint32_t presentModeCount;
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, h_Surface, &presentModeCount, nullptr);
  if (presentModeCount != 0)
  {
    details.presentModes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, h_Surface, &presentModeCount, details.presentModes.data());
  }

  return details;
}

VkSurfaceFormatKHR SwapchainBuilder::choose_surface_format(const std::vector<VkSurfaceFormatKHR>& supportedFormats)
{
  for (const auto& format : supportedFormats)
  {
    if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
    {
      return format;
    }
  }
  return supportedFormats[0];
}

VkPresentModeKHR SwapchainBuilder::choose_present_mode(const std::vector<VkPresentModeKHR>& supportedModes)
{
  for (const auto& mode : supportedModes)
    {
      if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
      {
        return mode;
      }
  }
  return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D SwapchainBuilder::choose_extent(const VkSurfaceCapabilitiesKHR& capabilities)
{

  if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
  {
    return capabilities.currentExtent;
  }
  else
  {
    int width, height;
    glfwGetFramebufferSize(h_Window, &width, &height);
    VkExtent2D actualExtent = {
      (uint32_t) (width),
      (uint32_t) (height)
    };

    actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.width, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    return actualExtent;
  }
}

void SwapchainBuilder::get_swapchain_images(std::vector<VkImage>& images)
{
  uint32_t count = 0;
  vkGetSwapchainImagesKHR(h_Device, h_Swapchain, &count, nullptr);
  images.resize(count);
  vkGetSwapchainImagesKHR(h_Device, h_Swapchain, &count, images.data());
}

void SwapchainBuilder::get_image_views(std::vector<VkImageView>& views, const std::vector<VkImage>& images)
{
  views.resize(images.size());

  for (size_t i = 0; i < images.size(); i++)
  {
    VkImageViewCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = images[i];
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = m_SurfaceFormat;
    
    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    
    createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;

    VK_CHECK_RESULT(vkCreateImageView(h_Device, &createInfo, nullptr, &views[i]));
  }
}
} // namespace vkstartup
