#include "aurora_pch.h"
#include "application.h"
#include "engine.h"
#include "vk_initialisers.h"
#include "vk_startup.h"
#include "vk_images.h"
#include "vk_descriptors.h"
#include "vk_pipelines.h"

#include <GLFW/glfw3.h>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

//FIXME: temportary imgui!
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec3.hpp> // glm::vec3
#include <glm/vec4.hpp> // glm::vec4
#include <glm/mat4x4.hpp> // glm::mat4
#include <glm/ext/matrix_transform.hpp> // glm::translate, glm::rotate, glm::scale
#include <glm/ext/matrix_clip_space.hpp> // glm::perspective
#include <glm/ext/scalar_constants.hpp> // glm::pi


// NOTE: needs to create instance ... contains device ... surface swapchain logic .. frame drawing

namespace Aurora
{

Engine* self = nullptr;

Engine& Engine::get()
{
  return *self;
}

Engine::Engine()
{
  
  AR_ASSERT(self == nullptr, "Failed setting self to this");
  self = this;

  init_vulkan();
  init_swapchain();
  init_commands();
  init_sync_structures();
  init_descriptors();
  init_pipelines();
  init_imgui(); 


  // init default mesh upload

  std::array<Vertex, 4> rect_vertices;
	rect_vertices[0].position = {0.5,-0.5, 0};
	rect_vertices[1].position = {0.5,0.5, 0};
	rect_vertices[2].position = {-0.5,-0.5, 0};
	rect_vertices[3].position = {-0.5,0.5, 0};

	rect_vertices[0].color = {0,0, 0,1};
	rect_vertices[1].color = { 0.5,0.5,0.5 ,1};
	rect_vertices[2].color = { 1,0, 0,1 };
	rect_vertices[3].color = { 0,1, 0,1 };
  std::array<uint32_t, 6> rect_indices;
  rect_indices[0] = 0;
	rect_indices[1] = 1;
	rect_indices[2] = 2;

	rect_indices[3] = 2;
	rect_indices[4] = 1;
	rect_indices[5] = 3;

  rectangle = upload_mesh(rect_vertices, rect_indices);
 
  // upload other meshes...
  testMeshes = load_gltf_meshes(this, "assets/basicmesh.glb").value();


  m_DeletionQueue.push_function([&]() {
    destroy_buffer(rectangle.indexBuffer);
    destroy_buffer(rectangle.vertexBuffer);
  });
} 

Engine::~Engine()
{
  vkDeviceWaitIdle(h_Device);
  
  self = nullptr;

  vks::destroy_debug_messenger(h_Instance, h_DebugMessenger, nullptr);
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
 
  for (auto& mesh : testMeshes)
  {
    destroy_buffer(mesh->meshBuffers.indexBuffer);
    destroy_buffer(mesh->meshBuffers.vertexBuffer);
  }

  m_DeletionQueue.flush();

  vkDestroyDevice(h_Device, nullptr);
  vkDestroyInstance(h_Instance, nullptr);
}

void Engine::draw()
{
  VK_CHECK_RESULT(vkWaitForFences(h_Device, 1, &get_current_frame().renderFence, true, 1000000000));
  // new frame

  VK_CHECK_RESULT(vkResetFences(h_Device, 1, &get_current_frame().renderFence));
  
  get_current_frame().deletionQueue.flush();
  
  uint32_t swapchainImageIndex;
  
  //VK_CHECK_RESULT(vkAcquireNextImageKHR(h_Device, h_Swapchain, 1000000000, get_current_frame().swapchainSemaphore, nullptr, &swapchainImageIndex));
  VkResult e = vkAcquireNextImageKHR(h_Device, h_Swapchain, 1000000000, get_current_frame().swapchainSemaphore, nullptr, &swapchainImageIndex);
  if (e == VK_ERROR_OUT_OF_DATE_KHR)
  {
    resize_requested = true;
    return;
  }
  
  if (resize_requested)
    resize_swapchain();

  VkCommandBuffer cmd = get_current_frame().mainCommandBuffer;
  VK_CHECK_RESULT(vkResetCommandBuffer(cmd, 0));
  
  m_DrawExtent.width = m_DrawImage.imageExtent.width;
  m_DrawExtent.height = m_DrawImage.imageExtent.height;
  
  VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
  VK_CHECK_RESULT(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

  vkutil::transition_image(cmd, m_DrawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

  // draw stuff here into the draw image
  draw_background(cmd);

  vkutil::transition_image(cmd, m_DrawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  vkutil::transition_image(cmd, m_DepthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
  draw_geometry(cmd);
  
  vkutil::transition_image(cmd, m_DrawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

  vkutil::transition_image(cmd, m_DrawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  vkutil::transition_image(cmd, m_SwapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  
  // NOTE: uses blit, could use less flexible but more performant option 
  vkutil::copy_image_to_image(cmd, m_DrawImage.image, m_SwapchainImages[swapchainImageIndex], m_DrawExtent, m_DrawExtent /* FIXME: this should be swapchain extent?? */);
  
  vkutil::transition_image(cmd, m_SwapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  // imgui draws directly to swapchain image instead of to draw img
  draw_imgui(cmd, m_SwapchainImageViews[swapchainImageIndex]);
  vkutil::transition_image(cmd, m_SwapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

  VK_CHECK_RESULT(vkEndCommandBuffer(cmd));

  // submit 
  VkCommandBufferSubmitInfo cmdInfo = vkinit::command_buffer_submit_info(cmd);

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

  //VK_CHECK_RESULT(vkQueuePresentKHR(h_GraphicsQueue, &info));
  VkResult presentResult = vkQueuePresentKHR(h_GraphicsQueue, &info);
  if (presentResult == VK_ERROR_OUT_OF_DATE_KHR)
  {
    resize_requested = true;
  }
  m_FrameNumber++;
}


void Engine::init_vulkan()
{
  uint32_t requiredAPI = VK_API_VERSION_1_3;
  AR_CORE_WARN("Required Vulkan Instance Version 1.3");
  check_instance_ext_support();
  check_validation_layer_support();
  create_instance();
  
  if (m_UseValidationLayers == true)
    vks::setup_validation_layer_callback(h_Instance, h_DebugMessenger, validation_callback);

  glfwCreateWindowSurface(h_Instance, Application::get_main_window().get_handle(), nullptr, &h_Surface);
  
  create_device();

  VmaAllocatorCreateInfo allocatorInfo{};
  allocatorInfo.physicalDevice = h_PhysicalDevice;
  allocatorInfo.device = h_Device;
  allocatorInfo.instance = h_Instance;
  allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
  vmaCreateAllocator(&allocatorInfo, &m_Allocator);

  m_DeletionQueue.push_function([&]() {vmaDestroyAllocator(m_Allocator);});
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
  createInfo.enabledExtensionCount = static_cast<uint32_t>(m_InstanceExtensions.size());
  createInfo.ppEnabledExtensionNames = m_InstanceExtensions.data();

  if (m_UseValidationLayers)
  {
    createInfo.enabledLayerCount = static_cast<uint32_t>(m_ValidationLayers.size());
    createInfo.ppEnabledLayerNames = m_ValidationLayers.data();
  }
  
  VK_CHECK_RESULT(vkCreateInstance(&createInfo, nullptr, &h_Instance));
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

void Engine::create_device()
{
  Features features{};
  features.f12.bufferDeviceAddress = VK_TRUE;
  features.f13.dynamicRendering = VK_TRUE;
  features.f13.synchronization2 = VK_TRUE;

  // could have more options like set preferred gpu type, set required format, require geometry shader, etc
  vks::DeviceBuilder builder{h_Instance, h_Surface};
  builder.set_required_extensions(m_DeviceExtensions);
  builder.set_required_features(features);
  
  builder.build(
    h_PhysicalDevice, h_Device,
    m_Indices,
    h_GraphicsQueue, h_PresentQueue
  );
}

void Engine::init_swapchain()
{
  vks::SwapchainBuilder builder{h_PhysicalDevice, h_Device, h_Surface, m_Indices};
  //builder.set_preferred_present_mode();
  //builder.set_preferred_format();
  
  builder.build(
    h_Swapchain,
    m_SwapchainImages, 
    m_SwapchainFormat, 
    m_SwapchainExtent
  );

  m_SwapchainImageViews = vkutil::get_image_views(h_Device, m_SwapchainFormat.format, m_SwapchainImages);

  // create draw image
  VkExtent3D drawImageExtent = {m_SwapchainExtent.width, m_SwapchainExtent.height, 1}; 
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
  
  // NOTE: depth image creation!!!
  m_DepthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
  m_DepthImage.imageExtent = drawImageExtent;

  VkImageUsageFlags depthImageUsages{};
  depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  
  VkImageCreateInfo dimg_info = vkinit::image_create_info(m_DepthImage.imageFormat, depthImageUsages, drawImageExtent);
  vmaCreateImage(m_Allocator, &dimg_info, &rimg_allocinfo, &m_DepthImage.image, &m_DepthImage.allocation, nullptr);
  VkImageViewCreateInfo dviewinfo = vkinit::imageview_create_info(m_DepthImage.imageFormat, m_DepthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);

  VK_CHECK_RESULT(vkCreateImageView(h_Device, &dviewinfo, nullptr, &m_DepthImage.imageView));


  m_DeletionQueue.push_function([=, this]() { 
    vkDestroyImageView(h_Device, m_DrawImage.imageView, nullptr);
    vmaDestroyImage(m_Allocator, m_DrawImage.image, m_DrawImage.allocation);

    vkDestroyImageView(h_Device, m_DepthImage.imageView, nullptr);
    vmaDestroyImage(m_Allocator, m_DepthImage.image, m_DepthImage.allocation);
  });
}

void Engine::draw_background(VkCommandBuffer cmd)
{
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, backgroundEffects[currentBackgroundEffect].pipeline);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, gradientPipelineLayout, 0, 1, &drawImageDescriptors, 0, nullptr);
  
  ComputePushConstants pc;
  //pc.data1[0] = 1.0f; pc.data1[1] = 0.0f; pc.data1[2] = 0.0f; pc.data1[3] = 1.0f;
  //pc.data2[0] = 0.0f; pc.data2[1] = 0.0f; pc.data2[2] = 1.0f; pc.data2[3] = 1.0f;
  vkCmdPushConstants(cmd, gradientPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &backgroundEffects[currentBackgroundEffect].data);

  vkCmdDispatch(cmd, std::ceil(m_DrawExtent.width / 16.0), std::ceil(m_DrawExtent.height / 16.0), 1);
}


void Engine::init_descriptors()
{
  std::vector<DescriptorAllocator::PoolSizeRatio> sizes = 
  {
    {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}
  };
  g_DescriptorAllocator.init_pool(h_Device, 10, sizes);

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
  init_triangle_pipeline();
  init_mesh_pipeline();
}

void Engine::init_background_pipelines()
{
  VkPipelineLayoutCreateInfo computeLayout{};
  computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  computeLayout.pNext = nullptr;
  computeLayout.pSetLayouts = &drawImageDescriptorLayout;
  computeLayout.setLayoutCount = 1;
  
  VkPushConstantRange pcs{};
  pcs.offset = 0;
  pcs.size = sizeof(ComputePushConstants);
  pcs.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
  
  computeLayout.pPushConstantRanges = &pcs;
  computeLayout.pushConstantRangeCount = 1;

  VK_CHECK_RESULT(vkCreatePipelineLayout(h_Device, &computeLayout, nullptr, &gradientPipelineLayout));

  VkShaderModule computeDrawShader;
  if (!vkutil::load_shader_module
    ("shaders/push_gradient.spv", h_Device, &computeDrawShader))
  {
    AR_CORE_ERROR("Error when building compute shader!");
  }

  VkShaderModule skyShader;
  if (!vkutil::load_shader_module
    ("shaders/sky_shader.spv", h_Device, &skyShader))
  {
    AR_CORE_ERROR("Error when building sky shader");
  }

  VkPipelineShaderStageCreateInfo stageInfo = vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_COMPUTE_BIT, computeDrawShader);

  VkComputePipelineCreateInfo computePipelineCreateInfo{};
  computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  computePipelineCreateInfo.pNext = nullptr;
  computePipelineCreateInfo.layout = gradientPipelineLayout;
  computePipelineCreateInfo.stage = stageInfo;
    
  ComputeEffect gradient;
  gradient.layout = gradientPipelineLayout;
  gradient.name = "gradient";
  gradient.data = {};

  gradient.data.data1[0] = 1;
  gradient.data.data1[1] = 0;
  gradient.data.data1[2] = 0;
  gradient.data.data1[3] = 1;

  gradient.data.data2[0] = 0;
  gradient.data.data2[1] = 0;
  gradient.data.data2[2] = 1;
  gradient.data.data2[3] = 1;

  ComputeEffect sky;
  sky.layout = gradientPipelineLayout;
  sky.name = "sky";
  sky.data = {};

  sky.data.data1[0] = 0.1;
  sky.data.data1[1] = 0.2;
  sky.data.data1[2] = 0.4;
  sky.data.data1[3] = 0.97;

  VK_CHECK_RESULT(vkCreateComputePipelines(h_Device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &gradient.pipeline));

  computePipelineCreateInfo.stage.module = skyShader;

  VK_CHECK_RESULT(vkCreateComputePipelines(h_Device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &sky.pipeline));

  backgroundEffects.push_back(gradient);
  backgroundEffects.push_back(sky);

  vkDestroyShaderModule(h_Device, computeDrawShader, nullptr);
  vkDestroyShaderModule(h_Device, skyShader, nullptr);

  m_DeletionQueue.push_function([&]() {
    vkDestroyPipelineLayout(h_Device, gradientPipelineLayout, nullptr);
    for (const auto& bgEffect : backgroundEffects)
    {
      vkDestroyPipeline(h_Device, bgEffect.pipeline, nullptr);
    }
  });
}

void Engine::init_sync_structures()
{
  VkFenceCreateInfo fence = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
  VkSemaphoreCreateInfo semaphore = vkinit::semaphore_create_info();
  for (int i = 0; i < FRAME_OVERLAP; i++)
  {
    VK_CHECK_RESULT(vkCreateFence(h_Device, &fence, nullptr, &m_Frames[i].renderFence));
    VK_CHECK_RESULT(vkCreateSemaphore(h_Device, &semaphore, nullptr, &m_Frames[i].swapchainSemaphore));
    VK_CHECK_RESULT(vkCreateSemaphore(h_Device, &semaphore, nullptr, &m_Frames[i].renderSemaphore));
  }

  VK_CHECK_RESULT(vkCreateFence(h_Device, &fence, nullptr, &m_ImmediateFence));
  m_DeletionQueue.push_function([=, this] {
    vkDestroyFence(h_Device, m_ImmediateFence, nullptr);
  });
}

void Engine::init_commands()
{
  VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(m_Indices.graphicsFamily.value(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
  for (int i = 0; i < FRAME_OVERLAP; i++)
  {
    VK_CHECK_RESULT(vkCreateCommandPool(h_Device, &commandPoolInfo, nullptr, &m_Frames[i].commandPool));
    VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(m_Frames[i].commandPool, 1);
    VK_CHECK_RESULT(vkAllocateCommandBuffers(h_Device, &cmdAllocInfo, &m_Frames[i].mainCommandBuffer));
  }
  
  // create immediate submit command pool & buffer
  VK_CHECK_RESULT(vkCreateCommandPool(h_Device, &commandPoolInfo, nullptr, &m_ImmediateCommandPool));
  VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(m_ImmediateCommandPool, 1);
  VK_CHECK_RESULT(vkAllocateCommandBuffers(h_Device, &cmdAllocInfo, &m_ImmediateCommandBuffer));
  m_DeletionQueue.push_function([=, this]() {
    vkDestroyCommandPool(h_Device, m_ImmediateCommandPool, nullptr);
  });

}

void Engine::init_imgui()
{
  VkDescriptorPoolSize pool_sizes[] = { { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 } };

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 1000;
	pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
	pool_info.pPoolSizes = pool_sizes;

	VkDescriptorPool imguiPool;
	VK_CHECK_RESULT(vkCreateDescriptorPool(h_Device, &pool_info, nullptr, &imguiPool));

  ImGui::CreateContext();
  ImGui_ImplGlfw_InitForVulkan(Application::get_main_window().get_handle(), true);
  ImGui_ImplVulkan_InitInfo info{};
  info.Instance = h_Instance;
  info.PhysicalDevice = h_PhysicalDevice;
  info.Device = h_Device;
  info.Queue = h_GraphicsQueue;
  info.PipelineCache = VK_NULL_HANDLE;
  info.DescriptorPool = imguiPool;
  info.MinImageCount = 3;
  info.ImageCount = 3;
  info.UseDynamicRendering = true;
  info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  
  VkPipelineRenderingCreateInfo info2{};
  info2.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  info2.colorAttachmentCount = 1;
  info2.pColorAttachmentFormats = &m_SwapchainFormat.format;
  
  info.PipelineRenderingCreateInfo = info2;
  ImGui_ImplVulkan_Init(&info);
  ImGui_ImplVulkan_CreateFontsTexture();
  
  m_DeletionQueue.push_function([=, this]() {
    ImGui_ImplVulkan_Shutdown();
    vkDestroyDescriptorPool(h_Device, imguiPool, nullptr);
  });

  // FIXME: temporary imgui color space fix, PR is on the way.
  ImGuiStyle& style = ImGui::GetStyle();
  for (int i = 0; i < ImGuiCol_COUNT; i++)
  {
    ImVec4& colour = style.Colors[i];
    colour.x = powf(colour.x, 2.2f);
    colour.y = powf(colour.y, 2.2f);
    colour.z = powf(colour.z, 2.2f);
    colour.w = colour.w;
  }
}

void Engine::draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView)
{
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  
  
  if (ImGui::Begin("background"))
  {
    ComputeEffect& selected = backgroundEffects[currentBackgroundEffect];
    ImGui::Text("Selected effect: %s", selected.name);
    ImGui::SliderInt("Effect Index: ", &currentBackgroundEffect, 0, backgroundEffects.size() - 1); 
    ImGui::ColorEdit4("data1", (float*) &selected.data.data1);
    ImGui::InputFloat4("data1", (float*) &selected.data.data1);
    ImGui::InputFloat4("data2", (float*) &selected.data.data2);
    ImGui::InputFloat4("data3", (float*) &selected.data.data3);
    ImGui::InputFloat4("data4", (float*) &selected.data.data4);
  
    ImGui::Text("frametime %f ms", stats.frametime);
  }
  ImGui::End();
  ImGui::Render(); 

  VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  VkRenderingInfo renderInfo = vkinit::rendering_info(m_SwapchainExtent, &colorAttachment, nullptr);
  vkCmdBeginRendering(cmd, &renderInfo);
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
  vkCmdEndRendering(cmd);
}

void Engine::init_triangle_pipeline()
{
  VkShaderModule triangleFragShader;
  if (!vkutil::load_shader_module("shaders/triangle/triangle_frag.spv", h_Device, &triangleFragShader))
  {
    AR_CORE_ERROR("Error when building the triangle fragment shader");
  }
  AR_CORE_INFO("Triangle Fragment Shader Succesfully loaded!");

  VkShaderModule triangleVertexShader;
  if (!vkutil::load_shader_module("shaders/triangle/triangle_vert.spv", h_Device, &triangleVertexShader))
  {
    AR_CORE_ERROR("Error when building the triangle vertex shader");
  }
  AR_CORE_INFO("Triangle Vertex Shader Succesfully loaded!");
  
  VkPipelineLayoutCreateInfo info = vkinit::pipeline_layout_create_info();
  VK_CHECK_RESULT(vkCreatePipelineLayout(h_Device, &info, nullptr, &trigPipelineLayout));
  
  PipelineBuilder builder;
  builder.PipelineLayout = trigPipelineLayout;
  
  builder.set_shaders(triangleVertexShader, triangleFragShader);
  builder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
  builder.set_polygon_mode(VK_POLYGON_MODE_FILL);
  builder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
  
  builder.set_multisampling_none();
  builder.disable_blending();
  //builder.disable_depthtest();
  builder.enable_depthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL); 
  builder.set_color_attachment_format(m_DrawImage.imageFormat);
  builder.set_depth_format(m_DepthImage.imageFormat);

  trianglePipeline = builder.build_pipeline(h_Device);

  vkDestroyShaderModule(h_Device, triangleFragShader, nullptr);
  vkDestroyShaderModule(h_Device, triangleVertexShader, nullptr);

  m_DeletionQueue.push_function([&] {
    vkDestroyPipelineLayout(h_Device, trigPipelineLayout, nullptr);
    vkDestroyPipeline(h_Device, trianglePipeline, nullptr);
  });
}

void Engine::draw_geometry(VkCommandBuffer cmd)
{
  //VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(m_DrawImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(m_DrawImage.imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
  VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(m_DepthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

  VkRenderingInfo renderInfo = vkinit::rendering_info(m_DrawExtent, &colorAttachment, &depthAttachment);
  vkCmdBeginRendering(cmd, &renderInfo);
  
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, trianglePipeline);

  VkViewport viewport{};
  viewport.x = 0;
  viewport.y = 0;
  viewport.width = m_DrawExtent.width;
  viewport.height = m_DrawExtent.height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset.x = 0;
  scissor.offset.y = 0;
  scissor.extent.width = m_DrawExtent.width;
  scissor.extent.height = m_DrawExtent.height;

  vkCmdSetScissor(cmd, 0, 1, &scissor);

  vkCmdDraw(cmd, 3, 1, 0, 0);


  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, meshPipeline);
  GPUDrawPushConstants pcs{};

  pcs.worldMatrix = {1.0f};
  
  //memcpy(pcs.worldMatrix, matrix.data(), matrix.size() * sizeof(float));
  pcs.vertexBuffer = rectangle.vertexBufferAddress;
  
  vkCmdPushConstants(cmd, meshPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &pcs);
  vkCmdBindIndexBuffer(cmd, rectangle.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
  vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);
  
  glm::mat4 view = glm::mat4{1.0f};
  view = glm::translate(view, glm::vec3{0, 0, -5});
  glm::mat4 projection = glm::perspective(glm::radians(70.0f), (float) m_DrawExtent.width / (float) m_DrawExtent.height, 10000.0f, 0.1f);
  projection[1][1] *= -1;
  pcs.worldMatrix = projection * view;


  pcs.vertexBuffer = testMeshes[2]->meshBuffers.vertexBufferAddress;
  vkCmdPushConstants(cmd, meshPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &pcs);
  vkCmdBindIndexBuffer(cmd, testMeshes[2]->meshBuffers.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
  vkCmdDrawIndexed(cmd, testMeshes[2]->surfaces[0].count, 1, testMeshes[2]->surfaces[0].startIndex, 0, 0);


  vkCmdEndRendering(cmd);
  
}

AllocatedBuffer Engine::create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
{
  VkBufferCreateInfo bufferInfo{};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.pNext = nullptr;
  bufferInfo.size = allocSize;
  bufferInfo.usage = usage;

  VmaAllocationCreateInfo vmaAllocInfo{};
  vmaAllocInfo.usage = memoryUsage;
  vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
  
  AllocatedBuffer newBuffer{};
  
  VK_CHECK_RESULT(vmaCreateBuffer(m_Allocator, &bufferInfo, &vmaAllocInfo, &newBuffer.buffer, &newBuffer.allocation, &newBuffer.info));
  return newBuffer;
}

void Engine::destroy_buffer(const AllocatedBuffer& buffer)
{
  vmaDestroyBuffer(m_Allocator, buffer.buffer, buffer.allocation);
}

GPUMeshBuffers Engine::upload_mesh(std::span<Vertex> vertices, std::span<uint32_t> indices)
{
  const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
  const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

  GPUMeshBuffers newSurface;

  newSurface.vertexBuffer = create_buffer(
    vertexBufferSize,
    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    VMA_MEMORY_USAGE_GPU_ONLY);
  
  VkBufferDeviceAddressInfo deviceAddressInfo{};
  deviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
  deviceAddressInfo.buffer = newSurface.vertexBuffer.buffer;
  newSurface.vertexBufferAddress = vkGetBufferDeviceAddress(h_Device, &deviceAddressInfo);
  
  newSurface.indexBuffer = create_buffer(
    indexBufferSize,
    VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    VMA_MEMORY_USAGE_GPU_ONLY);
  

  //FIXME: compare perf cpu to gpu instead of gpu only! (small gpu/cpu shared mem)
  AllocatedBuffer staging = create_buffer(vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
  void* data = staging.allocation->GetMappedData();

  memcpy(data, vertices.data(), vertexBufferSize);
  memcpy((char*) data + vertexBufferSize, indices.data(), indexBufferSize); //FIXME: C style casting

  // FIXME: UNFINISHED!
  immediate_submit([&](VkCommandBuffer cmd){
    VkBufferCopy vertexCopy{};
    vertexCopy.dstOffset = 0;
    vertexCopy.srcOffset = 0;
    vertexCopy.size = vertexBufferSize;

    vkCmdCopyBuffer(cmd, staging.buffer, newSurface.vertexBuffer.buffer, 1, &vertexCopy);
    
    VkBufferCopy indexCopy{};
    indexCopy.dstOffset = 0;
    indexCopy.srcOffset = vertexBufferSize;
    indexCopy.size = indexBufferSize;

    vkCmdCopyBuffer(cmd, staging.buffer, newSurface.indexBuffer.buffer, 1, &indexCopy);
  });
  destroy_buffer(staging);

  return newSurface;
}

void Engine::immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function)
{
  VK_CHECK_RESULT(vkResetFences(h_Device, 1, &m_ImmediateFence));
  VK_CHECK_RESULT(vkResetCommandBuffer(m_ImmediateCommandBuffer, 0));

  VkCommandBuffer cmd = m_ImmediateCommandBuffer;
  VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
  
  VK_CHECK_RESULT(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

  function(cmd);

  VK_CHECK_RESULT(vkEndCommandBuffer(cmd));

  VkCommandBufferSubmitInfo cmdInfo = vkinit::command_buffer_submit_info(cmd);
  VkSubmitInfo2 submit = vkinit::submit_info(&cmdInfo, nullptr, nullptr);
  VK_CHECK_RESULT(vkQueueSubmit2(h_GraphicsQueue, 1, &submit, m_ImmediateFence));

  VK_CHECK_RESULT(vkWaitForFences(h_Device, 1, &m_ImmediateFence, true, 9999999999));

} 

void Engine::init_mesh_pipeline()
{
  VkShaderModule meshFragShader;
  if (!vkutil::load_shader_module("shaders/mesh/mesh.frag.spv", h_Device, &meshFragShader))
  {
    AR_CORE_ERROR("Error when building mesh fragment shader succesfuly");
  }
  AR_CORE_INFO("Built Mesh fragment shader succesfully!!");

  VkShaderModule meshVertShader;
  if (!vkutil::load_shader_module("shaders/mesh/mesh.vert.spv", h_Device, &meshVertShader))
  {
    AR_CORE_ERROR("Error when building mesh vertex shader...");
  }
  AR_CORE_INFO("Build mesh vertex shader successfulyl111!!");

  VkPushConstantRange bufferRange{};
  bufferRange.offset = 0;
  bufferRange.size = sizeof(GPUDrawPushConstants);
  bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  
  VkPipelineLayoutCreateInfo pipelineInfo = vkinit::pipeline_layout_create_info();
  pipelineInfo.pPushConstantRanges = &bufferRange;
  pipelineInfo.pushConstantRangeCount = 1;
  
  VK_CHECK_RESULT(vkCreatePipelineLayout(h_Device, &pipelineInfo, nullptr, &meshPipelineLayout));

  //FIXME: if it does something more than set a value (set multiple, init many values) it has a func
  // pipeline layout could also have a function so everything has a func
  // should i make DeviceBuilder and SwapchainBuilder like this too?
  // creation depends on vkdevice u could create the same thing with diff devices but options change the thing so should be in the build func?
  // how do i do multiple return then...

  PipelineBuilder builder;
  builder.PipelineLayout = meshPipelineLayout;
  builder.set_shaders(meshVertShader, meshFragShader);
  builder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
  builder.set_polygon_mode(VK_POLYGON_MODE_FILL);
  builder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
  builder.set_multisampling_none();
  builder.enable_blending_additive();
  builder.enable_depthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
  builder.set_color_attachment_format(m_DrawImage.imageFormat);
  builder.set_depth_format(m_DepthImage.imageFormat);
  meshPipeline = builder.build_pipeline(h_Device);

  vkDestroyShaderModule(h_Device, meshFragShader, nullptr);
  vkDestroyShaderModule(h_Device, meshVertShader, nullptr);
  
  m_DeletionQueue.push_function([&]() {
    vkDestroyPipelineLayout(h_Device, meshPipelineLayout, nullptr);
    vkDestroyPipeline(h_Device, meshPipeline, nullptr);
  });
}

void Engine::resize_swapchain()
{
  vkDeviceWaitIdle(h_Device);
  
  vkDestroySwapchainKHR(h_Device, h_Swapchain, nullptr);
  for (auto view : m_SwapchainImageViews)
  {
    vkDestroyImageView(h_Device, view, nullptr); 
  }

  int w, h;
  glfwGetWindowSize(Application::get_main_window().get_handle(), &w, &h);
  // FIXME: should be window extent??
  m_DrawExtent.width = w;
  m_DrawExtent.height = h;
  
  init_swapchain();
  resize_requested = false;

}

} // Aurora namespace
