#include "engine.h"

#include "aurora_pch.h"
#include "ar_asserts.h"
#include "logger.h"
#include "window.h"
#include "vk_initialisers.h"
#include "vk_images.h"
#include "vk_descriptors.h"
#include "vk_pipelines.h"
#include "vk_device.h"
#include "vk_swapchain.h"
#include "gfx_effects.h"
#include <GLFW/glfw3.h>
#include "vk_types.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"

#include <volk.h>

#include <glm/packing.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp>

namespace Aurora {

AutoCVar_Int shadowEnabled("shadow_mapping.enabled", "is shadow mapping enabled", 1, CVarFlags::EditCheckbox);
AutoCVar_Int shadowViewFromLight("shadow_mapping.view_from_light", "view scene from view of directional light shadow caster", 0, CVarFlags::EditCheckbox);
AutoCVar_Int shadowRotateLight("shadow_mapping.rotate_light", "rotate light around origin showcasing real time shadows", 0, CVarFlags::EditCheckbox);
AutoCVar_Float shadowSoftness("shadow_mapping.softness", "radius of pcf sampling", 0.0025, CVarFlags::None);

AutoCVar_Int debugLinesEnabled("debug.show_lines", "", 1, CVarFlags::EditCheckbox);

AutoCVar_Float cameraFOV("camera.fov", "camera fov in degrees", 70.0f, CVarFlags::Advanced);
AutoCVar_Float cameraFar("camera.far", "", 10000.0, CVarFlags::Advanced);
AutoCVar_Float cameraNear("camera.near", "", 0.1, CVarFlags::Advanced);

AutoCVar_Int ssaoDisplayTexture("ssao.display_texture", "", 0, CVarFlags::EditCheckbox);

static Engine* s_Instance = nullptr;
Engine* Engine::get()
{
  AR_LOG_ASSERT(s_Instance != nullptr, "Trying to get reference to Engine but s_Instance is a null pointer!");
  return s_Instance;
}

void Engine::init()
{
  AR_LOG_ASSERT(!s_Instance, "Engine already exists!");
  s_Instance = this;
  
  internalExtent = {1280, 800, 1};

  init_vulkan();
  init_swapchain();
  init_commands();
  init_sync_structures();
  init_descriptors();
  init_pipelines();
  init_imgui(); 
  init_default_data();
  
  mainCamera.init();
   
  std::string structurePath = "assets/simple_shadow.glb";
  auto structureFile = load_gltf(this, structurePath);
  AR_LOG_ASSERT(structureFile.has_value(), "gltf loaded correctly!");

  loadedScenes["structure"] = *structureFile;
  
  // prepare shadow uniform
  shadowPass.buffer = create_buffer(sizeof(u_ShadowPass), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
  m_DeletionQueue.push_function([=, this] {
    destroy_buffer(shadowPass.buffer);
  });

  // prepare gfx effects
  bloom::prepare();
  ssao::prepare();
  
} 


void Engine::shutdown()
{
  vkDeviceWaitIdle(device);
  s_Instance = nullptr;

  loadedScenes.clear();
  
  vkDestroySwapchainKHR(device, m_Swapchain.handle, nullptr);
  vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
  
  for (auto view : m_Swapchain.views)
  {
    vkDestroyImageView(device, view, nullptr);
  }
  
  for (int i = 0; i < FRAME_OVERLAP; i++)
  {
    vkDestroyCommandPool(device, m_Frames[i].commandPool, nullptr);

    vkDestroyFence(device, m_Frames[i].renderFence, nullptr);
    vkDestroySemaphore(device, m_Frames[i].renderSemaphore, nullptr);
    vkDestroySemaphore(device, m_Frames[i].swapchainSemaphore, nullptr);
    
    m_Frames[i].deletionQueue.flush();
  }

  m_DeletionQueue.flush();
  
  vmaDestroyAllocator(m_Allocator);

  vkDestroyDevice(device, nullptr);
  vkDestroyInstance(m_Instance, nullptr);
}

bool Engine::should_quit()
{
  return !glfwWindowShouldClose(Window::get());
}

void Engine::run()
{
  while (should_quit())
  {
    auto start = std::chrono::system_clock::now();
    
    if (valid_swapchain)
    {
      glfwPollEvents();
    }
    else
    {
      glfwWaitEvents();
      continue;
    }

    if (stopRendering)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
      continue;
    }
    
    update_scene();
    draw();
    
    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    stats.frametime = elapsed.count() / 1000.0f;
  }
}

void Engine::resize_swapchain(int width, int height)
{
  AR_LOG_ASSERT(width > 0 && height > 0, "Attempted to resize swapchain to 0x0");

  vkDeviceWaitIdle(device);

  VkSwapchainKHR oldSwapchain = m_Swapchain.handle;
  for (int i = 0; i < m_Swapchain.views.size(); i++)
  {
    vkDestroyImageView(device, m_Swapchain.views[i], nullptr);
  }
  
  vkDestroySwapchainKHR(device, oldSwapchain, nullptr);

  SwapchainContextBuilder builder{m_Device, m_Surface};
  m_Swapchain = builder
    .set_preferred_format(VkFormat::VK_FORMAT_B8G8R8A8_SRGB)
    .set_preferred_colorspace(VkColorSpaceKHR::VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
    .set_preferred_present(VkPresentModeKHR::VK_PRESENT_MODE_IMMEDIATE_KHR)
    .build();
  
  valid_swapchain = true;
}

void Engine::update_scene()
{
  auto start = std::chrono::system_clock::now();

  mainCamera.update();

  glm::mat4 view = mainCamera.get_view_matrix();
  glm::mat4 projection = glm::perspective(glm::radians(cameraFOV.get()), (float) m_DrawExtent.width / (float) m_DrawExtent.height, cameraFar.get(), cameraNear.get());
  projection[1][1] *= -1;

  sceneData.view = view;
  sceneData.proj = projection;
  sceneData.viewproj = projection * view;

  if (shadowViewFromLight.get() == true)
  {
    sceneData.viewproj = lightProj * lView;
    sceneData.view = lView;
    sceneData.proj = lightProj;
  }
  
	sceneData.ambientColor = glm::vec4(0.1f); // would it be like a skybox
  sceneData.sunlightColor = glm::vec4(1.0f);
  

  loadedScenes["structure"]->queue_draw(glm::mat4{1.0f}, mainDrawContext);
  
  mainDrawContext.opaque_draws.clear(); 
  mainDrawContext.opaque_draws.reserve(mainDrawContext.OpaqueSurfaces.size());
  
  // frustum culling for main camera
  for (uint32_t i = 0; i < mainDrawContext.OpaqueSurfaces.size(); i++)
  {
    if (is_visible(mainDrawContext.OpaqueSurfaces[i], sceneData.viewproj))
    {
      mainDrawContext.opaque_draws.push_back(i);
    }

    // draw aabb
    if (debugLinesEnabled.get() == true)
      queue_debug_obb(mainDrawContext.OpaqueSurfaces[i].bounds.origin, mainDrawContext.OpaqueSurfaces[i].bounds.extents);

  }
   
  // FIXME: Another way of doing this is that we would calculate a sort key , and then our opaque_draws would be something like 20 bits draw index,
  // and 44 bits for sort key/hash. That way would be faster than this as it can be sorted through faster methods.
  std::sort(mainDrawContext.opaque_draws.begin(), mainDrawContext.opaque_draws.end(), [&](const auto& iA, const auto& iB) {
    const RenderObject& A = mainDrawContext.OpaqueSurfaces[iA];
    const RenderObject& B = mainDrawContext.OpaqueSurfaces[iB];
    if(A.material == B.material)
    {
      return A.indexBuffer < B.indexBuffer;
    }
    else
    {
      return A.material < B.material;
    }
  });


  auto end = std::chrono::system_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  stats.scene_update_time = elapsed.count() / 1000.0f;
}

void Engine::draw()
{
  VkResult r;
  VK_CHECK_RESULT(vkWaitForFences(device, 1, &get_current_frame().renderFence, true, 1000000000));
  
  get_current_frame().deletionQueue.flush();
  get_current_frame().frameDescriptors.clear_pools(device);

  uint32_t swapchainImageIndex;
  if (valid_swapchain)
  {
    r = vkAcquireNextImageKHR(device, m_Swapchain.handle, 1000000000, get_current_frame().swapchainSemaphore, nullptr, &swapchainImageIndex);
    if (r == VK_ERROR_OUT_OF_DATE_KHR)
    {
      valid_swapchain = false;
    }
    else
    {
      AR_LOG_ASSERT(r == VK_SUCCESS || r == VK_SUBOPTIMAL_KHR, "Error getting next swapchain image!");
    }
  }
  if (!valid_swapchain)
  {
    return;
  }

  m_DrawExtent.height = glm::min(m_Swapchain.extent2d.height, m_DrawImage.imageExtent.height) * m_RenderScale;
  m_DrawExtent.width = glm::min(m_Swapchain.extent2d.width, m_DrawImage.imageExtent.width) * m_RenderScale;

  VK_CHECK_RESULT(vkResetFences(device, 1, &get_current_frame().renderFence));

  VkCommandBuffer cmd = get_current_frame().mainCommandBuffer;
  VK_CHECK_RESULT(vkResetCommandBuffer(cmd, 0));
  
  VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
  VK_CHECK_RESULT(vkBeginCommandBuffer(cmd, &cmdBeginInfo))
  

  // draw compute bg
  vkutil::transition_image(cmd, m_DrawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
  draw_background(cmd);

  // draw depth prepass first to be able to overlap shadow mapping & screen space (depth based) compute effects
  vkutil::transition_image(cmd, m_DepthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
  draw_depth_prepass(cmd);

  vkutil::transition_image(cmd, m_ShadowDepthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
  draw_shadow_pass(cmd);
  vkutil::transition_image(cmd, m_ShadowDepthImage.image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL);
  
  vkutil::transition_image(cmd, m_DepthImage.image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL);
  // depth based compute ss effects
  ssao::run(cmd, m_DepthImage.imageView);
  vkutil::transition_image(cmd, m_DepthImage.image, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

  vkutil::transition_image(cmd, m_DrawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  draw_geometry(cmd);
  draw_debug_lines(cmd);

  // NOTE: Post Effects
  vkutil::transition_image(cmd, m_DrawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
  bloom::run(cmd, m_DrawImage.imageView);
  vkutil::transition_image(cmd, m_DrawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  
  vkutil::transition_image(cmd, m_Swapchain.images[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  
  if (ssaoDisplayTexture.get() == true)
  {
    vkutil::transition_image(cmd, ssao::outputAmbient.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkutil::copy_image_to_image(cmd, ssao::outputAmbient.image, m_Swapchain.images[swapchainImageIndex], m_DrawExtent, m_Swapchain.extent2d);
    vkutil::transition_image(cmd, ssao::outputAmbient.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

  }
  else
  {
    vkutil::copy_image_to_image(cmd, m_DrawImage.image, m_Swapchain.images[swapchainImageIndex], m_DrawExtent, m_Swapchain.extent2d);
  }
  
  vkutil::transition_image(cmd, m_Swapchain.images[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  draw_imgui(cmd, m_Swapchain.views[swapchainImageIndex]);
  vkutil::transition_image(cmd, m_Swapchain.images[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

  VK_CHECK_RESULT(vkEndCommandBuffer(cmd));

  // submit 
  VkCommandBufferSubmitInfo cmdInfo = vkinit::command_buffer_submit_info(cmd);

  VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, get_current_frame().swapchainSemaphore);
  VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, get_current_frame().renderSemaphore);

  VkSubmitInfo2 submit = vkinit::submit_info(&cmdInfo, &signalInfo, &waitInfo);
  VK_CHECK_RESULT(vkQueueSubmit2(graphicsQueue, 1, &submit, get_current_frame().renderFence));
 
  
  VkPresentInfoKHR presentInfo = {
    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .pNext = nullptr,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &get_current_frame().renderSemaphore,
    .swapchainCount = 1,
    .pSwapchains = &m_Swapchain.handle,
    .pImageIndices = &swapchainImageIndex
  };

  r = vkQueuePresentKHR(presentQueue, &presentInfo);
  if (r == VK_ERROR_OUT_OF_DATE_KHR)
  {
    valid_swapchain = false;
  }
  else
  {
    AR_LOG_ASSERT(r == VK_SUCCESS || r == VK_SUBOPTIMAL_KHR, "Error presenting swapchain image!");
  }

  frameNumber++;
}

void Engine::draw_background(VkCommandBuffer cmd)
{
  ComputeEffect effect = m_BackgroundEffects[m_BackgroundEffectIndex]; 

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.layout, 0, 1, &m_DrawDescriptors, 0, nullptr);
  
  ComputePushConstants pc;
  vkCmdPushConstants(cmd, effect.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &effect.data);

  vkCmdDispatch(cmd, std::ceil(internalExtent.width / 16.0), std::ceil(internalExtent.height / 16.0), 1);
}

void Engine::draw_shadow_pass(VkCommandBuffer cmd)
{
  if (shadowEnabled.get() == false) return;

  
  VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(m_ShadowDepthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
  VkRenderingInfo renderInfo = vkinit::rendering_info({m_ShadowExtent.width, m_ShadowExtent.height}, nullptr, &depthAttachment);
  vkCmdBeginRendering(cmd, &renderInfo);

 lightProj = glm::ortho(
    -pcss_settings.ortho_size,
    pcss_settings.ortho_size,
    -pcss_settings.ortho_size,
    pcss_settings.ortho_size,
    pcss_settings.far, pcss_settings.near); //FIXME: arbritary znear zfar planes

  lightProj[1][1] *= -1;

  float x_value = sin(pcss_settings.shadowNumber/200.0) * pcss_settings.distance;
  float z_value = cos(pcss_settings.shadowNumber/200.0) * pcss_settings.distance;
  
  if (shadowRotateLight.get() == true)
  {
    pcss_settings.shadowNumber++;
  }

  lView = glm::lookAt(
    glm::vec3{x_value, pcss_settings.distance, z_value},
    glm::vec3{0.0f, 0.0f, 0.0f},
    glm::vec3{0.0f, 1.0f, 0.0f}
  );
  
  sceneData.sunlightDirection = glm::normalize(glm::vec4{x_value, pcss_settings.distance, z_value, 1.0f}); // .w for sun power
  shadowPass.lightView = lightProj * lView; 
  pcss_settings.lightViewProj = lightProj * lView;

  std::vector<uint32_t> shadow_draws;
  shadow_draws.reserve(mainDrawContext.OpaqueSurfaces.size());
  
  for (uint32_t i = 0; i < mainDrawContext.OpaqueSurfaces.size(); i++)
  {
    // FIXME: does nothing rn but need to do culling from camera view!
    if (is_visible(mainDrawContext.OpaqueSurfaces[i], lightProj*lView))
    {
      shadow_draws.push_back(i);
    }
  }

  // NOTE: CMS Settings?? different mat proj or a scale to basic 1 or smth
  
  u_ShadowPass data;
  data.lightViewProj = lightProj * lView;

  u_ShadowPass* shadowPassUniform = (u_ShadowPass*) shadowPass.buffer.allocation->GetMappedData();
  *shadowPassUniform = data;

  VkDescriptorSet shadowDescriptor = get_current_frame().frameDescriptors.allocate(device, m_ShadowSetLayout);
  DescriptorWriter writer;
  writer.write_buffer(0, shadowPass.buffer.buffer, sizeof(u_ShadowPass), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER); // FIXME: .buffer .buffer :sob:
  writer.update_set(device, shadowDescriptor);
  
  VkViewport viewport = vkinit::dynamic_viewport(m_ShadowExtent);
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor = vkinit::dynamic_scissor(m_ShadowExtent);
  vkCmdSetScissor(cmd, 0, 1, &scissor);
  
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ShadowPipeline);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ShadowPipelineLayout, 0, 1, &shadowDescriptor, 0, nullptr);
  
  VkBuffer lastIndexBuffer = VK_NULL_HANDLE;
  auto draw = [&](const RenderObject& draw) {
    if (draw.indexBuffer != lastIndexBuffer)
    {
      lastIndexBuffer = draw.indexBuffer;
      vkCmdBindIndexBuffer(cmd, draw.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    }

    shadow_map_pcs pcs{};
    pcs.modelMatrix = draw.transform; // worldMatrix == modelMatrix
    pcs.positions = draw.positionBufferAddress; 
   
    // scuffed way to not render ground plane to shadow map
    vkCmdPushConstants(cmd, draw.material->pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(shadow_map_pcs), &pcs);
    vkCmdDrawIndexed(cmd, draw.indexCount, 1, draw.firstIndex, 0, 0);
  };
  

  for (auto& r : shadow_draws)
  {
    draw(mainDrawContext.OpaqueSurfaces[r]);
  }

  vkCmdEndRendering(cmd);
}

void Engine::draw_depth_prepass(VkCommandBuffer cmd)
{
  VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(m_DepthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
  VkRenderingInfo depthPrepassInfo = vkinit::rendering_info(m_DrawExtent, nullptr, &depthAttachment);
  vkCmdBeginRendering(cmd, &depthPrepassInfo);

  VkViewport viewport = vkinit::dynamic_viewport(m_DrawExtent);
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor = vkinit::dynamic_scissor(m_DrawExtent);
  vkCmdSetScissor(cmd , 0, 1, &scissor);
  
  AllocatedBuffer gpuSceneDataBuffer = create_buffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
  get_current_frame().deletionQueue.push_function([=, this] {
    destroy_buffer(gpuSceneDataBuffer);
  });

  GPUSceneData* sceneUniformData = (GPUSceneData*) gpuSceneDataBuffer.allocation->GetMappedData();
  *sceneUniformData = sceneData;
  
  // FIXME: dont create a new uniform buffer every time :sob:

  VkDescriptorSet depth = get_current_frame().frameDescriptors.allocate(device, m_ShadowSetLayout);
  DescriptorWriter writer;
  writer.write_buffer(0, gpuSceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER); // FIXME: .buffer .buffer :sob:
  writer.update_set(device, depth);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_DepthPrepassPipeline);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ShadowPipelineLayout, 0, 1, &depth, 0, nullptr);

  VkBuffer lastIndexBuffer = VK_NULL_HANDLE;
  auto draw = [&](const RenderObject& draw) {
    if (draw.indexBuffer != lastIndexBuffer)
    {
      lastIndexBuffer = draw.indexBuffer;
      vkCmdBindIndexBuffer(cmd, draw.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    }

    depth_only_pcs pcs{};
    pcs.modelMatrix = draw.transform; // worldMatrix == modelMatrix
    pcs.positions = draw.positionBufferAddress; 


    vkCmdPushConstants(cmd, zpassLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(depth_only_pcs), &pcs);
    vkCmdDrawIndexed(cmd, draw.indexCount, 1, draw.firstIndex, 0, 0);
  };
  

  for (auto& r : mainDrawContext.opaque_draws)
  {
    draw(mainDrawContext.OpaqueSurfaces[r]);
  }

  vkCmdEndRendering(cmd);
}

void Engine::draw_geometry(VkCommandBuffer cmd)
{
  auto start = std::chrono::system_clock::now();
  stats.drawcall_count = 0;
  stats.triangle_count = 0;


  
  VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(m_DrawImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(m_DepthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL);
  depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

  VkRenderingInfo renderInfo = vkinit::rendering_info(m_DrawExtent, &colorAttachment, &depthAttachment);
  vkCmdBeginRendering(cmd, &renderInfo);
  
  // descriptor set for scene data created every frame! temporal per frame data - skinned mesh? (UBO)
  
  AllocatedBuffer gpuSceneDataBuffer = create_buffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
  get_current_frame().deletionQueue.push_function([=, this] {
    destroy_buffer(gpuSceneDataBuffer);
  });

  AllocatedBuffer shadowSettings = create_buffer(sizeof(ShadowFragmentSettings), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
  get_current_frame().deletionQueue.push_function([=, this] {
    destroy_buffer(shadowSettings);
  });

  GPUSceneData* sceneUniformData = (GPUSceneData*) gpuSceneDataBuffer.allocation->GetMappedData();
  *sceneUniformData = sceneData;

  ShadowFragmentSettings* settings = (ShadowFragmentSettings*) shadowSettings.allocation->GetMappedData();
  settings->lightViewProj = pcss_settings.lightViewProj;
  settings->near = 0.1;
  settings->far = 20.0;
  settings->light_size = 0.1;
  settings->enabled = shadowEnabled.get();
  settings->softness = shadowSoftness.get();

  VkDescriptorSet globalDescriptor = get_current_frame().frameDescriptors.allocate(device, m_SceneDescriptorLayout);
  DescriptorWriter writer;
  writer.write_buffer(0, gpuSceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  writer.write_image(1, m_ShadowDepthImage.imageView, m_ShadowSampler, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  writer.write_buffer(2, shadowSettings.buffer, sizeof(ShadowFragmentSettings), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  writer.update_set(device, globalDescriptor);
  
  VkViewport viewport = vkinit::dynamic_viewport(m_DrawExtent);
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor = vkinit::dynamic_scissor(m_DrawExtent);
  vkCmdSetScissor(cmd , 0, 1, &scissor);

  MaterialPipeline* lastPipeline = nullptr;
  MaterialInstance* lastMaterial = nullptr;
  VkBuffer lastIndexBuffer = VK_NULL_HANDLE;

  auto draw = [&](const RenderObject& draw) {
    if (draw.material != lastMaterial)
    {
      lastMaterial = draw.material;

      if (draw.material->pipeline != lastPipeline)
      {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.material->pipeline->pipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.material->pipeline->layout, 0, 1, &globalDescriptor, 0, nullptr); // NOTE: why inside the loop & not at the start
      }
      
      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.material->pipeline->layout, 1, 1, &draw.material->materialSet, 0, nullptr);

    }

    if (draw.indexBuffer != lastIndexBuffer)
    {
      lastIndexBuffer = draw.indexBuffer;
      vkCmdBindIndexBuffer(cmd, draw.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    }

    std_material_pcs pcs{};
    pcs.modelMatrix = draw.transform;
    pcs.vertexBuffer = draw.vertexBufferAddress;
    pcs.positionBuffer = draw.positionBufferAddress;
    // world matrix is the model matrix??

    vkCmdPushConstants(cmd, draw.material->pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(std_material_pcs), &pcs);
    vkCmdDrawIndexed(cmd, draw.indexCount, 1, draw.firstIndex, 0, 0);

    stats.drawcall_count++;
    stats.triangle_count += draw.indexCount / 3;
  };

  for (auto& r : mainDrawContext.opaque_draws)
  {
    draw(mainDrawContext.OpaqueSurfaces[r]);
  }

  for (auto& r : mainDrawContext.TransparentSurfaces)
  {
    draw(r);
  }

  vkCmdEndRendering(cmd);  
  mainDrawContext.OpaqueSurfaces.clear();
  mainDrawContext.TransparentSurfaces.clear();

  auto end = std::chrono::system_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  stats.mesh_draw_time = elapsed.count() / 1000.0f;
}

void Engine::queue_debug_line(glm::vec3 p1, glm::vec3 p2)
{
  debugLines.push_back(p1);
  debugLines.push_back(p2);
}

void Engine::queue_debug_frustum(glm::mat4 proj)
{

}

void Engine::queue_debug_obb(glm::vec3 origin, glm::vec3 extents)
{
  glm::vec3 v[8] = 
  {
    origin + glm::vec3{extents.x, extents.y, extents.z},
    origin + glm::vec3{-extents.x, extents.y, extents.z},
    origin + glm::vec3{-extents.x, -extents.y, extents.z},
    origin + glm::vec3{extents.x, -extents.y, extents.z},

    origin + glm::vec3{extents.x, extents.y, -extents.z},
    origin + glm::vec3{-extents.x, extents.y, -extents.z},
    origin + glm::vec3{-extents.x, -extents.y, -extents.z},
    origin + glm::vec3{extents.x, -extents.y, -extents.z},
  };

  queue_debug_line(v[0], v[1]);
  queue_debug_line(v[1], v[2]);
  queue_debug_line(v[2], v[3]);
  queue_debug_line(v[3], v[0]);

  queue_debug_line(v[4], v[5]);
  queue_debug_line(v[5], v[6]);
  queue_debug_line(v[6], v[7]);
  queue_debug_line(v[7], v[4]);
  
  queue_debug_line(v[0], v[4]);
  queue_debug_line(v[1], v[5]);
  queue_debug_line(v[2], v[6]);
  queue_debug_line(v[3], v[7]);
}

void Engine::draw_debug_lines(VkCommandBuffer cmd)
{

  //queue_debug_obb(glm::vec3{0.0f}, glm::vec3{2.0f});

  if (debugLinesEnabled.get() == false) return;
  if (debugLines.size() == 0) return;

  AR_LOG_ASSERT(debugLines.size() % 2 == 0, "Debug Lines buffer must be a multiple of two");
 
  VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(m_DrawImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  VkRenderingInfo renderInfo = vkinit::rendering_info(m_DrawExtent, &colorAttachment,  nullptr);
  vkCmdBeginRendering(cmd, &renderInfo);


  debugLinesBuffer = create_buffer(debugLines.size() * sizeof(glm::vec3), VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

  glm::vec3* data = (glm::vec3*) debugLinesBuffer.allocation->GetMappedData();
  memcpy(data, debugLines.data(), debugLines.size() * sizeof(glm::vec3)); 
  
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, debugLinePipeline);
  
  VkViewport viewport = vkinit::dynamic_viewport(m_DrawExtent);
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor = vkinit::dynamic_scissor(m_DrawExtent);
  vkCmdSetScissor(cmd , 0, 1, &scissor);
  
  VkBufferDeviceAddressInfo positionBDA {
    .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
    .buffer = debugLinesBuffer.buffer
  };
  
  debug_line_pcs pcs{};
  pcs.viewproj = sceneData.viewproj;
  pcs.positions = vkGetBufferDeviceAddress(device, &positionBDA);

  vkCmdPushConstants(cmd, debugLinePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(debug_line_pcs), &pcs);

  vkCmdDraw(cmd, debugLines.size(), 1, 0, 0);

  // draw line list at DrawImage
  // make buffer for queued lines
  // upload cpu to gpu
  // bind - set pcs 
  // draw - non indexed geometry - same pipeline
  destroy_buffer(debugLinesBuffer); 
  debugLines.clear();

  vkCmdEndRendering(cmd);

}

void Engine::draw_imgui(VkCommandBuffer cmd, VkImageView target)
{
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  
  ImGui::Begin("Stats");
    ImGui::Text("frametime %f ms", stats.frametime);
    ImGui::Text("draw time %f ms", stats.mesh_draw_time);
    ImGui::Text("update time %f ms", stats.scene_update_time);
    ImGui::Text("triangles %i", stats.triangle_count);
    ImGui::Text("draws %i", stats.drawcall_count);
  ImGui::End();
  
  ImGui::Begin("Renderer Settings");
    if (ImGui::CollapsingHeader("Bloom"))
    {
      ImGui::Text("boop");
      ImGui::Checkbox("Enabled", (bool*) CVarSystem::get()->get_int_cvar("bloom.enabled"));
    }
    if (ImGui::CollapsingHeader("Shadows"))
    {
      ImGui::Checkbox("Enabled", (bool*) shadowEnabled.get_ptr());
      ImGui::Checkbox("Rotate Light", (bool*) shadowRotateLight.get_ptr());
      ImGui::Checkbox("View from Light", (bool*) shadowViewFromLight.get_ptr());
      ImGui::SliderFloat("Shadow Softness", shadowSoftness.get_ptr(), 0.0, 0.1, "%.4f");
     
      ImGui::InputFloat("Frustum Size: ", &pcss_settings.ortho_size);
      ImGui::InputFloat("Far Plane: ", &pcss_settings.far);
      ImGui::InputFloat("Near Plane: ", &pcss_settings.near);
      ImGui::InputFloat("Light Distance: ", &pcss_settings.distance);
    }
    
    if (ImGui::CollapsingHeader("Background Effects"))
    {
      ImGui::Text("Selected effect: %s", m_BackgroundEffects[m_BackgroundEffectIndex].name);
      ImGui::SliderInt("Effect Index: ", &m_BackgroundEffectIndex, 0, m_BackgroundEffects.size() - 1);
    }
    ImGui::SliderFloat("Render Scale", &m_RenderScale, 0.3f, 1.0f);

  CVarSystem::get()->draw_editor();

  ImGui::End();

  ImGui::Render(); 
  

  VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(target, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  VkRenderingInfo renderInfo = vkinit::rendering_info(m_Swapchain.extent2d, &colorAttachment, nullptr);
  vkCmdBeginRendering(cmd, &renderInfo);
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
  vkCmdEndRendering(cmd);
}

//FIXME: a simple flat plane 50x50 breaks it :sob: maybe it needs volume to work or smth??
bool Engine::is_visible(const RenderObject& obj, const glm::mat4& viewproj) {
  return true; // FIXME: not working

  std::array<glm::vec3, 8> corners {
      glm::vec3 { 1, 1, 1 },
      glm::vec3 { 1, 1, -1 },
      glm::vec3 { 1, -1, 1 },
      glm::vec3 { 1, -1, -1 },
      glm::vec3 { -1, 1, 1 },
      glm::vec3 { -1, 1, -1 },
      glm::vec3 { -1, -1, 1 },
      glm::vec3 { -1, -1, -1 },
  };

  glm::mat4 matrix = viewproj * obj.transform;

  glm::vec3 min = { 1.5, 1.5, 1.5 };
  glm::vec3 max = { -1.5, -1.5, -1.5 };

  for (int c = 0; c < 8; c++) {
      // project each corner into clip space
      glm::vec4 v = matrix * glm::vec4(obj.bounds.origin + (corners[c] * obj.bounds.extents), 1.f);

      // perspective correction
      v.x = v.x / v.w;
      v.y = v.y / v.w;
      v.z = v.z / v.w;

      min = glm::min(glm::vec3 { v.x, v.y, v.z }, min);
      max = glm::max(glm::vec3 { v.x, v.y, v.z }, max);
  }

  // check the clip space box is within the view
  if (min.z > 1.f || max.z < 0.f || min.x > 1.f || max.x < -1.f || min.y > 1.f || max.y < -1.f) {
      return false;
  } else {
      return true;
  }
}

void Engine::immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function)
{
  VK_CHECK_RESULT(vkResetFences(device, 1, &m_ImmFence));
  VK_CHECK_RESULT(vkResetCommandBuffer(m_ImmCommandBuffer, 0));

  VkCommandBuffer cmd = m_ImmCommandBuffer;
  VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
  
  VK_CHECK_RESULT(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

  function(cmd);

  VK_CHECK_RESULT(vkEndCommandBuffer(cmd));

  VkCommandBufferSubmitInfo cmdInfo = vkinit::command_buffer_submit_info(cmd);
  VkSubmitInfo2 submit = vkinit::submit_info(&cmdInfo, nullptr, nullptr);
  VK_CHECK_RESULT(vkQueueSubmit2(graphicsQueue, 1, &submit, m_ImmFence));

  VK_CHECK_RESULT(vkWaitForFences(device, 1, &m_ImmFence, true, 9999999999));

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

GPUMeshBuffers Engine::upload_mesh(std::span<glm::vec3> positions, std::span<Vertex> vertices, std::span<uint32_t> indices)
{
  // vertices interleaved(normal uv colour) | positions (only pos)
  const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
  const size_t positionBufferSize = positions.size() * sizeof(glm::vec3);
  const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

  GPUMeshBuffers newSurface;

  newSurface.vertexBuffer = create_buffer(
    vertexBufferSize,
    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    VMA_MEMORY_USAGE_GPU_ONLY);
 
  VkBufferDeviceAddressInfo attributesBDA {
    .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
    .buffer = newSurface.vertexBuffer.buffer
  };
  newSurface.vertexBufferAddress = vkGetBufferDeviceAddress(device, &attributesBDA);
  
  newSurface.positionBuffer = create_buffer(
    positionBufferSize,
    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    VMA_MEMORY_USAGE_GPU_ONLY);

  VkBufferDeviceAddressInfo positionBDA {
    .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
    .buffer = newSurface.positionBuffer.buffer
  };
  newSurface.positionBufferAddress = vkGetBufferDeviceAddress(device, &positionBDA);


  newSurface.indexBuffer = create_buffer(
    indexBufferSize,
    VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    VMA_MEMORY_USAGE_GPU_ONLY);
  

  //FIXME: compare perf cpu to gpu instead of gpu only! (small gpu/cpu shared mem)
  AllocatedBuffer staging = create_buffer(vertexBufferSize + positionBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
  void* data = staging.allocation->GetMappedData();

  memcpy(data, positions.data(), positionBufferSize);
  memcpy((char*) data + positionBufferSize, vertices.data(), vertexBufferSize);
  memcpy((char*) data + positionBufferSize + vertexBufferSize, indices.data(), indexBufferSize); //FIXME: C style casting

  immediate_submit([&](VkCommandBuffer cmd){
    VkBufferCopy positionCopy{};
    positionCopy.dstOffset = 0;
    positionCopy.srcOffset = 0;
    positionCopy.size = positionBufferSize;
    
    vkCmdCopyBuffer(cmd, staging.buffer, newSurface.positionBuffer.buffer, 1, &positionCopy);

    VkBufferCopy vertexCopy{};
    vertexCopy.dstOffset = 0;
    vertexCopy.srcOffset = positionBufferSize;
    vertexCopy.size = vertexBufferSize;

    vkCmdCopyBuffer(cmd, staging.buffer, newSurface.vertexBuffer.buffer, 1, &vertexCopy);
    
    VkBufferCopy indexCopy{};
    indexCopy.dstOffset = 0;
    indexCopy.srcOffset = positionBufferSize + vertexBufferSize;
    indexCopy.size = indexBufferSize;

    vkCmdCopyBuffer(cmd, staging.buffer, newSurface.indexBuffer.buffer, 1, &indexCopy);
  });
  destroy_buffer(staging);


  vklog::label_buffer(device, newSurface.vertexBuffer.buffer, "VERTEX ATTRIBUTES");
  vklog::label_buffer(device, newSurface.positionBuffer.buffer, "POSITION BUFFER");

  return newSurface;
}

AllocatedImage Engine::create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
  AllocatedImage newImage;
  newImage.imageFormat = format;
  newImage.imageExtent = size;

  VkImageCreateInfo imgInfo = vkinit::image_create_info(format, usage, size);
  if (mipmapped)
  {
    imgInfo.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
  }

  VmaAllocationCreateInfo allocInfo{};
  allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  allocInfo.requiredFlags = VkMemoryPropertyFlags{VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT};

  VK_CHECK_RESULT(vmaCreateImage(m_Allocator, &imgInfo, &allocInfo, &newImage.image, &newImage.allocation, nullptr));
  
  VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
  if (format == VK_FORMAT_D32_SFLOAT)
  {
    aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
  }

  VkImageViewCreateInfo viewInfo = vkinit::imageview_create_info(format, newImage.image, aspectFlags);
  viewInfo.subresourceRange.levelCount = imgInfo.mipLevels;

  VK_CHECK_RESULT(vkCreateImageView(device, &viewInfo, nullptr, &newImage.imageView));

  return newImage;
}

AllocatedImage Engine::create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
  // 4 bytes per pixel RGBA
  size_t dataSize = size.depth * size.width * size.height * 4;
  AllocatedBuffer uploadBuffer = create_buffer(dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
  
  memcpy(uploadBuffer.info.pMappedData, data, dataSize);
  
  AllocatedImage newImage = create_image(size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipmapped);

  immediate_submit([&](VkCommandBuffer cmd) {
    vkutil::transition_image(cmd, newImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkBufferImageCopy copyRegion{};
    copyRegion.bufferOffset = 0;
    copyRegion.bufferRowLength = 0;
    copyRegion.bufferImageHeight = 0;
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.mipLevel = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageExtent = size;

    vkCmdCopyBufferToImage(cmd, uploadBuffer.buffer, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
    
    if (mipmapped)
    {
      vkutil::generate_mipmaps(cmd, newImage.image, VkExtent2D{newImage.imageExtent.width, newImage.imageExtent.height});
    }
    else
    {
      vkutil::transition_image(cmd, newImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

  });
  
  destroy_buffer(uploadBuffer);
  return newImage;
}

void Engine::destroy_image(const AllocatedImage& img) const
{
  vkDestroyImageView(device, img.imageView, nullptr);
  vmaDestroyImage(m_Allocator, img.image, img.allocation);
}

void Engine::init_default_data()
{
  uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
  m_WhiteImage = create_image((void*)&white, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
  
  uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 1));
  m_GreyImage = create_image((void*)&grey, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

  uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 1));
  m_BlackImage = create_image((void*)&black, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

  uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
  std::array<uint32_t, 16*16> pixels;
  for (int x = 0; x < 16; x++)
  {
    for (int y = 0; y < 16; y++)
    {
      pixels[y * 16 + x] = ((x%2) ^(y%2)) ? magenta : black;
    }
  }
  
  m_ErrorCheckerboardImage = create_image(pixels.data(), VkExtent3D{16, 16, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

  VkSamplerCreateInfo sampl{.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr};
  sampl.magFilter = VK_FILTER_NEAREST;
  sampl.minFilter = VK_FILTER_NEAREST;
  vkCreateSampler(device, &sampl, nullptr, &m_DefaultSamplerNearest);

  sampl.magFilter = VK_FILTER_LINEAR;
  sampl.minFilter = VK_FILTER_LINEAR;
  vkCreateSampler(device, &sampl, nullptr, &m_DefaultSamplerLinear);
  
  sampl.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  sampl.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  sampl.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;

  // NOTE: linear for now, it should be shadow billinear?? hardware!
  sampl.magFilter = VK_FILTER_NEAREST;
  sampl.minFilter = VK_FILTER_NEAREST;
  vkCreateSampler(device, &sampl, nullptr, &m_ShadowSampler);


  GLTFMetallic_Roughness::MaterialResources materialResources;
  materialResources.colorImage = m_WhiteImage;
  materialResources.colorSampler = m_DefaultSamplerLinear;
  materialResources.metalRoughImage = m_WhiteImage;
  materialResources.metalRoughSampler = m_DefaultSamplerLinear;

  AllocatedBuffer materialConstants = create_buffer(sizeof(GLTFMetallic_Roughness::MaterialConstants), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
  GLTFMetallic_Roughness::MaterialConstants* sceneUniformData = (GLTFMetallic_Roughness::MaterialConstants*) materialConstants.allocation->GetMappedData();
  sceneUniformData->colorFactors = glm::vec4{1, 1, 1, 1};
  sceneUniformData->metal_rough_factors = glm::vec4{1, 0.5, 0, 1};
  
  m_DeletionQueue.push_function([=, this]{
    destroy_buffer(materialConstants);
  });
  
  materialResources.dataBuffer = materialConstants.buffer;
  materialResources.dataBufferOffset = 0;

  defaultData = metalRoughMaterial.write_material(device, MaterialPass::MainColour, materialResources, globalDescriptorAllocator);




  m_DeletionQueue.push_function([&]{
    vkDestroySampler(device, m_DefaultSamplerLinear, nullptr);
    vkDestroySampler(device, m_DefaultSamplerNearest, nullptr);
    vkDestroySampler(device, m_ShadowSampler, nullptr); 
    // FIXME: metalRoughnessMaterial.clear_resources() instead!
    
    metalRoughMaterial.clear_resources(device);

    destroy_image(m_WhiteImage);
    destroy_image(m_GreyImage);
    destroy_image(m_BlackImage);
    destroy_image(m_ErrorCheckerboardImage);
  });

}



void Engine::validate_instance_supported()
{
  // validate instance version supported
  uint32_t version = 0;
  vkEnumerateInstanceVersion(&version);
  AR_LOG_ASSERT(version > VK_API_VERSION_1_3, "This Application requires Vulkan 1.3, which has not been found!");
  AR_CORE_INFO("Using Vulkan Instance [version {}.{}.{}]", VK_API_VERSION_MAJOR(version), VK_API_VERSION_MINOR(version), VK_API_VERSION_PATCH(version));
  
  // validate validation layer support
  if (m_UseValidationLayers)
  {
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
  
  if (m_UseValidationLayers)
  {
    Logger::setup_validation_layer_callback(m_Instance, m_DebugMessenger, Logger::validation_callback);
    m_DeletionQueue.push_function([&]() {
      vkDestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);
    });
  }

  glfwCreateWindowSurface(m_Instance, Window::get(), nullptr, &m_Surface);
  
  create_device();

  VmaAllocatorCreateInfo allocatorInfo{};
  allocatorInfo.physicalDevice = physicalDevice;
  allocatorInfo.device = device;
  allocatorInfo.instance = m_Instance;
  allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

  VmaVulkanFunctions functions{};
  functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
  functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
  allocatorInfo.pVulkanFunctions = &functions;
  vmaCreateAllocator(&allocatorInfo, &m_Allocator);
}

void Engine::create_instance()
{
  VkApplicationInfo app{.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .pNext = nullptr};
  app.pEngineName = "aurora";
  app.engineVersion = VK_MAKE_VERSION(0, 0, 1);
  app.apiVersion = VK_API_VERSION_1_3;

  VkInstanceCreateInfo instance{.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pNext = nullptr};
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
    .set_preferred_gpu_type(VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
    .build();

  device = m_Device.logical;
  physicalDevice = m_Device.physical;
  
  graphicsQueue = m_Device.graphics;
  graphicsIndex = m_Device.graphicsIndex;

  presentQueue = m_Device.present;
  presentIndex = m_Device.presentIndex;
}

void Engine::init_swapchain()
{
  SwapchainContextBuilder builder{m_Device, m_Surface};
  m_Swapchain = builder
    .set_preferred_format(VkFormat::VK_FORMAT_B8G8R8A8_SRGB)
    .set_preferred_colorspace(VkColorSpaceKHR::VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
    .set_preferred_present(VkPresentModeKHR::VK_PRESENT_MODE_IMMEDIATE_KHR)
    .build();
    
  AR_CORE_INFO("Using {}", vkutil::stringify_present_mode(m_Swapchain.presentMode));
  
  m_DrawExtent = {internalExtent.width, internalExtent.height};

	VkImageUsageFlags drawImageUsages{};
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  drawImageUsages |= VK_IMAGE_USAGE_SAMPLED_BIT;

  VkImageUsageFlags depthImageUsages{};
  depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  depthImageUsages |= VK_IMAGE_USAGE_SAMPLED_BIT;
  
  m_DrawImage = create_image(internalExtent, VK_FORMAT_R16G16B16A16_SFLOAT, drawImageUsages, false);
  m_DepthImage = create_image(internalExtent, VK_FORMAT_D32_SFLOAT, depthImageUsages, false);
  m_ShadowDepthImage = create_image(m_ShadowExtent, VK_FORMAT_D32_SFLOAT, depthImageUsages, false);
  
  vklog::label_image(device, m_DrawImage.image, "Draw Image");
  vklog::label_image(device, m_DepthImage.image, "Depth Image");
  vklog::label_image(device, m_ShadowDepthImage.image, "Shadow Mapping Image");

  m_DeletionQueue.push_function([=, this]() {
    destroy_image(m_DrawImage);
    destroy_image(m_DepthImage);
    destroy_image(m_ShadowDepthImage);
  });
}

void Engine::init_descriptors()
{
  std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = 
  {
    {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}
  };
  globalDescriptorAllocator.init(device, 10, sizes);

  // compute background descriptor layout? 
  {
    DescriptorLayoutBuilder builder;
    builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    m_DrawDescriptorLayout = builder.build(device, VK_SHADER_STAGE_COMPUTE_BIT);
  }
  m_DrawDescriptors = globalDescriptorAllocator.allocate(device, m_DrawDescriptorLayout);
  // links the draw image to the descriptor for the compute shader in the pipeline!
  DescriptorWriter writer;
  writer.write_image(0, m_DrawImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
  writer.update_set(device, m_DrawDescriptors);

  // scene data descriptor layout
  {
    DescriptorLayoutBuilder builder;
    builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    builder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    builder.add_binding(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    m_SceneDescriptorLayout = builder.build(device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT); 
  }
  
  {
    DescriptorLayoutBuilder builder;
    builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    m_SingleImageDescriptorLayout = builder.build(device, VK_SHADER_STAGE_FRAGMENT_BIT);
  }

  
  // creates global frame descriptor set
  for (int i = 0; i < FRAME_OVERLAP; i++)
  {
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frameSizes = {
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4},
    };
    m_Frames[i].frameDescriptors = DescriptorAllocatorGrowable{};
    m_Frames[i].frameDescriptors.init(device, 1000, frameSizes);
    
    m_DeletionQueue.push_function([&, i]() {
      m_Frames[i].frameDescriptors.destroy_pools(device);
    });

  }

  m_DeletionQueue.push_function([&]() {
    globalDescriptorAllocator.destroy_pools(device);
    vkDestroyDescriptorSetLayout(device, m_DrawDescriptorLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, m_SingleImageDescriptorLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, m_SceneDescriptorLayout, nullptr);
  });

}
//FIXME: this whole function is wack i should divide it up .. or move to init_descriptors or smth
void Engine::init_pipelines()
{
  init_background_pipelines(); // compute backgrounds
  metalRoughMaterial.build_pipelines(this);
  
  init_depth_prepass_pipeline();
  init_shadow_map_pipeline();

  // init debug line pipeline
  
  VkShaderModule debugFrag;
  AR_LOG_ASSERT(
    vkutil::load_shader_module("shaders/debug_line/debug_line.frag.spv", device, &debugFrag),
    "Error when building the debug line fragment shader module"
  );
  
  VkShaderModule debugVert;
  AR_LOG_ASSERT(
    vkutil::load_shader_module("shaders/debug_line/debug_line.vert.spv", device, &debugVert),
    "Error when building the debug line vertex shader module"
  );

  VkPushConstantRange range{};
  range.offset = 0;
  range.size = sizeof(debug_line_pcs);
  range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  VkPipelineLayoutCreateInfo layout = vkinit::pipeline_layout_create_info();
  layout.pPushConstantRanges = &range;
  layout.pushConstantRangeCount = 1;
  VK_CHECK_RESULT(vkCreatePipelineLayout(device, &layout, nullptr, &debugLinePipelineLayout));

  PipelineBuilder builder;
  builder.set_shaders(debugVert, debugFrag);
  builder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_LINE_LIST);
  builder.set_polygon_mode(VK_POLYGON_MODE_LINE);
  builder.set_multisampling_none();
  builder.disable_blending();
  builder.disable_depthtest();
  builder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
  builder.PipelineLayout = debugLinePipelineLayout;
  debugLinePipeline = builder.build_pipeline(device);
  
  vkDestroyShaderModule(device, debugVert, nullptr);
  vkDestroyShaderModule(device, debugFrag, nullptr);

  m_DeletionQueue.push_function([&](){
    vkDestroyPipelineLayout(device, debugLinePipelineLayout, nullptr);
    vkDestroyPipeline(device, debugLinePipeline, nullptr);
  });

}

void Engine::init_background_pipelines()
{
  VkPipelineLayoutCreateInfo computeLayout = vkinit::pipeline_layout_create_info();
  computeLayout.pSetLayouts = &m_DrawDescriptorLayout;
  computeLayout.setLayoutCount = 1;
  
  VkPushConstantRange pcs{};
  pcs.offset = 0;
  pcs.size = sizeof(ComputePushConstants);
  pcs.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
  
  computeLayout.pPushConstantRanges = &pcs;
  computeLayout.pushConstantRangeCount = 1;
  
  VkPipelineLayout layout{};

  VK_CHECK_RESULT(vkCreatePipelineLayout(device, &computeLayout, nullptr, &layout));

  VkShaderModule gradientShader;
  AR_LOG_ASSERT(
    vkutil::load_shader_module("shaders/push_gradient.comp.spv", device, &gradientShader),
    "Error loading Gradient Compute Effect Shader"
  );  

  VkPipelineShaderStageCreateInfo stageInfo = vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_COMPUTE_BIT, gradientShader);

  VkComputePipelineCreateInfo computePipelineCreateInfo{};
  computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  computePipelineCreateInfo.pNext = nullptr;
  computePipelineCreateInfo.layout = layout;
  computePipelineCreateInfo.stage = stageInfo;
    
  ComputeEffect gradient;
  gradient.layout = layout;
  gradient.name = "gradient";
  gradient.data = {};

  gradient.data.data1[0] = 0.1;
  gradient.data.data1[1] = 0.2;
  gradient.data.data1[2] = 0.25;
  gradient.data.data1[3] = 1;

  gradient.data.data2[0] = 0;
  gradient.data.data2[1] = 0;
  gradient.data.data2[2] = 1;
  gradient.data.data2[3] = 1;

  VK_CHECK_RESULT(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &gradient.pipeline));

  m_BackgroundEffects.push_back(gradient);

  vkDestroyShaderModule(device, gradientShader, nullptr);

  m_DeletionQueue.push_function([&, layout]() {
    vkDestroyPipelineLayout(device, layout, nullptr);
    for (const auto& bgEffect : m_BackgroundEffects)
    {
      vkDestroyPipeline(device, bgEffect.pipeline, nullptr);
    }
  });
}

void Engine::init_depth_prepass_pipeline()
{

  VkShaderModule zpassFrag;
  AR_LOG_ASSERT(
    vkutil::load_shader_module("shaders/zprepass/zprepass.frag.spv", device, &zpassFrag),
    "Error when building the shadow pass fragment shader module"
  );
  
  VkShaderModule zpassVert;
  AR_LOG_ASSERT(
    vkutil::load_shader_module("shaders/zprepass/zprepass.vert.spv", device, &zpassVert),
    "Error when building the shadow pass vertex shader module"
  );

  {
    DescriptorLayoutBuilder layoutBuilder;
    layoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    zpassDescriptorLayout = layoutBuilder.build(device, VK_SHADER_STAGE_VERTEX_BIT);
  }
 
  VkPushConstantRange range{};
  range.offset = 0;
  range.size = sizeof(shadow_map_pcs);
  range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  VkPipelineLayoutCreateInfo layout = vkinit::pipeline_layout_create_info();
  layout.setLayoutCount = 1;
  layout.pSetLayouts = &zpassDescriptorLayout;
  layout.pPushConstantRanges = &range;
  layout.pushConstantRangeCount = 1;
  VK_CHECK_RESULT(vkCreatePipelineLayout(device, &layout, nullptr, &zpassLayout));

  PipelineBuilder builder;
  builder.set_shaders(zpassVert, zpassFrag);
  builder.set_depth_format(m_DepthImage.imageFormat);
  builder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
  builder.set_polygon_mode(VK_POLYGON_MODE_FILL);
  builder.set_cull_mode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
  builder.set_multisampling_none();
  builder.disable_blending();
  builder.enable_depthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
  builder.PipelineLayout = zpassLayout;
  m_DepthPrepassPipeline = builder.build_pipeline(device);
  
  vkDestroyShaderModule(device, zpassVert, nullptr);
  vkDestroyShaderModule(device, zpassFrag, nullptr);

  m_DeletionQueue.push_function([&](){
    vkDestroyDescriptorSetLayout(device, zpassDescriptorLayout, nullptr);
    vkDestroyPipelineLayout(device, zpassLayout, nullptr);
    vkDestroyPipeline(device, m_DepthPrepassPipeline, nullptr);
  });
}

void Engine::init_shadow_map_pipeline()
{
  VkShaderModule shadowFrag;
  AR_LOG_ASSERT(
    vkutil::load_shader_module("shaders/shadow/shadow_map.frag.spv", device, &shadowFrag),
    "Error when building the shadow pass fragment shader module"
  );
  
  VkShaderModule shadowVert;
  AR_LOG_ASSERT(
    vkutil::load_shader_module("shaders/shadow/shadow_map.vert.spv", device, &shadowVert),
    "Error when building the shadow pass vertex shader module"
  );
 
  { 
    DescriptorLayoutBuilder builder;
    builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    m_ShadowSetLayout = builder.build(device, VK_SHADER_STAGE_VERTEX_BIT);
  }

  VkPushConstantRange range{};
  range.offset = 0;
  range.size = sizeof(shadow_map_pcs);
  range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  

  VkPipelineLayoutCreateInfo layoutInfo = vkinit::pipeline_layout_create_info();
  layoutInfo.setLayoutCount = 1;
  layoutInfo.pSetLayouts = &m_ShadowSetLayout;
  layoutInfo.pPushConstantRanges = &range;
  layoutInfo.pushConstantRangeCount = 1;
  VK_CHECK_RESULT(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &m_ShadowPipelineLayout));

  PipelineBuilder builder;
  builder.set_shaders(shadowVert, shadowFrag);
  builder.set_depth_format(m_DepthImage.imageFormat);
  builder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
  builder.set_polygon_mode(VK_POLYGON_MODE_FILL);
  builder.set_cull_mode(VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
  builder.set_multisampling_none();
  builder.disable_blending();
  builder.enable_depthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
  builder.PipelineLayout = m_ShadowPipelineLayout;
  m_ShadowPipeline = builder.build_pipeline(device);
  
  vkDestroyShaderModule(device, shadowFrag, nullptr);
  vkDestroyShaderModule(device, shadowVert, nullptr);

  m_DeletionQueue.push_function([&](){
    vkDestroyDescriptorSetLayout(device, m_ShadowSetLayout, nullptr);
    vkDestroyPipelineLayout(device, m_ShadowPipelineLayout, nullptr);
    vkDestroyPipeline(device, m_ShadowPipeline, nullptr);
  });
}

void Engine::init_sync_structures()
{
  VkFenceCreateInfo fence = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
  VkSemaphoreCreateInfo semaphore = vkinit::semaphore_create_info();
  for (int i = 0; i < FRAME_OVERLAP; i++)
  {
    VK_CHECK_RESULT(vkCreateFence(device, &fence, nullptr, &m_Frames[i].renderFence));
    VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphore, nullptr, &m_Frames[i].swapchainSemaphore));
    VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphore, nullptr, &m_Frames[i].renderSemaphore));
  }

  VK_CHECK_RESULT(vkCreateFence(device, &fence, nullptr, &m_ImmFence));
  m_DeletionQueue.push_function([=, this] {
    vkDestroyFence(device, m_ImmFence, nullptr);
  });
}

void Engine::init_commands()
{
  VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(graphicsIndex, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
  for (int i = 0; i < FRAME_OVERLAP; i++)
  {
    VK_CHECK_RESULT(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &m_Frames[i].commandPool));
    VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(m_Frames[i].commandPool, 1);
    VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdAllocInfo, &m_Frames[i].mainCommandBuffer));
  }
  
  // create immediate submit command pool & buffer
  VK_CHECK_RESULT(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &m_ImmCommandPool));
  VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(m_ImmCommandPool, 1);
  VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdAllocInfo, &m_ImmCommandBuffer));
  m_DeletionQueue.push_function([=, this]() {
    vkDestroyCommandPool(device, m_ImmCommandPool, nullptr);
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
	VK_CHECK_RESULT(vkCreateDescriptorPool(device, &pool_info, nullptr, &imguiPool));

  ImGui::CreateContext();
  ImGui_ImplGlfw_InitForVulkan(Window::get(), true);
  ImGui_ImplVulkan_InitInfo info{};
  info.Instance = m_Instance;
  info.PhysicalDevice = physicalDevice;
  info.Device = device;
  info.Queue = graphicsQueue;
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
    vkDestroyDescriptorPool(device, imguiPool, nullptr);
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

} // namespace aurora
