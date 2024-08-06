#include "aurora_pch.h"
#include "device.h"
#include <GLFW/glfw3.h>

namespace Aurora {

Device::Device(Window& window)
  : m_Window{window}
{
  // init vulkan, check validation, check instance extensions, create instance, get phsyical dev -> logical devic
  CheckInstanceExtensionSupport();
  CheckValidationLayersSupport();

  CreateInstance();
  SetupValidationLayerCallback();
  m_Window.CreateWindowSurface(h_Instance, h_Surface);
  CreateLogicalDevice();
  CreateSwapchain();
  CreateImageViews();
}

Device::~Device()
{
  vkDeviceWaitIdle(h_Device);
  for (auto imageView : m_SwapchainImageViews)
  {
    vkDestroyImageView(h_Device, imageView, nullptr);
  }
  vkDestroySwapchainKHR(h_Device, h_Swapchain, nullptr);
  DestroyDebugUtilsMessengerEXT(h_Instance, h_DebugMessenger, nullptr);
  vkDestroyDevice(h_Device, nullptr);
  vkDestroySurfaceKHR(h_Instance, h_Surface, nullptr);
  vkDestroyInstance(h_Instance, nullptr);
} 

void Device::CheckInstanceExtensionSupport()
{
  //Timer scoped("Device::CheckInstanceSupport");

  uint32_t requiredCount = 0;
  const char** requiredExtensions;
  requiredExtensions = glfwGetRequiredInstanceExtensions(&requiredCount);
  m_InstanceExtensions.insert(m_InstanceExtensions.end(), requiredExtensions, requiredExtensions + requiredCount);

  if (m_UseValidationLayers)
  {
    m_InstanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    requiredCount++;
  }
  
  uint32_t supportedCount = 0;
  vkEnumerateInstanceExtensionProperties(nullptr, &supportedCount, nullptr);
  std::vector<VkExtensionProperties> supportedExtensions(supportedCount);
  vkEnumerateInstanceExtensionProperties(nullptr, &supportedCount, supportedExtensions.data());
  
  AR_CORE_WARN("Required Instance Extensions: ");
  for (const char* extensionName : m_InstanceExtensions)
  {
    AR_CORE_WARN("\t{}", extensionName);

    bool extensionFound = false;
    for (const auto& extension : supportedExtensions)
    {
      if (strcmp(extensionName, extension.extensionName) == 0)
      {
        extensionFound = true;
        break;
      }
    }
    AR_ASSERT(extensionFound, "Required {} Extension not supported!", extensionName);
  }

}

void Device::CheckValidationLayersSupport()
{
  if (!m_UseValidationLayers)
    return;

  uint32_t layerCount = 0;
  vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
  std::vector<VkLayerProperties> supportedLayers(layerCount);
  vkEnumerateInstanceLayerProperties(&layerCount, supportedLayers.data());
  
  AR_CORE_WARN("Required Validation Layers: ");
  for (const char* layerName : m_ValidationLayers)
  {
    AR_CORE_WARN("\t{}", layerName);
    bool layerFound = false;
    for (const auto& layerProperties : supportedLayers)
    {
      if (strcmp(layerName, layerProperties.layerName) == 0)
      {
        layerFound = true;
        break;
      }
    }
    AR_ASSERT(layerFound, "Required {} Validation Layer not supported!", layerName);
  }

}

void Device::CreateInstance()
{
  VkApplicationInfo appInfo{};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pEngineName = "Aurora";
  appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 1);
  appInfo.apiVersion = VK_API_VERSION_1_3;

  VkInstanceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.pApplicationInfo = &appInfo;
  createInfo.enabledExtensionCount = as<uint32_t>(m_InstanceExtensions.size());
  createInfo.ppEnabledExtensionNames = m_InstanceExtensions.data();

  if (m_UseValidationLayers)
  {
    createInfo.enabledLayerCount = as<uint32_t>(m_ValidationLayers.size());
    createInfo.ppEnabledLayerNames = m_ValidationLayers.data();
  }
  
  VK_CHECK_RESULT(vkCreateInstance(&createInfo, nullptr, &h_Instance));
}

void Device::SetupValidationLayerCallback()
{
  // no for now!
  if(!m_UseValidationLayers)
    return;

  VkDebugUtilsMessengerCreateInfoEXT createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  createInfo.pfnUserCallback = debugCallback;
  VK_CHECK_RESULT(CreateDebugUtilsMessengerEXT(h_Instance, &createInfo, nullptr, &h_DebugMessenger));
}

VkBool32 Device::debugCallback(
  VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
  VkDebugUtilsMessageTypeFlagsEXT messageType,
  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
  void* pUserData)
{
  if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
  {
    AR_CORE_ERROR("VALIDATION LAYER ERROR: {}", pCallbackData->pMessage);
  }

  return VK_FALSE;
}

VkResult Device::CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
{
  auto fn = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
  if (fn == nullptr)
  {
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }

  return fn(instance, pCreateInfo, pAllocator, pDebugMessenger);
}

void Device::DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
{
  auto fn = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
  if (fn == nullptr)
  {
    return;
  }
  fn(instance, debugMessenger, pAllocator);
}

void Device::CreateLogicalDevice()
{
  AR_CORE_WARN("Required Device Extensions: ");
  for (const auto& name : m_DeviceExtensions)
  {
    AR_CORE_WARN("\t{}", name);
  }


  PickPhysicalDevice();
  auto indices = FindQueueFamilies(h_PhysicalDevice);
  std::set<uint32_t> uniqueQueueFamilies = 
  {
    indices.graphicsFamily.value(), indices.presentFamily.value()
  };

  std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
  queueCreateInfos.reserve(uniqueQueueFamilies.size());

  float queuePriority = 1.0f;
  for (uint32_t queueFamily : uniqueQueueFamilies)
  {
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = queueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    queueCreateInfos.push_back(queueCreateInfo);
  }


  VkPhysicalDeviceFeatures deviceFeatures{};
  VkPhysicalDeviceVulkan13Features features13{};
  features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  features13.synchronization2 = VK_TRUE;
  features13.dynamicRendering = VK_TRUE;

  VkPhysicalDeviceFeatures2 features2{};
  features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  features2.pNext = &features13;
  features2.features = deviceFeatures;



  VkDeviceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  createInfo.pQueueCreateInfos = queueCreateInfos.data();
  createInfo.queueCreateInfoCount = as<uint32_t>(queueCreateInfos.size());
  
  createInfo.pNext = &features2;
  createInfo.pEnabledFeatures = nullptr;

  
  createInfo.enabledExtensionCount = as<uint32_t>(m_DeviceExtensions.size());
  createInfo.ppEnabledExtensionNames = m_DeviceExtensions.data();

  //NOTE: device specific validation layers are deprecated!
  createInfo.enabledLayerCount = 0;

  VK_CHECK_RESULT(vkCreateDevice(h_PhysicalDevice, &createInfo, nullptr, &h_Device));

  vkGetDeviceQueue(h_Device, indices.graphicsFamily.value(), 0, &h_GraphicsQueue);
  vkGetDeviceQueue(h_Device, indices.presentFamily.value(), 0, &h_PresentQueue);
  
  AR_ASSERT(h_GraphicsQueue != VK_NULL_HANDLE, "Graphics Queue is VK_HANDLE_NULL!");
  AR_ASSERT(h_PresentQueue != VK_NULL_HANDLE, "Present Queue is VK_HANDLE_NULL!");
}




void Device::PickPhysicalDevice()
{
  uint32_t deviceCount;
  vkEnumeratePhysicalDevices(h_Instance, &deviceCount, nullptr);
  AR_ASSERT(deviceCount != 0, "Failed to find GPUs with Vulkan support!");
  
  std::vector<VkPhysicalDevice> devices(deviceCount);
  vkEnumeratePhysicalDevices(h_Instance, &deviceCount, devices.data());
  
  std::multimap<int, VkPhysicalDevice> candidates;

  for (const auto& device : devices)
  {
    int score = RateDeviceSuitability(device);
    candidates.insert({score, device});
  }

  if (candidates.rbegin()->first > 0)
  {
    h_PhysicalDevice = candidates.rbegin()->second; 
  }
  

  AR_ASSERT(h_PhysicalDevice != VK_NULL_HANDLE , "Failed to reference highest scored Physical Device!");
  //h_PhysicalDevice = devices[1];
  //VkPhysicalDeviceProperties deviceProperties;
  //vkGetPhysicalDeviceProperties(h_PhysicalDevice, &deviceProperties);
  //AR_CORE_INFO("Using {} as the Physical Device", deviceProperties.deviceName);

  //FIX: vkGetDeviceQueue - index is hardcoded at 0 right now because we are only creating one queue
  // from this family!
  //CreateLogicalDevice();
  //auto indices = FindQueueFamilies(h_PhysicalDevice);
  // vkGetDeviceQueue(h_Device, indices.graphicsFamily.value(), 0, &h_GraphicsQueue);
}

uint32_t Device::RateDeviceSuitability(VkPhysicalDevice device)
{
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

  if (!IsDeviceSuitable(device))
  {
    score = 0;
  }

  return score;
}


bool Device::IsDeviceSuitable(VkPhysicalDevice device)
{
  bool extensionsSupported = CheckDeviceExtensionSupport(device);

  bool SwapChainAdequeate = false;
  if (extensionsSupported)
  {
    SwapChainSupportDetails details = QuerySwapChainSupport(device);
    SwapChainAdequeate = !details.formats.empty() && !details.presentModes.empty();
  }
  
  return extensionsSupported && SwapChainAdequeate;
}

bool Device::CheckDeviceExtensionSupport(VkPhysicalDevice device)
{
  uint32_t extensionCount;
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
  std::vector<VkExtensionProperties> supportedExtensions(extensionCount);
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, supportedExtensions.data());

  std::set<std::string> requiredExtensions(m_DeviceExtensions.begin(), m_DeviceExtensions.end());
  for (const auto& extension : supportedExtensions)
  {
    requiredExtensions.erase(extension.extensionName);
  }

  return requiredExtensions.empty();
}

Device::QueueFamilyIndices Device::FindQueueFamilies(VkPhysicalDevice device)
{
  QueueFamilyIndices indices;
  
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

    if(indices.IsComplete())
      break;
    
  }
  
  AR_ASSERT(indices.IsComplete(), "Succesfully found all required Queue Families!");
  this->indices = indices;
  return indices;
}


Device::SwapChainSupportDetails Device::QuerySwapChainSupport(VkPhysicalDevice device)
{
  SwapChainSupportDetails details;

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

VkSurfaceFormatKHR Device::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& supportedFormats)
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

VkPresentModeKHR Device::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& supportedModes)
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

VkExtent2D Device::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
  if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
  {
    return capabilities.currentExtent;
  }
  else
  {
    int width, height;
    glfwGetFramebufferSize(m_Window.GetHandle(), &width, &height);
    VkExtent2D actualExtent = {
      as<uint32_t>(width),
      as<uint32_t>(height)
    };

    actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.width, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    return actualExtent;
  }
}

void Device::CreateSwapchain()
{
  SwapChainSupportDetails details = QuerySwapChainSupport(h_PhysicalDevice);
  VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(details.formats);
  VkPresentModeKHR presentMode = ChooseSwapPresentMode(details.presentModes);
  VkExtent2D extent = ChooseSwapExtent(details.capabilities);

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

  QueueFamilyIndices indices = FindQueueFamilies(h_PhysicalDevice);
  uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

  if (indices.graphicsFamily != indices.presentFamily)
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

  vkGetSwapchainImagesKHR(h_Device, h_Swapchain, &imageCount, nullptr);
  m_SwapchainImages.resize(imageCount);
  vkGetSwapchainImagesKHR(h_Device, h_Swapchain, &imageCount, m_SwapchainImages.data());

  m_SwapchainExtent = extent;
  m_SwapchainImageFormat = surfaceFormat.format;


  VkExtent3D drawImageExtent{extent.width, extent.height, 1};


}

void Device::CreateImageViews()
{
  m_SwapchainImageViews.resize(m_SwapchainImages.size());

  for (size_t i = 0; i < m_SwapchainImages.size(); i++)
  {
    VkImageViewCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = m_SwapchainImages[i];
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = m_SwapchainImageFormat;
    
    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    
    createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;

    VK_CHECK_RESULT(vkCreateImageView(h_Device, &createInfo, nullptr, &m_SwapchainImageViews[i]));
  }
}

}
