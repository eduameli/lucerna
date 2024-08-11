#include "engine.h"
#include "aurora_pch.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include "vk_initialisers.h"
#include "vk_startup.h"
#include "application.h"

#include <GLFW/glfw3.h>

// NOTE: needs to create instance ... contains device ... surface swapchain logic .. frame drawing

namespace Aurora
{
Engine::Engine()
{
  Window& win = Application::get_main_window();
  init_vulkan();
 
  // create surface 
  glfwCreateWindowSurface(h_Instance, win.get_handle(), nullptr, &h_Surface);

  vks::DeviceBuilder builder{ h_Instance };
  auto [logical, physical, queues]  = builder 
    .set_required_extensions(m_DeviceExtensions)
    .set_surface(h_Surface)
    .select_physical_device()
    .build();

  
  h_Device = logical;
  h_PhysicalDevice = physical;
  
  h_GraphicsQueue = queues.Graphics;
  h_PresentQueue = queues.Present;
}

Engine::~Engine()
{
  destroy_debug_messenger(h_Instance, h_DebugMessenger, nullptr);
  vkDestroySurfaceKHR(h_Instance, h_Surface, nullptr);

  vkDestroyDevice(h_Device, nullptr);
  vkDestroyInstance(h_Instance, nullptr);
}

void Engine::draw()
{}


void Engine::init_vulkan()
{
  check_instance_ext_support();
  check_validation_layer_support();
  create_instance();
  setup_validation_layer_callback();
}


void Engine::check_instance_ext_support()
{
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
  for (const char* name : m_InstanceExtensions)
  {
    AR_CORE_WARN("\t{}", name);
    bool extensionFound = false;
    for (const auto& extension : supportedExtensions)
    {
      if (strcmp(name, extension.extensionName) == 0)
      {
        extensionFound = true;
        break;
      }
    }
    AR_ASSERT(extensionFound, "Required {} Extension is not supported!", name);
  }
}

void Engine::check_validation_layer_support()
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

void Engine::create_instance()
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

void Engine::setup_validation_layer_callback()
{
  if(!m_UseValidationLayers)
    return;

  VkDebugUtilsMessengerCreateInfoEXT createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  createInfo.
  pfnUserCallback = validation_callback;
  VK_CHECK_RESULT(create_debug_messenger(h_Instance, &createInfo, nullptr, &h_DebugMessenger));
}




VkBool32 Engine::validation_callback(
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

VkResult Engine::create_debug_messenger(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
{
  auto fn = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
  if (fn == nullptr)
  {
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }

  return fn(instance, pCreateInfo, pAllocator, pDebugMessenger);
}

void Engine::destroy_debug_messenger(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
{
  auto fn = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
  if (fn == nullptr)
  {
    return;
  }
  fn(instance, debugMessenger, pAllocator);
}


/*
 
Engine::Engine(Window& window)
  : m_Window{window}, h_Device{m_Window}
{
  //FIX: could have a better "API" like vkbootstrap enum=
  h_GraphicsQueue = h_Device.GetGraphicsQueue();
  m_GraphicsQueueIndex = h_Device.indices.graphicsFamily.value();

  // create command pool / buffers
  VkCommandPoolCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  createInfo.pNext = nullptr;
  createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  createInfo.queueFamilyIndex = m_GraphicsQueueIndex;

  for (int i = 0; i < FRAME_OVERLAP; i++)
  {
    VK_CHECK_RESULT(vkCreateCommandPool(h_Device.GetLogicalDevice(), &createInfo, nullptr, &m_Frames[i].commandPool));
    
    VkCommandBufferAllocateInfo cmdInfo{};
    cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdInfo.pNext = nullptr;
    cmdInfo.commandPool = m_Frames[i].commandPool;
    cmdInfo.commandBufferCount = 1;
    cmdInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    VK_CHECK_RESULT(vkAllocateCommandBuffers(h_Device.GetLogicalDevice(), &cmdInfo, &m_Frames[i].mainCommandBuffer));
  }
  
  // init sync structures
  VkFenceCreateInfo fenceInfo{};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.pNext = nullptr;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  VkSemaphoreCreateInfo semaphoreInfo{};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  semaphoreInfo.pNext = nullptr;
  
  for (int i = 0; i < FRAME_OVERLAP; i++)
  {
    VK_CHECK_RESULT(vkCreateFence(h_Device.GetLogicalDevice(), &fenceInfo, nullptr, &m_Frames[i].renderFence));
    
    VK_CHECK_RESULT(vkCreateSemaphore(h_Device.GetLogicalDevice(), &semaphoreInfo, nullptr, &m_Frames[i].swapchainSemaphore));
    VK_CHECK_RESULT(vkCreateSemaphore(h_Device.GetLogicalDevice(), &semaphoreInfo, nullptr, &m_Frames[i].renderSemaphore));
  }

  VmaAllocatorCreateInfo allocatorInfo{};
  allocatorInfo.physicalDevice = h_Device.GetPhysicalDevice();
  allocatorInfo.device = h_Device.GetLogicalDevice();
  allocatorInfo.instance = h_Device.GetInstance();
  allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
  vmaCreateAllocator(&allocatorInfo, &m_Allocator);
  m_DeletionQueue.PushFunction([&]() {
    vmaDestroyAllocator(m_Allocator);
  });


}

*/

/*
Engine::~Engine()
{
  vkDeviceWaitIdle(h_Device.GetLogicalDevice());
  for (int i = 0; i < FRAME_OVERLAP; i++)
  {
    vkDestroyCommandPool(h_Device.GetLogicalDevice(), m_Frames[i].commandPool, nullptr);
    vkDestroyFence(h_Device.GetLogicalDevice(), m_Frames[i].renderFence, nullptr);
    vkDestroySemaphore(h_Device.GetLogicalDevice(), m_Frames[i].renderSemaphore, nullptr);
    vkDestroySemaphore(h_Device.GetLogicalDevice(), m_Frames[i].swapchainSemaphore, nullptr);

  }
  m_DeletionQueue.Flush();
}

void Engine::Draw()
{
  VK_CHECK_RESULT(vkWaitForFences(h_Device.GetLogicalDevice(), 1, &GetCurrentFrame().renderFence, true, 1000000000));
  GetCurrentFrame().deletionQueue.Flush();

  VK_CHECK_RESULT(vkResetFences(h_Device.GetLogicalDevice(), 1, &GetCurrentFrame().renderFence));

  uint32_t swapChainImageIndex;
  VK_CHECK_RESULT(vkAcquireNextImageKHR(h_Device.GetLogicalDevice(), h_Device.GetSwapchain(), 1000000000, GetCurrentFrame().swapchainSemaphore, nullptr, &swapChainImageIndex));
  

  //NOTE: vkhelper class for easy inits
  VkCommandBufferBeginInfo info{};
  info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  info.pNext = nullptr;
  info.pInheritanceInfo = nullptr;
  info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  VkCommandBuffer cmd = GetCurrentFrame().mainCommandBuffer;
  VK_CHECK_RESULT(vkResetCommandBuffer(cmd, 0));
  VK_CHECK_RESULT(vkBeginCommandBuffer(cmd, &info));
 
  TransitionImage(cmd, h_Device.m_SwapchainImages[swapChainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

  VkClearColorValue clearValue;
  float flash = std::abs(std::sin(m_FrameNumber / 120.0f));
  clearValue = {{0.0f, 0.0f, flash, 1.0f}};
  VkImageSubresourceRange clearRange{};
  clearRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  clearRange.baseMipLevel = 0;
  clearRange.levelCount = VK_REMAINING_MIP_LEVELS;
  clearRange.baseArrayLayer = 0;
  clearRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

  vkCmdClearColorImage(cmd, h_Device.m_SwapchainImages[swapChainImageIndex], VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);

  TransitionImage(cmd, h_Device.m_SwapchainImages[swapChainImageIndex], VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
  VK_CHECK_RESULT(vkEndCommandBuffer(cmd));

  VkCommandBufferSubmitInfo cmdInfo = vkinit::commandbuffer_submit_info(cmd);
  VkSemaphoreSubmitInfo waitInfo= vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, GetCurrentFrame().swapchainSemaphore);
  VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, GetCurrentFrame().renderSemaphore);
  VkSubmitInfo2 submit = vkinit::submit_info(&cmdInfo, &signalInfo, &waitInfo);
  
  VK_CHECK_RESULT(vkQueueSubmit2(h_Device.GetGraphicsQueue(), 1, &submit, GetCurrentFrame().renderFence))

  VkPresentInfoKHR present{};
  present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present.pNext = nullptr;
  present.pSwapchains = 
  &h_Device.GetSwapchain();
  present.swapchainCount = 1;
  present.pWaitSemaphores = &GetCurrentFrame().renderSemaphore;
  present.waitSemaphoreCount = 1;
  present.pImageIndices = &swapChainImageIndex;
  VK_CHECK_RESULT(vkQueuePresentKHR(h_Device.GetGraphicsQueue(), &present));

  m_FrameNumber++;


}

void Engine::TransitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout)
{

  VkImageMemoryBarrier2 imageBarrier{};
  imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  imageBarrier.pNext = nullptr;
  imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
  imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;
  imageBarrier.oldLayout = currentLayout;
  imageBarrier.newLayout = newLayout;
  
  VkImageAspectFlags aspectMask = (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
  
  VkImageSubresourceRange sub{};
  sub.aspectMask = aspectMask;
  sub.baseMipLevel = 0;
  sub.levelCount = VK_REMAINING_MIP_LEVELS;
  sub.baseArrayLayer = 0;
  sub.layerCount = VK_REMAINING_ARRAY_LAYERS;

  imageBarrier.subresourceRange = sub;
  imageBarrier.image = image;

  VkDependencyInfo depInfo{};
  depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  depInfo.pNext = nullptr;
  depInfo.imageMemoryBarrierCount = 1;
  depInfo.pImageMemoryBarriers = &imageBarrier;
  
  vkCmdPipelineBarrier2(cmd, &depInfo);
 }
*/
}
