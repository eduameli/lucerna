#include "engine.h"

#include "aurora_pch.h"
#include "window.h"
#include "vk_initialisers.h"
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

#include "ar_asserts.h"
#include "logger.h"
#include "vk_device.h"
#include "vk_swapchain.h"

// NOTE: needs to create instance ... contains device ... surface swapchain logic .. frame drawing

namespace Aurora {

static Engine* s_Instance = nullptr;

Engine& Engine::get()
{
  AR_LOG_ASSERT(s_Instance != nullptr, "Trying to get reference to Engine but s_Instance is a null pointer!");
  return *s_Instance;
}

void Engine::init()
{
  AR_LOG_ASSERT(!s_Instance, "Engine already exists!");
  s_Instance = this;
  
  init_vulkan();
  init_swapchain();
  init_commands();
  init_sync_structures();
  init_descriptors();
  init_pipelines();
  init_imgui();
  
  m_TestMeshes = load_gltf_meshes(this, "assets/basicmesh.glb").value();
} 

void Engine::shutdown()
{
  vkDeviceWaitIdle(m_Device.logical);
  s_Instance = nullptr;
  
  vkDestroySwapchainKHR(m_Device.logical, m_Swapchain.handle, nullptr);
  vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
  
  for (auto view : m_Swapchain.views)
  {
    vkDestroyImageView(m_Device.logical, view, nullptr);
  }
  
  for (int i = 0; i < FRAME_OVERLAP; i++)
  {
    vkDestroyCommandPool(m_Device.logical, m_Frames[i].commandPool, nullptr);

    vkDestroyFence(m_Device.logical, m_Frames[i].renderFence, nullptr);
    vkDestroySemaphore(m_Device.logical, m_Frames[i].renderSemaphore, nullptr);
    vkDestroySemaphore(m_Device.logical, m_Frames[i].swapchainSemaphore, nullptr);
    m_Frames[i].deletionQueue.flush();
  }
 
  for (auto& mesh : m_TestMeshes)
  {
    destroy_buffer(mesh->meshBuffers.indexBuffer);
    destroy_buffer(mesh->meshBuffers.vertexBuffer);
  }

  m_DeletionQueue.flush();
  Logger::destroy_debug_messenger(m_Instance, m_DebugMessenger, nullptr);
  vkDestroyDevice(m_Device.logical, nullptr);
  vkDestroyInstance(m_Instance, nullptr);
}

void Engine::run()
{
  while (!glfwWindowShouldClose(Window::get_handle()))
  {
    auto start = std::chrono::system_clock::now();
    glfwPollEvents();
  
    if (glfwGetKey(Window::get_handle(), GLFW_KEY_ESCAPE))
      glfwSetWindowShouldClose(Window::get_handle(), true);

    if (stopRendering)
    {
      std::cout << "Sleeping!!" << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      continue;
    }

    draw();
    
    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    AR_CORE_TRACE("frame time {}", elapsed.count() / 1000.0f);
  }
}

void Engine::draw()
{
  VK_CHECK_RESULT(vkWaitForFences(m_Device.logical, 1, &get_current_frame().renderFence, true, 1000000000));
  VK_CHECK_RESULT(vkResetFences(m_Device.logical, 1, &get_current_frame().renderFence));
  
  get_current_frame().deletionQueue.flush();
  
  uint32_t swapchainImageIndex;
  
  VK_CHECK_RESULT(vkAcquireNextImageKHR(m_Device.logical, m_Swapchain.handle, 1000000000, get_current_frame().swapchainSemaphore, nullptr, &swapchainImageIndex));



  VkCommandBuffer cmd = get_current_frame().mainCommandBuffer;
  VK_CHECK_RESULT(vkResetCommandBuffer(cmd, 0));
  
  m_DrawExtent.width = m_DrawImage.imageExtent.width;
  m_DrawExtent.height = m_DrawImage.imageExtent.height;
  
  VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
  VK_CHECK_RESULT(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

  vkutil::transition_image(cmd, m_DrawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

  // draw compute
  draw_background(cmd);

  vkutil::transition_image(cmd, m_DrawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  vkutil::transition_image(cmd, m_DepthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
  
  draw_geometry(cmd);
  
  vkutil::transition_image(cmd, m_DrawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  vkutil::transition_image(cmd, m_Swapchain.images[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  
  vkutil::copy_image_to_image(cmd, m_DrawImage.image, m_Swapchain.images[swapchainImageIndex], m_DrawExtent, m_DrawExtent /* FIXME: this should be swapchain extent?? */);
  
  vkutil::transition_image(cmd, m_Swapchain.images[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  draw_imgui(cmd, m_Swapchain.views[swapchainImageIndex]);
  vkutil::transition_image(cmd, m_Swapchain.images[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

  VK_CHECK_RESULT(vkEndCommandBuffer(cmd));

  // submit 
  VkCommandBufferSubmitInfo cmdInfo = vkinit::command_buffer_submit_info(cmd);

  VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, get_current_frame().swapchainSemaphore);
  VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, get_current_frame().renderSemaphore);

  VkSubmitInfo2 submit = vkinit::submit_info(&cmdInfo, &signalInfo, &waitInfo);
  VK_CHECK_RESULT(vkQueueSubmit2(m_Device.graphics, 1, &submit, get_current_frame().renderFence));
  
  VkPresentInfoKHR info{};
  info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  info.pNext = nullptr;
  info.pSwapchains = &m_Swapchain.handle;
  info.swapchainCount = 1;
  info.pWaitSemaphores = &get_current_frame().renderSemaphore;
  info.waitSemaphoreCount = 1;

  info.pImageIndices = &swapchainImageIndex;

  VK_CHECK_RESULT(vkQueuePresentKHR(m_Device.present, &info));
  frameNumber++;
}

void Engine::validate_instance_supported()
{
  // validate instance version supported
  uint32_t version = 0;
  vkEnumerateInstanceVersion(&version);
  AR_LOG_ASSERT(version > VK_API_VERSION_1_3, "This Application requires Vulkan 1.3, which has not been found!");
  AR_CORE_INFO("Using Vulkan Instance [version {}.{}.{}]", VK_API_VERSION_MAJOR(version), VK_API_VERSION_MINOR(version), VK_API_VERSION_PATCH(version));
  
  // validate validation layer support
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
    bool found = false;
    for (const auto& layerProperties : supportedLayers)
    {
      if (strcmp(layerName, layerProperties.layerName) == 0)
      {
        found = true;
        break;
      }
    }
    
    AR_LOG_ASSERT(found, "Required {} layer is not supported!", layerName);
  }

  // validate instance extension support
  uint32_t requiredCount = 0;
  const char** requiredExtensions = glfwGetRequiredInstanceExtensions(&requiredCount);
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
  AR_CORE_WARN("Required Instance Extensions:");

  for (auto extensionName : m_InstanceExtensions)
  {
    AR_CORE_WARN("\t{}", extensionName);
    bool found = false;
    for (auto extension : supportedExtensions)
    {
      if (strcmp(extensionName , extension.extensionName) == 0)
      {
        found = true;
        break;
      }
    }
    AR_LOG_ASSERT(found, "Required {} Extension not supported!", extensionName);
  }
}

void Engine::init_vulkan()
{
  volkInitialize();

  validate_instance_supported();
  create_instance();
  
  if (m_UseValidationLayers == true)
    Logger::setup_validation_layer_callback(m_Instance, m_DebugMessenger, Logger::validation_callback);

  glfwCreateWindowSurface(m_Instance, Window::get_handle(), nullptr, &m_Surface);
  
  create_device();

  VmaAllocatorCreateInfo allocatorInfo{};
  allocatorInfo.physicalDevice = m_Device.physical;
  allocatorInfo.device = m_Device.logical;
  allocatorInfo.instance = m_Instance;
  allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

  VmaVulkanFunctions functions{};
  functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
  functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
  allocatorInfo.pVulkanFunctions = &functions;
  vmaCreateAllocator(&allocatorInfo, &m_Allocator);

  m_DeletionQueue.push_function([&]() {
    vmaDestroyAllocator(m_Allocator);
  });
}

void Engine::create_instance()
{
  VkApplicationInfo app;
  app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app.pEngineName = "Aurora";
  app.engineVersion = VK_MAKE_VERSION(0, 0, 1);
  app.apiVersion = VK_API_VERSION_1_3;

  VkInstanceCreateInfo instance{};
  instance.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instance.pApplicationInfo = &app;
  instance.enabledExtensionCount = static_cast<uint32_t>(m_InstanceExtensions.size());
  instance.ppEnabledExtensionNames = m_InstanceExtensions.data();

  if (m_UseValidationLayers)
  {
    instance.enabledLayerCount = static_cast<uint32_t>(m_ValidationLayers.size());
    instance.ppEnabledLayerNames = m_ValidationLayers.data();
  }
  
  VK_CHECK_RESULT(vkCreateInstance(&instance, nullptr, &m_Instance));
  volkLoadInstance(m_Instance);
}

void Engine::create_device()
{
  DeviceContextBuilder builder {m_Instance, m_Surface};
  m_Device = builder
    .set_minimum_version(1, 3)
    .set_required_extensions(m_DeviceExtensions)
    .set_preferred_gpu_type(VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
    .build();
}

void Engine::init_swapchain()
{
  SwapchainContextBuilder builder{m_Device, m_Surface};
  m_Swapchain = builder
    .set_preferred_format(VkFormat::VK_FORMAT_B8G8R8A8_SRGB)
    .set_preferred_colorspace(VkColorSpaceKHR::VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
    .set_preferred_present(VkPresentModeKHR::VK_PRESENT_MODE_MAILBOX_KHR)
    .build();
  
  // create draw image
  VkExtent3D drawImageExtent = {m_Swapchain.extent2d.width, m_Swapchain.extent2d.height, 1}; 
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
  VK_CHECK_RESULT(vkCreateImageView(m_Device.logical, &rview_info, nullptr, &m_DrawImage.imageView));
  
  // NOTE: depth image creation!!!
  m_DepthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
  m_DepthImage.imageExtent = drawImageExtent;

  VkImageUsageFlags depthImageUsages{};
  depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  
  VkImageCreateInfo dimg_info = vkinit::image_create_info(m_DepthImage.imageFormat, depthImageUsages, drawImageExtent);
  vmaCreateImage(m_Allocator, &dimg_info, &rimg_allocinfo, &m_DepthImage.image, &m_DepthImage.allocation, nullptr);
  VkImageViewCreateInfo dviewinfo = vkinit::imageview_create_info(m_DepthImage.imageFormat, m_DepthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);

  VK_CHECK_RESULT(vkCreateImageView(m_Device.logical, &dviewinfo, nullptr, &m_DepthImage.imageView));


  m_DeletionQueue.push_function([=, this]() { 
    vkDestroyImageView(m_Device.logical, m_DrawImage.imageView, nullptr);
    vmaDestroyImage(m_Allocator, m_DrawImage.image, m_DrawImage.allocation);

    vkDestroyImageView(m_Device.logical, m_DepthImage.imageView, nullptr);
    vmaDestroyImage(m_Allocator, m_DepthImage.image, m_DepthImage.allocation);
  });
}

void Engine::draw_background(VkCommandBuffer cmd)
{
  ComputeEffect effect = m_BackgroundEffects[m_BackgroundEffectIndex]; 

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.layout, 0, 1, &m_DrawDescriptors, 0, nullptr);
  
  ComputePushConstants pc;
  //pc.data1[0] = 1.0f; pc.data1[1] = 0.0f; pc.data1[2] = 0.0f; pc.data1[3] = 1.0f;
  //pc.data2[0] = 0.0f; pc.data2[1] = 0.0f; pc.data2[2] = 1.0f; pc.data2[3] = 1.0f;
  vkCmdPushConstants(cmd, effect.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &effect.data);

  vkCmdDispatch(cmd, std::ceil(m_DrawExtent.width / 16.0), std::ceil(m_DrawExtent.height / 16.0), 1);
}


void Engine::init_descriptors()
{
  std::vector<DescriptorAllocator::PoolSizeRatio> sizes = 
  {
    {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}
  };
  m_DescriptorAllocator.init_pool(m_Device.logical, 10, sizes);

  {
    DescriptorLayoutBuilder builder;
    builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    m_DrawDescriptorLayout = builder.build(m_Device.logical, VK_SHADER_STAGE_COMPUTE_BIT);
  }

  m_DrawDescriptors = m_DescriptorAllocator.allocate(m_Device.logical, m_DrawDescriptorLayout);
  
  // links the draw image to the descriptor for the compute shader in the pipeline!
  VkDescriptorImageInfo info{};
  info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  info.imageView = m_DrawImage.imageView;

  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.pNext = nullptr;

  write.dstBinding = 0;
  write.dstSet = m_DrawDescriptors;
  write.descriptorCount = 1;
  write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  write.pImageInfo = &info;

  vkUpdateDescriptorSets(m_Device.logical, 1, &write, 0, nullptr);

  m_DeletionQueue.push_function([&]() {
    m_DescriptorAllocator.destroy_pool(m_Device.logical);
    vkDestroyDescriptorSetLayout(m_Device.logical, m_DrawDescriptorLayout, nullptr);
  });
}

void Engine::init_pipelines()
{
  init_background_pipelines();
  //init_triangle_pipeline();
  init_mesh_pipeline();
}

void Engine::init_background_pipelines()
{
  VkPipelineLayoutCreateInfo computeLayout{};
  computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  computeLayout.pNext = nullptr;
  computeLayout.pSetLayouts = &m_DrawDescriptorLayout;
  computeLayout.setLayoutCount = 1;
  
  VkPushConstantRange pcs{};
  pcs.offset = 0;
  pcs.size = sizeof(ComputePushConstants);
  pcs.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
  
  computeLayout.pPushConstantRanges = &pcs;
  computeLayout.pushConstantRangeCount = 1;
  
  VkPipelineLayout layout{};

  VK_CHECK_RESULT(vkCreatePipelineLayout(m_Device.logical, &computeLayout, nullptr, &layout));

  VkShaderModule computeDrawShader;
  if (!vkutil::load_shader_module
    ("shaders/push_gradient.spv", m_Device.logical, &computeDrawShader))
  {
    AR_CORE_ERROR("Error when building compute shader!");
  }

  VkShaderModule skyShader;
  if (!vkutil::load_shader_module
    ("shaders/sky_shader.spv", m_Device.logical, &skyShader))
  {
    AR_CORE_ERROR("Error when building sky shader");
  }

  VkPipelineShaderStageCreateInfo stageInfo = vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_COMPUTE_BIT, computeDrawShader);

  VkComputePipelineCreateInfo computePipelineCreateInfo{};
  computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  computePipelineCreateInfo.pNext = nullptr;
  computePipelineCreateInfo.layout = layout;
  computePipelineCreateInfo.stage = stageInfo;
    
  ComputeEffect gradient;
  gradient.layout = layout;
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
  sky.layout = layout;
  sky.name = "sky";
  sky.data = {};

  sky.data.data1[0] = 0.1;
  sky.data.data1[1] = 0.2;
  sky.data.data1[2] = 0.4;
  sky.data.data1[3] = 0.97;

  VK_CHECK_RESULT(vkCreateComputePipelines(m_Device.logical, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &gradient.pipeline));

  computePipelineCreateInfo.stage.module = skyShader;

  VK_CHECK_RESULT(vkCreateComputePipelines(m_Device.logical, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &sky.pipeline));

  m_BackgroundEffects.push_back(gradient);
  m_BackgroundEffects.push_back(sky);

  vkDestroyShaderModule(m_Device.logical, computeDrawShader, nullptr);
  vkDestroyShaderModule(m_Device.logical, skyShader, nullptr);

  m_DeletionQueue.push_function([&, layout]() {
    vkDestroyPipelineLayout(m_Device.logical, layout, nullptr);
    for (const auto& bgEffect : m_BackgroundEffects)
    {
      vkDestroyPipeline(m_Device.logical, bgEffect.pipeline, nullptr);
    }
  });
}

void Engine::init_sync_structures()
{
  VkFenceCreateInfo fence = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
  VkSemaphoreCreateInfo semaphore = vkinit::semaphore_create_info();
  for (int i = 0; i < FRAME_OVERLAP; i++)
  {
    VK_CHECK_RESULT(vkCreateFence(m_Device.logical, &fence, nullptr, &m_Frames[i].renderFence));
    VK_CHECK_RESULT(vkCreateSemaphore(m_Device.logical, &semaphore, nullptr, &m_Frames[i].swapchainSemaphore));
    VK_CHECK_RESULT(vkCreateSemaphore(m_Device.logical, &semaphore, nullptr, &m_Frames[i].renderSemaphore));
  }

  VK_CHECK_RESULT(vkCreateFence(m_Device.logical, &fence, nullptr, &m_ImmFence));
  m_DeletionQueue.push_function([=, this] {
    vkDestroyFence(m_Device.logical, m_ImmFence, nullptr);
  });
}

void Engine::init_commands()
{
  VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(m_Device.graphicsIndex, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
  for (int i = 0; i < FRAME_OVERLAP; i++)
  {
    VK_CHECK_RESULT(vkCreateCommandPool(m_Device.logical, &commandPoolInfo, nullptr, &m_Frames[i].commandPool));
    VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(m_Frames[i].commandPool, 1);
    VK_CHECK_RESULT(vkAllocateCommandBuffers(m_Device.logical, &cmdAllocInfo, &m_Frames[i].mainCommandBuffer));
  }
  
  // create immediate submit command pool & buffer
  VK_CHECK_RESULT(vkCreateCommandPool(m_Device.logical, &commandPoolInfo, nullptr, &m_ImmCommandPool));
  VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(m_ImmCommandPool, 1);
  VK_CHECK_RESULT(vkAllocateCommandBuffers(m_Device.logical, &cmdAllocInfo, &m_ImmCommandBuffer));
  m_DeletionQueue.push_function([=, this]() {
    vkDestroyCommandPool(m_Device.logical, m_ImmCommandPool, nullptr);
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
	VK_CHECK_RESULT(vkCreateDescriptorPool(m_Device.logical, &pool_info, nullptr, &imguiPool));

  ImGui::CreateContext();
  ImGui_ImplGlfw_InitForVulkan(Window::get_handle(), true);
  ImGui_ImplVulkan_InitInfo info{};
  info.Instance = m_Instance;
  info.PhysicalDevice = m_Device.physical;
  info.Device = m_Device.logical;
  info.Queue = m_Device.graphics;
  info.PipelineCache = VK_NULL_HANDLE;
  info.DescriptorPool = imguiPool;
  info.MinImageCount = 3;
  info.ImageCount = 3;
  info.UseDynamicRendering = true;
  info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  
  VkPipelineRenderingCreateInfo info2{};
  info2.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  info2.colorAttachmentCount = 1;
  info2.pColorAttachmentFormats = &m_Swapchain.format;
  
  info.PipelineRenderingCreateInfo = info2;
  ImGui_ImplVulkan_Init(&info);
  ImGui_ImplVulkan_CreateFontsTexture();
  
  m_DeletionQueue.push_function([=, this]() {
    ImGui_ImplVulkan_Shutdown();
    vkDestroyDescriptorPool(m_Device.logical, imguiPool, nullptr);
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

void Engine::draw_imgui(VkCommandBuffer cmd, VkImageView target)
{
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  
  
  if (ImGui::Begin("settings"))
  {
    ComputeEffect& selected = m_BackgroundEffects[m_BackgroundEffectIndex];
    ImGui::Text("Selected effect: %s", selected.name);
    ImGui::SliderInt("Effect Index: ", &m_BackgroundEffectIndex, 0, m_BackgroundEffects.size() - 1); 
    ImGui::ColorEdit4("data1", (float*) &selected.data.data1);
    ImGui::InputFloat4("data1", (float*) &selected.data.data1);
    ImGui::InputFloat4("data2", (float*) &selected.data.data2);
    ImGui::InputFloat4("data3", (float*) &selected.data.data3);
    ImGui::InputFloat4("data4", (float*) &selected.data.data4);
  }
  ImGui::End();
  ImGui::Render(); 

  VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(target, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  VkRenderingInfo renderInfo = vkinit::rendering_info(m_Swapchain.extent2d, &colorAttachment, nullptr);
  vkCmdBeginRendering(cmd, &renderInfo);
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
  vkCmdEndRendering(cmd);
}

void Engine::draw_geometry(VkCommandBuffer cmd)
{
  //VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(m_DrawImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(m_DrawImage.imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
  VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(m_DepthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

  VkRenderingInfo renderInfo = vkinit::rendering_info(m_DrawExtent, &colorAttachment, &depthAttachment);
  vkCmdBeginRendering(cmd, &renderInfo);
  
  //vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, trianglePipeline);
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_MeshPipeline);
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

  GPUDrawPushConstants pcs{};

  pcs.worldMatrix = {1.0f};
  
  glm::mat4 view = glm::mat4{1.0f};
  view = glm::translate(view, glm::vec3{0, 0, -5});
  glm::mat4 projection = glm::perspective(glm::radians(70.0f), (float) m_DrawExtent.width / (float) m_DrawExtent.height, 10000.0f, 0.1f);
  projection[1][1] *= -1;
  pcs.worldMatrix = projection * view;


  pcs.vertexBuffer = m_TestMeshes[2]->meshBuffers.vertexBufferAddress;
  vkCmdPushConstants(cmd, m_MeshPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &pcs);
  vkCmdBindIndexBuffer(cmd, m_TestMeshes[2]->meshBuffers.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
  vkCmdDrawIndexed(cmd, m_TestMeshes[2]->surfaces[0].count, 1, m_TestMeshes[2]->surfaces[0].startIndex, 0, 0);


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
  newSurface.vertexBufferAddress = vkGetBufferDeviceAddress(m_Device.logical, &deviceAddressInfo);
  
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
  VK_CHECK_RESULT(vkResetFences(m_Device.logical, 1, &m_ImmFence));
  VK_CHECK_RESULT(vkResetCommandBuffer(m_ImmCommandBuffer, 0));

  VkCommandBuffer cmd = m_ImmCommandBuffer;
  VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
  
  VK_CHECK_RESULT(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

  function(cmd);

  VK_CHECK_RESULT(vkEndCommandBuffer(cmd));

  VkCommandBufferSubmitInfo cmdInfo = vkinit::command_buffer_submit_info(cmd);
  VkSubmitInfo2 submit = vkinit::submit_info(&cmdInfo, nullptr, nullptr);
  VK_CHECK_RESULT(vkQueueSubmit2(m_Device.graphics, 1, &submit, m_ImmFence));

  VK_CHECK_RESULT(vkWaitForFences(m_Device.logical, 1, &m_ImmFence, true, 9999999999));

} 

void Engine::init_mesh_pipeline()
{
  VkShaderModule meshFragShader;
  if (!vkutil::load_shader_module("shaders/mesh/mesh.frag.spv", m_Device.logical, &meshFragShader))
  {
    AR_CORE_ERROR("Error when building mesh fragment shader!");
  }

  VkShaderModule meshVertShader;
  if (!vkutil::load_shader_module("shaders/mesh/mesh.vert.spv", m_Device.logical, &meshVertShader))
  {
    AR_CORE_ERROR("Error when building mesh vertex shader...");
  }

  VkPushConstantRange bufferRange{};
  bufferRange.offset = 0;
  bufferRange.size = sizeof(GPUDrawPushConstants);
  bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  
  VkPipelineLayoutCreateInfo pipelineInfo = vkinit::pipeline_layout_create_info();
  pipelineInfo.pPushConstantRanges = &bufferRange;
  pipelineInfo.pushConstantRangeCount = 1;
  
  VK_CHECK_RESULT(vkCreatePipelineLayout(m_Device.logical, &pipelineInfo, nullptr, &m_MeshPipelineLayout));

  //FIXME: if it does something more than set a value (set multiple, init many values) it has a func
  // pipeline layout could also have a function so everything has a func
  // should i make DeviceBuilder and SwapchainBuilder like this too?
  // creation depends on vkdevice u could create the same thing with diff devices but options change the thing so should be in the build func?
  // how do i do multiple return then...

  PipelineBuilder builder;
  builder.PipelineLayout = m_MeshPipelineLayout;
  builder.set_shaders(meshVertShader, meshFragShader);
  builder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
  builder.set_polygon_mode(VK_POLYGON_MODE_FILL);
  builder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
  builder.set_multisampling_none();
  builder.enable_blending_additive();
  builder.enable_depthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
  builder.set_color_attachment_format(m_DrawImage.imageFormat);
  builder.set_depth_format(m_DepthImage.imageFormat);
  m_MeshPipeline = builder.build_pipeline(m_Device.logical);

  vkDestroyShaderModule(m_Device.logical, meshFragShader, nullptr);
  vkDestroyShaderModule(m_Device.logical, meshVertShader, nullptr);
  
  m_DeletionQueue.push_function([&]() {
    vkDestroyPipelineLayout(m_Device.logical, m_MeshPipelineLayout, nullptr);
    vkDestroyPipeline(m_Device.logical, m_MeshPipeline, nullptr);
  });
}

void Engine::resize_swapchain()
{
  vkDeviceWaitIdle(m_Device.logical);
  
  vkDestroySwapchainKHR(m_Device.logical, m_Swapchain.handle, nullptr);
  for (auto view : m_Swapchain.views)
  {
    vkDestroyImageView(m_Device.logical, view, nullptr); 
  }

  int w, h;
  glfwGetWindowSize(Window::get_handle(), &w, &h);
  // FIXME: should be window extent??
  m_DrawExtent.width = w;
  m_DrawExtent.height = h;
  
  init_swapchain();
  resizeRequested = false;

}

} // namespace aurora
