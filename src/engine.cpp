#include "engine.h"
#include "aurora_pch.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include "vk_initialisers.h"
#include "vk_startup.h"
#include "application.h"

#include <GLFW/glfw3.h>
#include "vk_images.h"
#include "vk_descriptors.h"
#include "vk_pipelines.h"

// NOTE: needs to create instance ... contains device ... surface swapchain logic .. frame drawing

namespace Aurora
{
Engine::Engine()
{
  init_vulkan();
  // FIXME: m_Indices.graphicsFamily.value() is rlly bad, maybe just add a new variable keep it simple!
  // common vulkan mistake = trying to write a magical wrapper for everying eg. SwapchainBuilder and DeviceBuilder which couldve just been some functions 
  // or using a struct for options, will rework api write it down options n compare.
  VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(m_Indices.graphicsFamily.value(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
  for (int i = 0; i < FRAME_OVERLAP; i++)
  {
    VK_CHECK_RESULT(vkCreateCommandPool(h_Device, &commandPoolInfo, nullptr, &m_Frames[i].commandPool));
    VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(m_Frames[i].commandPool, 1);
    VK_CHECK_RESULT(vkAllocateCommandBuffers(h_Device, &cmdAllocInfo, &m_Frames[i].mainCommandBuffer));
  }

  // this could be init_command_buffers
  // now init sync structures
  VkFenceCreateInfo fence = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
  VkSemaphoreCreateInfo semaphore = vkinit::semaphore_create_info();
  for (int i = 0; i < FRAME_OVERLAP; i++)
  {
    VK_CHECK_RESULT(vkCreateFence(h_Device, &fence, nullptr, &m_Frames[i].renderFence));
    VK_CHECK_RESULT(vkCreateSemaphore(h_Device, &semaphore, nullptr, &m_Frames[i].swapchainSemaphore));
    VK_CHECK_RESULT(vkCreateSemaphore(h_Device, &semaphore, nullptr, &m_Frames[i].renderSemaphore));
  }

  init_descriptors();
  init_pipelines();
} 

Engine::~Engine()
{
  vkDeviceWaitIdle(h_Device);

  destroy_debug_messenger(h_Instance, h_DebugMessenger, nullptr);
  vkDestroySwapchainKHR(h_Device, h_Swapchain, nullptr);
  vkDestroySurfaceKHR(h_Instance, h_Surface, nullptr);
  for (auto view : m_SwapchainImageViews)
  {
    vkDestroyImageView(h_Device, view, nullptr);
  }
  for (int i = 0; i < FRAME_OVERLAP; i++)
  {
    vkDestroyCommandPool(h_Device, m_Frames[i].commandPool, nullptr);

    vkDestroyFence(h_Device, m_Frames[i].renderFence, nullptr);
    vkDestroySemaphore(h_Device, m_Frames[i].renderSemaphore, nullptr);
    vkDestroySemaphore(h_Device, m_Frames[i].swapchainSemaphore, nullptr);

    m_Frames[i].deletionQueue.flush();
  }
  
  m_DeletionQueue.flush();

  vkDestroyDevice(h_Device, nullptr);
  vkDestroyInstance(h_Instance, nullptr);
}

void Engine::draw()
{
  Timer frametime; 
  VK_CHECK_RESULT(vkWaitForFences(h_Device, 1, &get_current_frame().renderFence, true, 1000000000));
  VK_CHECK_RESULT(vkResetFences(h_Device, 1, &get_current_frame().renderFence));

  uint32_t swapchainImageIndex;
  VK_CHECK_RESULT(vkAcquireNextImageKHR(h_Device, h_Swapchain, 1000000000, get_current_frame().swapchainSemaphore, nullptr, &swapchainImageIndex));
  VkCommandBuffer cmd = get_current_frame().mainCommandBuffer;
  VK_CHECK_RESULT(vkResetCommandBuffer(cmd, 0));
  
  m_DrawExtent.width = m_DrawImage.imageExtent.width;
  m_DrawExtent.height = m_DrawImage.imageExtent.height;

  VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
  VK_CHECK_RESULT(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

  vkutil::transition_image(cmd, m_DrawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
  
  draw_background(cmd);
  
  vkutil::transition_image(cmd, m_DrawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  vkutil::transition_image(cmd, m_SwapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  
  // NOTE: uses blit which is slower than other options but less restrictive, could change!
  vkutil::copy_image_to_image(cmd, m_DrawImage.image, m_SwapchainImages[swapchainImageIndex], m_DrawExtent, m_DrawExtent /* FIXME: this should be swapchain extent?? */);
  
  vkutil::transition_image(cmd, m_SwapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
  VK_CHECK_RESULT(vkEndCommandBuffer(cmd));

  // submit 
  VkCommandBufferSubmitInfo cmdInfo = vkinit::commandbuffer_submit_info(cmd);

  VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, get_current_frame().swapchainSemaphore);
  VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, get_current_frame().renderSemaphore);

  VkSubmitInfo2 submit = vkinit::submit_info(&cmdInfo, &signalInfo, &waitInfo);
  VK_CHECK_RESULT(vkQueueSubmit2(h_GraphicsQueue, 1, &submit, get_current_frame().renderFence));
  
  VkPresentInfoKHR info{};
  info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  info.pNext = nullptr;
  info.pSwapchains = &h_Swapchain;
  info.swapchainCount = 1;
  info.pWaitSemaphores = &get_current_frame().renderSemaphore;
  info.waitSemaphoreCount = 1;

  info.pImageIndices = &swapchainImageIndex;

  VK_CHECK_RESULT(vkQueuePresentKHR(h_GraphicsQueue, &info));
  m_FrameNumber++;
}


void Engine::init_vulkan()
{
  check_instance_ext_support();
  check_validation_layer_support();
  create_instance();
  setup_validation_layer_callback();
  glfwCreateWindowSurface(h_Instance, Application::get_main_window().get_handle(), nullptr, &h_Surface);
  create_device();
  //create_swapchain();

  VmaAllocatorCreateInfo allocatorInfo{};
  allocatorInfo.physicalDevice = h_PhysicalDevice;
  allocatorInfo.device = h_Device;
  allocatorInfo.instance = h_Instance;
  allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
  vmaCreateAllocator(&allocatorInfo, &m_Allocator);
  m_DeletionQueue.push_function([&]() {vmaDestroyAllocator(m_Allocator);});

  create_swapchain();
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

  // create VMA 
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


void Engine::create_device()
{
  VkPhysicalDeviceFeatures f1{};
  VkPhysicalDeviceVulkan11Features f11{};
  f11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;

  VkPhysicalDeviceVulkan12Features f12{};
  f12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
  f12.bufferDeviceAddress = VK_TRUE;

  VkPhysicalDeviceVulkan13Features f13{};
  f13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  f13.dynamicRendering = VK_TRUE;
  f13.synchronization2 = VK_TRUE;
  
  f11.pNext = &f12;
  f12.pNext = &f13;

  VkPhysicalDeviceFeatures2 features{};
  features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  features.pNext = &f11;
  features.features = f1;
  

  vks::DeviceBuilder builder{ h_Instance };
  builder
    .set_required_extensions(m_DeviceExtensions)
    .set_required_features(features)
    .set_surface(h_Surface)
    .select_physical_device()
    .build();

  h_Device = builder.get_logical_device();
  h_PhysicalDevice = builder.get_physical_device();
  h_GraphicsQueue = builder.get_graphics_queue();
  h_PresentQueue = builder.get_present_queue();
  m_Indices = builder.get_queue_indices();
}

void Engine::create_swapchain()
{
  QueueFamilyIndices indices;
  vks::SwapchainBuilder builder{};
  builder
    .set_devices(h_PhysicalDevice, h_Device)
    .set_surface(h_Surface)
    .set_queue_indices(m_Indices)
    .build();


  h_Swapchain = builder.get_swapchain();

  //FIXME: this api is cancer again.. whole builder api is bad..
  builder.get_swapchain_images(m_SwapchainImages);
  builder.get_image_views(m_SwapchainImageViews, m_SwapchainImages);

  VkExtent2D ext = builder.get_window_extent();
  VkExtent3D drawImageExtent = {
  	ext.width,
	  ext.height,
		1
	};

	//hardcoding the draw format to 32 bit float
	m_DrawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	m_DrawImage.imageExtent = drawImageExtent;

	VkImageUsageFlags drawImageUsages{};
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	VkImageCreateInfo rimg_info = vkinit::image_create_info(m_DrawImage.imageFormat, drawImageUsages, drawImageExtent);

	//for the draw image, we want to allocate it from gpu local memory
	VmaAllocationCreateInfo rimg_allocinfo = {};
	rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	//allocate and create the image
	vmaCreateImage(m_Allocator, &rimg_info, &rimg_allocinfo, &m_DrawImage.image, &m_DrawImage.allocation, nullptr);

  VkImageViewCreateInfo rview_info = vkinit::imageview_create_info(m_DrawImage.imageFormat, m_DrawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);
  VK_CHECK_RESULT(vkCreateImageView(h_Device, &rview_info, nullptr, &m_DrawImage.imageView));

  m_DeletionQueue.push_function([=, this]() { 
    vkDestroyImageView(h_Device, m_DrawImage.imageView, nullptr);
    vmaDestroyImage(m_Allocator, m_DrawImage.image, m_DrawImage.allocation); 
  });
}

void Engine::draw_background(VkCommandBuffer cmd)
{
  /* draw with vkCmdClearColor
  VkClearColorValue clearValue;
  float flash = std::abs(std::sin(m_FrameNumber / 120.0f));
  clearValue = {{0.0f, 0.0f, flash, 1.0f}};

  VkImageSubresourceRange clearRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
  vkCmdClearColorImage(cmd, m_DrawImage.image, VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);
  */
  
  // draw with compute shader dispatch

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, gradientPipeline);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, gradientPipelineLayout, 0, 1, &drawImageDescriptors, 0, nullptr);
  vkCmdDispatch(cmd, std::ceil(m_DrawExtent.width / 16.0), std::ceil(m_DrawExtent.height / 16.0), 1);
}


void Engine::init_descriptors()
{
  std::vector<DescriptorAllocator::PoolSizeRatio> sizes = 
  {
    {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}
  };
  g_DescriptorAllocator.init_pool(h_Device, 10, sizes);

  // FIXME: DeviceBuilder and SwapchainBuilder be like this instead of how it is now..
  {
    DescriptorLayoutBuilder builder;
    builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    drawImageDescriptorLayout = builder.build(h_Device, VK_SHADER_STAGE_COMPUTE_BIT);
  }

  drawImageDescriptors = g_DescriptorAllocator.allocate(h_Device, drawImageDescriptorLayout);
  
  // links the draw image to the descriptor for the compute shader in the pipeline!
  VkDescriptorImageInfo info{};
  info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  info.imageView = m_DrawImage.imageView;

  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.pNext = nullptr;

  write.dstBinding = 0;
  write.dstSet = drawImageDescriptors;
  write.descriptorCount = 1;
  write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  write.pImageInfo = &info;

  vkUpdateDescriptorSets(h_Device, 1, &write, 0, nullptr);

  m_DeletionQueue.push_function([&]() {
    g_DescriptorAllocator.destroy_pool(h_Device);
    vkDestroyDescriptorSetLayout(h_Device, drawImageDescriptorLayout, nullptr);
  });
}

void Engine::init_pipelines()
{
  init_background_pipelines();
}

void Engine::init_background_pipelines()
{
  VkPipelineLayoutCreateInfo computeLayout{};
  computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  computeLayout.pNext = nullptr;
  computeLayout.pSetLayouts = &drawImageDescriptorLayout;
  computeLayout.setLayoutCount = 1;

  VK_CHECK_RESULT(vkCreatePipelineLayout(h_Device, &computeLayout, nullptr, &gradientPipelineLayout));

  VkShaderModule computeDrawShader;
  if (!vkutil::load_shader_module
    ("shaders/gradient.spv", h_Device, &computeDrawShader))
  {
    AR_CORE_ERROR("Error when building compute shader!");
  }

  VkPipelineShaderStageCreateInfo stageInfo{};
  stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stageInfo.pNext = nullptr;
  stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  stageInfo.module = computeDrawShader;
  stageInfo.pName = "main";

  VkComputePipelineCreateInfo computePipelineCreateInfo{};
  computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  computePipelineCreateInfo.pNext = nullptr;
  computePipelineCreateInfo.layout = gradientPipelineLayout;
  computePipelineCreateInfo.stage = stageInfo;

  VK_CHECK_RESULT(vkCreateComputePipelines(h_Device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &gradientPipeline));

  vkDestroyShaderModule(h_Device, computeDrawShader, nullptr);

  m_DeletionQueue.push_function([&]() {
    vkDestroyPipelineLayout(h_Device, gradientPipelineLayout, nullptr);
    vkDestroyPipeline(h_Device, gradientPipeline, nullptr);
  });


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
