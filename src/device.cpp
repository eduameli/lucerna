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
}

Device::~Device()
{
  vkDeviceWaitIdle(h_Device);
  vkDestroyDevice(h_Device, nullptr);
  vkDestroySurfaceKHR(h_Instance, h_Surface, nullptr);
  vkDestroyInstance(h_Instance, nullptr);
} 

void Device::CheckInstanceExtensionSupport()
{
  uint32_t requiredCount = 0;
  const char** requiredExtensions;
  requiredExtensions = glfwGetRequiredInstanceExtensions(&requiredCount);
  m_RequiredExtensions.insert(m_RequiredExtensions.end(), requiredExtensions, requiredExtensions + requiredCount);

  if (m_UseValidationLayers)
  {
    m_RequiredExtensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    requiredCount++;
  }
  
  uint32_t supportedCount = 0;
  vkEnumerateInstanceExtensionProperties(nullptr, &supportedCount, nullptr);
  std::vector<VkExtensionProperties> supportedExtensions(supportedCount);
  vkEnumerateInstanceExtensionProperties(nullptr, &supportedCount, supportedExtensions.data());

  AR_CORE_WARN("Required Instance Extensions: ");
  for (const char* extensionName : m_RequiredExtensions)
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
  appInfo.apiVersion = VK_API_VERSION_1_0;

  VkInstanceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.pApplicationInfo = &appInfo;
  createInfo.enabledExtensionCount = as<uint32_t>(m_RequiredExtensions.size());
  createInfo.ppEnabledExtensionNames = m_RequiredExtensions.data();

  if (m_UseValidationLayers)
  {
    createInfo.enabledLayerCount = as<uint32_t>(m_ValidationLayers.size());
    createInfo.ppEnabledLayerNames = m_ValidationLayers.data();
  }
  
  VkResult result = vkCreateInstance(&createInfo, nullptr, &h_Instance);
  AR_ASSERT(result == VK_SUCCESS, "Vulkan Instance could not be created!");
}

void Device::SetupValidationLayerCallback()
{
  // no for now!
}

void Device::CreateSurface()
{
  //VkResult result = glfwCreateWindowSurface(h_Instance, Window::GetWindow(), nullptr, &h_Surface);
  //AR_ASSERT(result == VK_SUCCESS, "Failed to create window surface!");
}

void Device::CreateLogicalDevice()
{
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
    AR_CORE_INFO("INDEX: {}", queueFamily);
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = queueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    queueCreateInfos.push_back(queueCreateInfo);
  }

  /*
  VkDeviceQueueCreateInfo queueCreateInfo{};
  queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
  queueCreateInfo.queueCount = 1;

  float queuePriority = 1.0f;
  queueCreateInfo.pQueuePriorities = &queuePriority;
  */
  AR_CORE_FATAL(queueCreateInfos.size());
  VkPhysicalDeviceFeatures deviceFeatures{};
  VkDeviceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  createInfo.pQueueCreateInfos = queueCreateInfos.data();
  createInfo.queueCreateInfoCount = as<uint32_t>(queueCreateInfos.size());
  createInfo.pEnabledFeatures = &deviceFeatures;
  
  //NOTE: device specific validation layers are deprecated!
  createInfo.enabledLayerCount = 0;

  VkResult result = vkCreateDevice(h_PhysicalDevice, &createInfo, nullptr, &h_Device);
  AR_ASSERT(result == VK_SUCCESS, "Failed to create VkDevice from Physical Device!");

  AR_ASSERT(h_GraphicsQueue == VK_NULL_HANDLE, "TRUE");
  
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

  return score;
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
  return indices;
}
}
