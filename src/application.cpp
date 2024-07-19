#include "aurora_pch.h"
#include "application.h"

namespace Aurora {

Application::Application()
  : m_MainWindow{WIDTH, HEIGHT, "Aurora"}
{
  AR_CORE_INFO("Constructing Application");
  InitialiseVulkan();
}

Application::~Application()
{
  AR_CORE_INFO("Destroying Application");
  if(m_UseValidationLayers)
  {
    DestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
  }
  vkDestroyInstance(m_Instance, nullptr);
}

void Application::Run()
{
  //AR_CORE_TRACE("new frame!");
}

void Application::InitialiseVulkan()
{
  CheckInstanceExtensionSupport();
  CheckValidationLayersSupport();
  CreateInstance();
  SetupValidationLayerCallback();
  PickPhysicalDevice();
}

void Application::CreateInstance()
{
  VkApplicationInfo appInfo{};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pEngineName = "Aurora";
  appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.apiVersion = VK_API_VERSION_1_0;

  VkInstanceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.pApplicationInfo = &appInfo;

  uint32_t glfwExtensionCount = 0;
  const char** glfwExtensions;

  glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
  
  createInfo.enabledExtensionCount = static_cast<uint32_t>(m_RequiredExtensions.size());
  createInfo.ppEnabledExtensionNames = m_RequiredExtensions.data();
  
  if (m_UseValidationLayers)
  {
    createInfo.enabledLayerCount = static_cast<uint32_t>(m_ValidationLayers.size());
    createInfo.ppEnabledLayerNames = m_ValidationLayers.data();
  } else {
    createInfo.enabledLayerCount = 0;
  }
  
  VkResult result = vkCreateInstance(&createInfo, nullptr, &m_Instance);
  AR_ASSERT(result == VK_SUCCESS, "Vulkan Instance could not be created!");
  
}

void Application::CheckInstanceExtensionSupport()
{
  //FIX: no easy way to add extensions. it should be a VkExtensionProperties vector or set
  //that has some values set (extensions that are needed specifically for this app) & glfw extensions are added during runtime + MoltenVK
  // extensions are loaded using a const char* and a count , presumably the number of nulls it needs to skip.
  // Properties only has a C string and a spec version so its just a C string at the end of the day. 

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
  for (int i = 0; i < requiredCount; i++)
  {
    const char* extensionName = m_RequiredExtensions[i];
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

void Application::CheckValidationLayersSupport()
{
  if(!m_UseValidationLayers)
    return;

  AR_CORE_WARN("Required Validation Layers:");
  for(const auto& layer : m_ValidationLayers)
  {
    AR_CORE_WARN("\t{}", layer);
  }

  uint32_t layerCount;
  vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
  std::vector<VkLayerProperties> availableLayers(layerCount);
  vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

  for (const char* layerName : m_ValidationLayers)
  {
    bool LayerFound = false;
    for(const auto& layerProperties : availableLayers)
    {
      if (strcmp(layerName, layerProperties.layerName) == 0)
      {
        LayerFound = true;
        break;
      }
    }
   AR_ASSERT(!m_UseValidationLayers || LayerFound, "Required {} Validation Layer not supported!", layerName);
  }
}

void Application::SetupValidationLayerCallback()
{
  if (!m_UseValidationLayers)
    return;

  VkDebugUtilsMessengerCreateInfoEXT createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  createInfo.pfnUserCallback = debugCallback;
  createInfo.pUserData = nullptr;

  VkResult result = CreateDebugUtilsMessengerEXT(m_Instance, &createInfo, nullptr, &m_DebugMessenger);

  AR_ASSERT(result == VK_SUCCESS, "Failed to create custom Validation Layer Log callback!");
}

VKAPI_ATTR VkBool32 VKAPI_CALL Application::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {

    //std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
    AR_CORE_ERROR(pCallbackData->pMessage);
    return VK_FALSE;
}

VkResult Application::CreateDebugUtilsMessengerEXT(
  VkInstance instance,
  const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
  const VkAllocationCallbacks* pAllocator,
  VkDebugUtilsMessengerEXT* pDebugMessenger)
{
  auto fn = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
  return fn(instance, pCreateInfo, pAllocator, pDebugMessenger);
}

void Application::DestroyDebugUtilsMessengerEXT(
  VkInstance instance,
  VkDebugUtilsMessengerEXT debugMessenger,
  const VkAllocationCallbacks* pAllocator)
{
  auto fn = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
  return fn(instance, debugMessenger, pAllocator);
}

void Application::PickPhysicalDevice()
{
  //FIXME: make a device class?
  uint32_t deviceCount = 0;
  vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);
  AR_ASSERT(deviceCount != 0, "Failed to find GPUs with Vulkan support!");
  AR_CORE_INFO(deviceCount);
}

}
