#include "vk_startup.h"
#include "utilities.h"
#include "application.h"
namespace vks
{

DeviceBuilder::DeviceBuilder(VkInstance instance, VkSurfaceKHR surface)
  : m_Instance{instance}, m_Surface{surface}
{}

void DeviceBuilder::set_required_extensions(const std::vector<const char*>& extensions)
{
  m_RequiredExtensions = extensions;
}

void DeviceBuilder::set_required_features(Aurora::Features features)
{
  m_EnabledFeatures = features;
}

VkPhysicalDevice DeviceBuilder::select_physical_device()
{
  uint32_t deviceCount;
  vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);
  AR_ASSERT(deviceCount != 0);

  std::vector<VkPhysicalDevice> devices(deviceCount);
  vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());
  
  int maxScore = 0;
  VkPhysicalDevice selected = VK_NULL_HANDLE;
  for (const auto& device : devices)
  {
    int score = rate_device(device);
    if (score > maxScore)
    {
      selected = device;
      maxScore = score;
    }
  }
  
  if (selected == VK_NULL_HANDLE)
  {
    AR_CORE_FATAL("Failed to find a suitable physical device");
    AR_STOP;
  }
  
  VkPhysicalDeviceProperties properties{};
  vkGetPhysicalDeviceProperties(selected, &properties);

  AR_CORE_INFO("Using {}", properties.deviceName);
  return selected;
}

void DeviceBuilder::build(VkPhysicalDevice& physicalDevice, VkDevice& device, Aurora::QueueFamilyIndices& indices, VkQueue& graphics, VkQueue& present)
{
  if (m_RequiredExtensions.has_value())
  {
    AR_CORE_WARN("Required Device Extensions: ");
    auto extensions = m_RequiredExtensions.value();
    for (const auto& name : extensions)
    {
      AR_CORE_WARN("\t{}", name);
    }
  }
  
  physicalDevice = select_physical_device();
  indices = find_queue_families(physicalDevice);

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

  VkDeviceCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  info.pQueueCreateInfos = queueCreateInfos.data();
  info.queueCreateInfoCount = (uint32_t) queueCreateInfos.size();
  
  info.pNext = &m_EnabledFeatures.get();
  info.pEnabledFeatures = nullptr;
  

  AR_ASSERT(m_RequiredExtensions.has_value());

  info.enabledExtensionCount = (uint32_t) m_RequiredExtensions.value().size();
  info.ppEnabledExtensionNames = m_RequiredExtensions.value().data();

  info.enabledLayerCount = 0;
  
  VK_CHECK_RESULT(vkCreateDevice(physicalDevice, &info, nullptr, &device));
  vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphics);
  vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &present);
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
    // or a present only queue for async
    VkBool32 presentSupport = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_Surface, &presentSupport);
    if (presentSupport) {
      indices.presentFamily = i;
    }

    if(indices.is_complete())
      break;
    
  }
  
  AR_ASSERT(indices.is_complete());
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
  // FIXME: check feature support! 
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
  
  return score;
}

SwapchainBuilder::SwapchainBuilder(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, Aurora::QueueFamilyIndices indices)
  : h_PhysicalDevice{physicalDevice}, h_Device{device}, h_Surface{surface}, m_Indices{indices}
{}


void SwapchainBuilder::build(VkSwapchainKHR& swapchain, std::vector<VkImage>& images, VkSurfaceFormatKHR& surfaceFormat, VkExtent2D& extent)
{
  SwapchainSupportDetails details = query_swapchain_support(h_PhysicalDevice);
  VkPresentModeKHR presentMode = choose_present_mode(details.presentModes);
  surfaceFormat = choose_surface_format(details.formats);
  extent = choose_extent(details.capabilities);

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
  
  VK_CHECK_RESULT(vkCreateSwapchainKHR(h_Device, &createInfo, nullptr, &swapchain));
  
  uint32_t count = 0;
  vkGetSwapchainImagesKHR(h_Device, swapchain, &count, nullptr);
  images.resize(count);
  vkGetSwapchainImagesKHR(h_Device, swapchain, &count, images.data());

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
        AR_CORE_INFO("Using VK_PRESENT_MODE_MAILBOX_KHR");
        return mode;
      }
  }
  AR_CORE_INFO("Using VK_PRESENT_MODE_FIFO_KHR");
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
    glfwGetFramebufferSize(Aurora::Window::get_handle(), &width, &height);
    VkExtent2D actualExtent = {
      (uint32_t) (width),
      (uint32_t) (height)
    };

    actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.width, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    
    return actualExtent;
  }
}

void setup_validation_layer_callback(VkInstance instance, VkDebugUtilsMessengerEXT& messenger, PFN_vkDebugUtilsMessengerCallbackEXT callback)
{
  VkDebugUtilsMessengerCreateInfoEXT createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  createInfo.pfnUserCallback = callback;
  VK_CHECK_RESULT(create_debug_messenger(instance, &createInfo, nullptr, &messenger));
}


VkResult create_debug_messenger(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
{
  auto fn = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
  if (fn == nullptr)
  {
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }

  return fn(instance, pCreateInfo, pAllocator, pDebugMessenger);
}

void destroy_debug_messenger(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
{
  auto fn = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
  if (fn == nullptr)
  {
    return;
  }
  fn(instance, debugMessenger, pAllocator);
}

} // namespace vkstartup
