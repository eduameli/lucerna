#include "engine.h"
#include "application.h"
#include "aurora_pch.h"
#include "ar_asserts.h"
#include "input_structures.glsl"
#include "logger.h"
#include "vk_loader.h"
#include "window.h"
#include "vk_initialisers.h"
#include "vk_images.h"
#include "vk_descriptors.h"
#include "vk_pipelines.h"
#include "vk_device.h"
#include "vk_swapchain.h"
#include "gfx_effects.h"
#include <GLFW/glfw3.h>
#include <vulkan/vulkan_core.h>
#include "vk_types.h"
#include "imgui_backend.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"

#include <volk.h>


namespace Aurora {

AutoCVar_Int shadowEnabled("shadow_mapping.enabled", "is shadow mapping enabled", 0, CVarFlags::EditCheckbox);
AutoCVar_Int shadowViewFromLight("shadow_mapping.view_from_light", "view scene from view of directional light shadow caster", 0, CVarFlags::EditCheckbox);
AutoCVar_Int shadowRotateLight("shadow_mapping.rotate_light", "rotate light around origin showcasing real time shadows", 1, CVarFlags::EditCheckbox);
AutoCVar_Float shadowSoftness("shadow_mapping.softness", "radius of pcf sampling", 0.0025, CVarFlags::None);

AutoCVar_Int debugLinesEnabled("debug.show_lines", "", 0, CVarFlags::EditCheckbox);
AutoCVar_Int debugFrustumFreeze("debug.freeze_frustum", "", 0, CVarFlags::EditCheckbox);

AutoCVar_Float cameraFOV("camera.fov", "camera fov in degrees", 70.0f, CVarFlags::Advanced);
AutoCVar_Float cameraFar("camera.far", "", 10000.0, CVarFlags::Advanced);
AutoCVar_Float cameraNear("camera.near", "", 0.1, CVarFlags::Advanced);

AutoCVar_Int ssaoDisplayTexture("ssao.display_texture", "", 0, CVarFlags::EditCheckbox);
AutoCVar_Float ssaoKernelRadius("ssao.kernel_radius", "", 0.0, CVarFlags::None);
AutoCVar_Int ssaoEnabled("ssao.enabled", "", 1, CVarFlags::EditCheckbox);

void FrameGraph::add_sample(float sample)
{
  frametimes.push_back(sample);
  if (frametimes.size() == 120) frametimes.pop_front();
}

void FrameGraph::render_graph()
{

  float avg = std::accumulate(frametimes.begin(), frametimes.end(), 0.0f) / frametimes.size();
  float min = *std::min_element(frametimes.begin(), frametimes.end());
  float max = *std::max_element(frametimes.begin(), frametimes.end());
  
  double variance = 0.0;
  std::for_each(frametimes.begin(), frametimes.end(), [&](const float& val) {
      variance += std::pow(val - avg, 2);
  });

  variance /= frametimes.size();
  float sd = glm::sqrt(variance);
    
  std::string str = fmt::format("avg ms:  {}", avg);
  
  ImGui::Begin("Frame Graph", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::PlotLines("", [](void *data, int idx) -> float {
                    auto ft = static_cast<std::deque<float> *>(data);
                    return idx < ft->size() ? ft->at(idx) : 0.0f;
                }, &frametimes, frametimes.size(), 0, nullptr, avg - 8*sd, avg + 8*sd, ImVec2{190, 100});

    ImGui::Text("avg: %.3f", avg);
    ImGui::Text(" sd: %.3f\t cv: %.3f", sd, sd/avg);
    ImGui::Text("min: %.3f\tmax: %.3f", min, max);
  ImGui::End();
}

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

  VkSamplerCreateInfo s{.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr};
  s.maxLod = VK_LOD_CLAMP_NONE;
  s.minLod = 0;
  s.magFilter = VK_FILTER_LINEAR;
  s.minFilter = VK_FILTER_LINEAR;

  s.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  vkCreateSampler(device, &s, nullptr, &bindless_sampler);  

  
  
  
  init_swapchain();
  init_commands();
  init_sync_structures();
  init_descriptors();
  init_pipelines();
  init_imgui(); 
  init_default_data();
  


  
  mainCamera.init();


  mainDrawContext.indices.clear();
  mainDrawContext.vertices.clear();
  mainDrawContext.positions.clear();

   
  std::string structurePath = Application::config.scene_path;
  auto structureFile = load_gltf(this, structurePath);
  AR_LOG_ASSERT(structureFile.has_value(), "gltf loaded correctly!");

  loadedScenes["structure"] = *structureFile;
  loadedScenes["structure"]->queue_draw(glm::mat4{1.0f}, mainDrawContext); // set_draws
  
  AR_CORE_INFO("positions {} vertices {} indices {}", mainDrawContext.positions.size(), mainDrawContext.vertices.size(), mainDrawContext.indices.size());


  uint32_t idx = 0;
  // std::vector<VkDrawIndexedIndirectCommand> mem;
  for (int i = 0; i < mainDrawContext.draw_datas.size(); i++)
  {
    DrawData dd = mainDrawContext.draw_datas[i];
    Bounds b = mainDrawContext.bounds[i];

    glm::vec4 b2 = {b.origin.x, b.origin.y, b.origin.z, b.sphereRadius};
    
    IndirectDraw c{};
    c.indexCount = dd.indexCount;
    c.firstIndex = dd.firstIndex;
    c.firstInstance = idx;
    c.instanceCount = 1;
    c.vertexOffset = 0;
    c.sphereBounds = b2;
    mainDrawContext.mem.push_back(c);
    idx++;
  }
  // memcpy(data4, mem.data(), mem.size() * sizeof(VkDrawIndexedIndirectCommand));
  // vklog::label_buffer(device,indirectDrawBuffer.buffer, "big indirect draw cmd buffer");


  
  GPUSceneBuffers main = upload_scene(
    mainDrawContext.positions,
    mainDrawContext.vertices,
    mainDrawContext.indices,
    mainDrawContext.transforms,
    mainDrawContext.draw_datas,
    mainDrawContext.materials,
    mainDrawContext.mem
  );


  AR_CORE_INFO("[resource] Finished uploading scene buffers to gpu!");

  mainDrawContext.bigMeshes.indexBuffer = main.indexBuffer;
  mainDrawContext.bigMeshes.vertexBuffer = main.vertexBuffer;
  mainDrawContext.bigMeshes.positionBuffer = main.positionBuffer;
  indirectDrawBuffer = main.indirectCmdBuffer;
  bigTransformBuffer = main.transformBuffer;
  bigMaterialBuffer = main.materialBuffer;
  bigDrawDataBuffer = main.drawDataBuffer;
  
  
  // prepare shadow uniform
  shadowPass.buffer = create_buffer(sizeof(u_ShadowPass), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
  m_DeletionQueue.push_function([=, this] {
    destroy_buffer(shadowPass.buffer);
    destroy_buffer(bigTransformBuffer);
    destroy_buffer(bigMaterialBuffer);
    destroy_buffer(bigDrawDataBuffer);
    destroy_buffer(indirectDrawBuffer);

    
    destroy_buffer(mainDrawContext.bigMeshes.indexBuffer);
    destroy_buffer(mainDrawContext.bigMeshes.vertexBuffer);
    destroy_buffer(mainDrawContext.bigMeshes.positionBuffer);
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

    FrameGraph::add_sample(stats.frametime);

    
    descriptor_updates.clear();
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
    .set_preferred_present(VkPresentModeKHR::VK_PRESENT_MODE_FIFO_KHR)
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

  if (debugFrustumFreeze.get() == false)
  {
    lastDebugFrustum = sceneData.viewproj;
  }
  queue_debug_frustum(glm::inverse(lastDebugFrustum));

  
  if (shadowViewFromLight.get() == true)
  {
    sceneData.viewproj = lightProj * lView;
    sceneData.view = lView;
    sceneData.proj = lightProj;
  }
  
	sceneData.ambientColor = glm::vec4(0.1f); // would it be like a skybox
  sceneData.sunlightColor = glm::vec4(1.0f);


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


  // NOTE: do i need to bind every frame? sceneData or every pipeline??
  // vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, bindless_pipeline_layout, 1, 1, &bindless_descriptor_set, 0, nullptr);
  // vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, bindless_pipeline_layout, 0, 1, &global_descriptor_set, 0, nullptr);

  do_culling(cmd);
  


  
  // draw compute bg
  vkutil::transition_image(cmd, m_DrawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
  draw_background(cmd);

  // draw depth prepass first to be able to overlap shadow mapping & screen space (depth based) compute effects
  vkutil::transition_image(cmd, m_DepthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
  draw_depth_prepass(cmd);

  vkutil::transition_image(cmd, m_ShadowDepthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
  draw_shadow_pass(cmd);
  vkutil::transition_image(cmd, m_ShadowDepthImage.image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL);


  if (ssaoEnabled.get())
  {
    vkutil::transition_image(cmd, m_DepthImage.image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL);
    // depth based compute ss effects
    ssao::run(cmd, m_DepthImage.imageView);
    vkutil::transition_image(cmd, m_DepthImage.image, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
  }

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


  // update descriptor sets?
  update_descriptors();


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
  // shadow_draws.reserve(mainDrawContext.OpaqueSurfaces.size());
  
  // for (uint32_t i = 0; i < mainDrawContext.OpaqueSurfaces.size(); i++)
  // {
  //   if (is_visible(mainDrawContext.OpaqueSurfaces[i], lightProj*lView))
  //   {
  //     shadow_draws.push_back(i);
  //   }
  // }

  // NOTE: CMS Settings?? different mat proj or a scale to basic 1 or smth
  
  u_ShadowPass data;
  data.lightViewProj = lightProj * lView;

  u_ShadowPass* shadowPassUniform = (u_ShadowPass*) shadowPass.buffer.allocation->GetMappedData();
  *shadowPassUniform = data;

  VkDescriptorSet shadowDescriptor = get_current_frame().frameDescriptors.allocate(device, m_ShadowSetLayout);
  DescriptorWriter writer;
  writer.write_buffer(0, shadowPass.buffer.buffer, sizeof(u_ShadowPass), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER); // FIXME: .buffer .buffer :sob:

  writer.write_buffer(1, bigDrawDataBuffer.buffer, mainDrawContext.draw_datas.size() * sizeof(DrawData), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
  writer.write_buffer(2, bigTransformBuffer.buffer, mainDrawContext.transforms.size() * sizeof(glm::mat4x3), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
  writer.write_buffer(3, mainDrawContext.bigMeshes.positionBuffer.buffer, mainDrawContext.positions.size() * sizeof(glm::vec3), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
     
  writer.update_set(device, shadowDescriptor);
  
  VkViewport viewport = vkinit::dynamic_viewport(m_ShadowExtent);
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor = vkinit::dynamic_scissor(m_ShadowExtent);
  vkCmdSetScissor(cmd, 0, 1, &scissor);
  
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ShadowPipeline);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ShadowPipelineLayout, 0, 1, &shadowDescriptor, 0, nullptr);
  
  // VkBuffer lastIndexBuffer = VK_NULL_HANDLE;
  // auto draw = [&](const RenderObject& draw) {
  //   if (draw.indexBuffer != lastIndexBuffer)
  //   {
  //     lastIndexBuffer = draw.indexBuffer;
  //     vkCmdBindIndexBuffer(cmd, draw.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
  //   }

  //   shadow_map_pcs pcs{};
  //   pcs.modelMatrix = draw.transform; // worldMatrix == modelMatrix
  //   pcs.positions = draw.positionBufferAddress; 
   
  //   // scuffed way to not render ground plane to shadow map
  //   vkCmdPushConstants(cmd, m_ShadowPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(shadow_map_pcs), &pcs);
  //   vkCmdDrawIndexed(cmd, draw.indexCount, 1, draw.firstIndex, 0, 0);
  // };



  
  vkCmdBindIndexBuffer(cmd, mainDrawContext.bigMeshes.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
  vkCmdDrawIndexedIndirect(cmd, indirectDrawBuffer.buffer, 0, mainDrawContext.draw_datas.size(), sizeof(IndirectDraw));
  // for (auto& r : shadow_draws)
  // {
  //   draw(mainDrawContext.OpaqueSurfaces[r]);
  // }

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

  VkDescriptorSet depth = get_current_frame().frameDescriptors.allocate(device, zpassDescriptorLayout);
  DescriptorWriter writer;
  writer.write_buffer(0, gpuSceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER); // FIXME: .buffer .buffer :sob:
  writer.write_buffer(2, bigTransformBuffer.buffer, mainDrawContext.transforms.size() * sizeof(glm::mat4x3), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
  writer.write_buffer(4, mainDrawContext.bigMeshes.positionBuffer.buffer, mainDrawContext.positions.size() * sizeof(glm::vec3), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
  writer.write_buffer(1, bigDrawDataBuffer.buffer, mainDrawContext.draw_datas.size() * sizeof(DrawData), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
  writer.write_buffer(5, mainDrawContext.bigMeshes.vertexBuffer.buffer, mainDrawContext.vertices.size() * sizeof(Vertex), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
  writer.write_buffer(3, bigMaterialBuffer.buffer, mainDrawContext.materials.size() * sizeof(BindlessMaterial), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
  writer.update_set(device, depth);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_DepthPrepassPipeline);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, zpassLayout, 0, 1, &depth, 0, nullptr);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, zpassLayout, 1, 1, &bindless_descriptor_set, 0, nullptr); 

  vkCmdBindIndexBuffer(cmd, mainDrawContext.bigMeshes.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

  vkCmdDrawIndexedIndirect(cmd, indirectDrawBuffer.buffer, 0, mainDrawContext.draw_datas.size(), sizeof(IndirectDraw));

  
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
  
  // AllocatedBuffer gpuSceneDataBuffer = create_buffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
  // get_current_frame().deletionQueue.push_function([=, this] {
  //   destroy_buffer(gpuSceneDataBuffer);
  // });

  AllocatedBuffer shadowSettings = create_buffer(sizeof(ShadowFragmentSettings), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
  get_current_frame().deletionQueue.push_function([=, this] {
    destroy_buffer(shadowSettings);
  });

  // GPUSceneData* sceneUniformData = (GPUSceneData*) gpuSceneDataBuffer.allocation->GetMappedData();
  // *sceneUniformData = sceneData;

  ShadowFragmentSettings* settings = (ShadowFragmentSettings*) shadowSettings.allocation->GetMappedData();
  settings->lightViewProj = pcss_settings.lightViewProj;
  settings->near = 0.1;
  settings->far = 20.0;
  settings->light_size = 0.1;
  settings->enabled = shadowEnabled.get();
  settings->softness = shadowSoftness.get();
  settings->texture_idx = m_ShadowDepthImage.texture_idx; // how to give it a specific sampler

  
  AllocatedBuffer sceneDataBuf = create_buffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
  get_current_frame().deletionQueue.push_function([=, this](){
     destroy_buffer(sceneDataBuf);
  });

  GPUSceneData* sceneUniformData = (GPUSceneData*) sceneDataBuf.allocation->GetMappedData();
  *sceneUniformData = sceneData;


  VkDescriptorSet globalDescriptor = get_current_frame().frameDescriptors.allocate(device, m_SceneDescriptorLayout);
  DescriptorWriter writer;
  writer.write_buffer(0, sceneDataBuf.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  writer.write_image(1, m_ShadowDepthImage.imageView, m_ShadowSampler, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  writer.write_buffer(2, shadowSettings.buffer, sizeof(ShadowFragmentSettings), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  writer.write_image(3, ssao::outputBlurred.imageView, m_DefaultSamplerLinear, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  
  writer.write_buffer(4, bigDrawDataBuffer.buffer, mainDrawContext.draw_datas.size() * sizeof(DrawData), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
  writer.write_buffer(5, bigTransformBuffer.buffer, mainDrawContext.transforms.size() * sizeof(glm::mat4x3), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
  writer.write_buffer(6, bigMaterialBuffer.buffer, mainDrawContext.materials.size() * sizeof(BindlessMaterial), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
  
  writer.write_buffer(7, mainDrawContext.bigMeshes.positionBuffer.buffer, mainDrawContext.positions.size() * sizeof(glm::vec3), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
  writer.write_buffer(8, mainDrawContext.bigMeshes.vertexBuffer.buffer, mainDrawContext.vertices.size() * sizeof(Vertex), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
  writer.update_set(device, globalDescriptor);
  
  VkViewport viewport = vkinit::dynamic_viewport(m_DrawExtent);
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor = vkinit::dynamic_scissor(m_DrawExtent);
  vkCmdSetScissor(cmd , 0, 1, &scissor);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, std_pipeline);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, bindless_pipeline_layout, 1, 1, &bindless_descriptor_set, 0, nullptr);  
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, bindless_pipeline_layout, 0, 1, &globalDescriptor, 0, nullptr);


  
  // const size_t vertexBufferSize = mainDrawContext.vertices.size() * sizeof(Vertex);
  // const size_t positionBufferSize = mainDrawContext.positions.size() * sizeof(glm::vec3);
 
  vkCmdBindIndexBuffer(cmd, mainDrawContext.bigMeshes.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
  // AR_CORE_INFO("indeces {}", mainDrawContext.indices.size());
  // vkCmdBindIndexBuffer(cmd, mainDrawContext.OpaqueSurfaces[0].indexBuffer, 0, VK_INDEX_TYPE_UINT32);
  // FIXME: if u remove many then u get gaps?? burh this is hella complex dont support unloading meshes... only streaming!

  vkCmdDrawIndexedIndirect(cmd, indirectDrawBuffer.buffer, 0, mainDrawContext.mem.size(), sizeof(IndirectDraw));

  // AR_CORE_INFO("draw datas {}", mainDrawContext.draw_datas.size());

  // vkCmdDrawIndexed(cmd, 36, 1, 0, 0, 0);

  
  vkCmdEndRendering(cmd);  

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
  std::array<glm::vec3, 8> v {
      glm::vec3 { 1, 1, -1 },
      glm::vec3 { 1, 1, 0 },
      glm::vec3 { 1, -1, -1 },
      glm::vec3 { 1, -1, 0 },
      glm::vec3 { -1, 1, -1 },
      glm::vec3 { -1, 1, 0 },
      glm::vec3 { -1, -1, -1 },
      glm::vec3 { -1, -1, 0 },
  };

  for (glm::vec3& p : v)
  {
    glm::vec4 p2 = proj * glm::vec4(p, 1.0);
    p = glm::vec3(p2.x, p2.y, p2.z) / glm::vec3(p2.w);
    // AR_CORE_INFO("{}", glm::to_string(p));
  }

  queue_debug_line(v[0], v[1]);
  queue_debug_line(v[2], v[3]);
  queue_debug_line(v[4], v[5]);
  queue_debug_line(v[6], v[7]);


  queue_debug_line(v[0], v[2]);
  queue_debug_line(v[2], v[6]);
  queue_debug_line(v[6], v[2]);
  queue_debug_line(v[2], v[0]);


  queue_debug_line(v[1], v[3]);
  queue_debug_line(v[3], v[7]);
  queue_debug_line(v[7], v[5]);
  queue_debug_line(v[5], v[1]);



}

void Engine::queue_debug_obb(glm::mat4 transform, glm::vec3 origin, glm::vec3 extents)
{
    // Calculate the 8 vertices of the OBB from the origin and extents
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

    // Transform the vertices using the provided transformation matrix
    // for (int i = 0; i < 8; ++i) {
    //     glm::vec4 transformedVertex = transform * glm::vec4(v[i], 1.0f); // Apply transform to each vertex
    //     v[i] = glm::vec3(transformedVertex.x, transformedVertex.y, transformedVertex.z);
    // }

    // Queue the debug lines for the OBB (lines between vertices)
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



// FIXME: should only be one buffer at a time renderdoc is cooked
void Engine::draw_debug_lines(VkCommandBuffer cmd)
{

  //queue_debug_obb(glm::vec3{0.0f}, glm::vec3{2.0f});
  if (debugLinesEnabled.get() == false || debugLines.size() == 0)
  {
    return;
  } 
  
  AR_LOG_ASSERT(debugLines.size() % 2 == 0, "Debug Lines buffer must be a multiple of two");
 
  VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(m_DrawImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  VkRenderingInfo renderInfo = vkinit::rendering_info(m_DrawExtent, &colorAttachment,  nullptr);
  vkCmdBeginRendering(cmd, &renderInfo);


  debugLinesBuffer = create_buffer(debugLines.size() * sizeof(glm::vec3), VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
  std::string n = "debug lines buffer" + std::to_string(frameNumber);
  vklog::label_buffer(device, debugLinesBuffer.buffer, n.c_str()); // FIXME: kind of ass string manipulation also in gfx_effects.cpp bloom!
  glm::vec3* data = (glm::vec3*) debugLinesBuffer.allocation->GetMappedData();
  memcpy(data, debugLines.data(), debugLines.size() * sizeof(glm::vec3)); 
  
  //vmaFlushAllocation(m_Allocator,debugLinesBuffer.allocation , 0 , VK_WHOLE_SIZE); // FIXME: if its host coherent i dont need this??
  
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

  debugLines.clear();

  vkCmdEndRendering(cmd);

  // get_current_frame().deletionQueue.push_function([=, this]{
    destroy_buffer(debugLinesBuffer);                                                  
  // });
}

void Engine::draw_imgui(VkCommandBuffer cmd, VkImageView target)
{
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  ImGui::Begin("Stats", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::Text("frame: %zu", frameNumber);
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
    ImGui::End();
    
    CVarSystem::get()->draw_editor();

    FrameGraph::render_graph();

    // graphics programming stats menu
    ImGui::Begin("better stats");
      float framesPerSecond = 1.0f / (stats.frametime * 0.001f);
      ImGui::Text("afps: %.0f rad/s", glm::two_pi<float>() * framesPerSecond);
      ImGui::Text("dfps: %.0f °/s", glm::degrees(glm::two_pi<float>() * framesPerSecond));
      ImGui::Text("rfps: %.0f", framesPerSecond);
      ImGui::Text("rpms: %.0f", framesPerSecond * 60.0f);
      ImGui::Text("  ft: %.2f ms", stats.frametime);
      ImGui::Text("   f: %lu", frameNumber);
    ImGui::End();


    ImGui::Begin("Texture Picker");
      static int32_t texture_idx = 0;
      ImGui::Text("Bindless Texture Picker");
      ImGui::InputInt("texture idx", &texture_idx);
      ImGui::Image((ImTextureID) (uint64_t) texture_idx, ImVec2{250, 250});
    ImGui::End();
  // */
  ImGui::EndFrame();
  ImGui::Render();
  VulkanImGuiBackend::draw(cmd, device, target,m_Swapchain.extent2d);

  return;
  
 
  
  // ImGui::End();

  // ImGui::Render(); 
  // VulkanImGuiBackend::draw(cmd, device, target, m_Swapchain.extent2d);

  // VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(target, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  // VkRenderingInfo renderInfo = vkinit::rendering_info(m_Swapchain.extent2d, &colorAttachment, nullptr);
  // vkCmdBeginRendering(cmd, &renderInfo);
  // ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
  // ImGui::Render();

  
  // vkCmdEndRendering(cmd);
}

// FIXME: utterly scuffed, i ignore the min max .y for the aabb collision test between the cannonical viewing volume and the projected obb
bool Engine::is_visible(const RenderObject& obj, const glm::mat4& viewproj) {
    return true; // still gets false positives even when ignoring miny maxy
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
        v.y =  v.y / v.w;
        v.z = v.z / v.w;

        min = glm::min(glm::vec3 { v.x, v.y, v.z }, min);
        max = glm::max(glm::vec3 { v.x, v.y, v.z }, max);

        // AR_CORE_WARN("clip space coords({}):",std::to_string(i),  glm::to_string(v));
    }

    return !(min.z > 1.f || max.z < 0.0f || min.x > 1.f || max.x < -1.f /*|| min.y > 1.f || max.y < -1.f*/);
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


GPUSceneBuffers Engine::upload_scene(
  std::span<glm::vec3> positions,
  std::span<Vertex> vertices,
  std::span<uint32_t> indices,
  std::span<glm::mat4x3> transforms,
  std::span<DrawData> draw_datas,
  std::span<BindlessMaterial> materials,
  std::span<IndirectDraw> indirect_cmds)
{
  const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
  const size_t positionBufferSize = positions.size() * sizeof(glm::vec3);
  const size_t indexBufferSize = indices.size() * sizeof(uint32_t);
  const size_t transformBufferSize = transforms.size() * sizeof(glm::mat4x3);
  const size_t drawDataBufferSize = draw_datas.size() * sizeof(DrawData);
  const size_t materialBufferSize = materials.size() * sizeof(BindlessMaterial);
  const size_t indirectCmdBufferSize = indirect_cmds.size() * sizeof(IndirectDraw);
  
  
  GPUSceneBuffers scene;

  scene.vertexBuffer = create_buffer(
    vertexBufferSize,
    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    VMA_MEMORY_USAGE_GPU_ONLY
  );

  scene.positionBuffer = create_buffer(
    positionBufferSize,
    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    VMA_MEMORY_USAGE_GPU_ONLY
  );
    
  scene.indexBuffer = create_buffer(
    indexBufferSize,
    VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    VMA_MEMORY_USAGE_GPU_ONLY
  );
    
  scene.drawDataBuffer = create_buffer(
    drawDataBufferSize,
    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    VMA_MEMORY_USAGE_GPU_ONLY
  );

  scene.transformBuffer = create_buffer(
    transformBufferSize,
    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    VMA_MEMORY_USAGE_GPU_ONLY
  );

  scene.materialBuffer = create_buffer(
    materialBufferSize,
    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    VMA_MEMORY_USAGE_GPU_ONLY
  );
  scene.indirectCmdBuffer = create_buffer(
    indirectCmdBufferSize,
    VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    VMA_MEMORY_USAGE_GPU_ONLY
  );


  vklog::label_buffer(device, scene.vertexBuffer.buffer, "big vertex buffer");
  vklog::label_buffer(device, scene.positionBuffer.buffer, "big position buffer");
  vklog::label_buffer(device, scene.indexBuffer.buffer, "big index buffer");
  vklog::label_buffer(device, scene.drawDataBuffer.buffer, "big draw data buffer");
  vklog::label_buffer(device, scene.transformBuffer.buffer, "big transform buffer");
  vklog::label_buffer(device, scene.materialBuffer.buffer, "big material buffer");
  vklog::label_buffer(device, scene.indirectCmdBuffer.buffer, "big indirect cmd buffer");



  AllocatedBuffer staging = create_buffer(
    vertexBufferSize +
    positionBufferSize +
    indexBufferSize +
    drawDataBufferSize +
    transformBufferSize +
    materialBufferSize +
    indirectCmdBufferSize,
    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    VMA_MEMORY_USAGE_CPU_ONLY);
  void* data = staging.allocation->GetMappedData();

  memcpy(data, positions.data(), positionBufferSize);
  memcpy((char*) data + positionBufferSize, vertices.data(), vertexBufferSize);
  memcpy((char*) data + positionBufferSize + vertexBufferSize, indices.data(), indexBufferSize); //FIXME: C style casting
  // pos, vert, idx

  // draw data, transform, material, indirect_cmd
  memcpy((char*) data + positionBufferSize + vertexBufferSize + indexBufferSize, draw_datas.data(), drawDataBufferSize);
  memcpy((char*) data + positionBufferSize + vertexBufferSize + indexBufferSize + drawDataBufferSize, transforms.data(), transformBufferSize);
  memcpy((char*) data + positionBufferSize + vertexBufferSize + indexBufferSize + drawDataBufferSize + transformBufferSize, materials.data(), materialBufferSize);
  memcpy((char*) data + positionBufferSize + vertexBufferSize + indexBufferSize + drawDataBufferSize + transformBufferSize + materialBufferSize, indirect_cmds.data(), indirectCmdBufferSize);



  immediate_submit([&](VkCommandBuffer cmd){
    VkBufferCopy positionCopy{};
    positionCopy.dstOffset = 0;
    positionCopy.srcOffset = 0;
    positionCopy.size = positionBufferSize;
    
    vkCmdCopyBuffer(cmd, staging.buffer, scene.positionBuffer.buffer, 1, &positionCopy);

    VkBufferCopy vertexCopy{};
    vertexCopy.dstOffset = 0;
    vertexCopy.srcOffset = positionBufferSize;
    vertexCopy.size = vertexBufferSize;
    vkCmdCopyBuffer(cmd, staging.buffer, scene.vertexBuffer.buffer, 1, &vertexCopy);
    
    VkBufferCopy indexCopy{};
    indexCopy.dstOffset = 0;
    indexCopy.srcOffset = positionBufferSize + vertexBufferSize;
    indexCopy.size = indexBufferSize;
    vkCmdCopyBuffer(cmd, staging.buffer, scene.indexBuffer.buffer, 1, &indexCopy);

                
    VkBufferCopy drawCopy{};
    drawCopy.dstOffset = 0;
    drawCopy.srcOffset = positionBufferSize + vertexBufferSize + indexBufferSize;
    drawCopy.size = drawDataBufferSize;
    vkCmdCopyBuffer(cmd, staging.buffer, scene.drawDataBuffer.buffer, 1, &drawCopy);

    VkBufferCopy materialCopy{};
    materialCopy.dstOffset = 0;
    materialCopy.srcOffset = positionBufferSize + vertexBufferSize + indexBufferSize + drawDataBufferSize + transformBufferSize;
    materialCopy.size = materialBufferSize;
    vkCmdCopyBuffer(cmd, staging.buffer, scene.materialBuffer.buffer, 1, &materialCopy);

    VkBufferCopy transformCopy{};
    transformCopy.dstOffset = 0;
    transformCopy.srcOffset = positionBufferSize + vertexBufferSize + indexBufferSize + drawDataBufferSize;
    transformCopy.size = transformBufferSize;
    vkCmdCopyBuffer(cmd, staging.buffer, scene.transformBuffer.buffer, 1, &transformCopy);


    VkBufferCopy cmdCopy{};
    cmdCopy.dstOffset = 0;
    cmdCopy.srcOffset = positionBufferSize + vertexBufferSize + indexBufferSize + drawDataBufferSize + transformBufferSize + materialBufferSize;
    cmdCopy.size = indirectCmdBufferSize;
    vkCmdCopyBuffer(cmd, staging.buffer, scene.indirectCmdBuffer.buffer, 1, &cmdCopy);

    AR_CORE_INFO("size a {}, size b {}", transformCopy.size, cmdCopy.size);
  });

  


  destroy_buffer(staging);


  
  return scene;
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
  memcpy((uint8_t*) data + positionBufferSize, vertices.data(), vertexBufferSize);
  memcpy((uint8_t*) data + positionBufferSize + vertexBufferSize, indices.data(), indexBufferSize); //FIXME: C style casting

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
  std::string pos = "POSITION BUFFER";
  
  vklog::label_buffer(device, newSurface.positionBuffer.buffer, pos.c_str());

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



  bool is_sampled = usage & VK_IMAGE_USAGE_SAMPLED_BIT;
  bool is_storage = usage & VK_IMAGE_USAGE_STORAGE_BIT;

  if (is_sampled)
  {
    sampledCounter++;
    newImage.texture_idx = sampledCounter;
  }

  if (is_storage)
  {
    newImage.image_idx = freeImages.allocate();
  }


  descriptor_updates.push_back(newImage);
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


  m_DeletionQueue.push_function([&]{
    vkDestroySampler(device, m_DefaultSamplerLinear, nullptr);
    vkDestroySampler(device, m_DefaultSamplerNearest, nullptr);
    vkDestroySampler(device, m_ShadowSampler, nullptr); 
    vkDestroySampler(device, bindless_sampler, nullptr); // not created here? should it be destroyed here?

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
  app.pEngineName = "Lucerna";
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
    .set_preferred_gpu_type(VkPhysicalDeviceType::VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
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
    .set_preferred_present(VkPresentModeKHR::VK_PRESENT_MODE_FIFO_KHR)
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
    {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
    {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
    {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
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
    builder.add_binding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);


    // for draw indirect
    builder.add_binding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    builder.add_binding(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    builder.add_binding(6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

    builder.add_binding(7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    builder.add_binding(8, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
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

/*
NOTE: uniforms layout
binding 0 - sceneData (UBO)
binding 1 - sampler img
binding 2 - storage img

set 1 samplers
set 2 images
anything else?

ssbo - using bda
*/
void Engine::init_bindless_pipeline_layout()
{
  
  std::array<VkDescriptorPoolSize, 3> pool_size_bindless =
  {
    VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, SAMPLED_IMAGE_COUNT},
    VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLER, SAMPLER_COUNT},
    VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, STORAGE_IMAGE_COUNT}
  };

  VkDescriptorPoolCreateInfo pool_info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, .pNext = nullptr};
  pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
  pool_info.maxSets = 1;
  pool_info.poolSizeCount = pool_size_bindless.size();
  pool_info.pPoolSizes = pool_size_bindless.data();
  VK_CHECK_RESULT(vkCreateDescriptorPool(device, &pool_info, nullptr, &bindless_descriptor_pool));

  VkDescriptorSetLayoutBinding binding[3]; // spec guarantees 4
  VkDescriptorSetLayoutBinding& sampledImg = binding[0];
  sampledImg.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  sampledImg.descriptorCount = SAMPLED_IMAGE_COUNT;
  sampledImg.binding = SAMPLED_IMAGE_BINDING;
  sampledImg.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
  sampledImg.pImmutableSamplers = nullptr;

  VkDescriptorSetLayoutBinding& sampler = binding[1];
  sampler.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
  sampler.descriptorCount = SAMPLER_COUNT;
  sampler.binding = SAMPLER_BINDING;
  sampler.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
  sampler.pImmutableSamplers = nullptr;
  
  VkDescriptorSetLayoutBinding& storageImg = binding[2];
  storageImg.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  storageImg.descriptorCount = STORAGE_IMAGE_COUNT;
  storageImg.binding = STORAGE_IMAGE_BINDING;
  storageImg.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
  storageImg.pImmutableSamplers = nullptr;
    
  VkDescriptorSetLayoutCreateInfo layout_info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .pNext = nullptr};
  layout_info.bindingCount = pool_size_bindless.size();
  layout_info.pBindings = binding;
  layout_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;

  VkDescriptorBindingFlags bindless_flags = 
    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT;

  VkDescriptorBindingFlags binding_flags[3];
  binding_flags[0] = bindless_flags;
  binding_flags[1] = bindless_flags;
  binding_flags[2] = bindless_flags;

  VkDescriptorSetLayoutBindingFlagsCreateInfo extend_info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO, .pNext = nullptr};
  extend_info.bindingCount = pool_size_bindless.size();
  extend_info.pBindingFlags = binding_flags;

  layout_info.pNext = &extend_info;
  VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &layout_info, nullptr, &bindless_descriptor_layout));


  VkDescriptorSetAllocateInfo alloc_info{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, .pNext = nullptr};
  alloc_info.descriptorPool = bindless_descriptor_pool;
  alloc_info.descriptorSetCount = 1;
  alloc_info.pSetLayouts = &bindless_descriptor_layout;
  VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &alloc_info, &bindless_descriptor_set));


  global_descriptor_set = globalDescriptorAllocator.allocate(device, m_SceneDescriptorLayout);
  
  VkPushConstantRange range{};
  range.offset = 0;
  range.size = 128;
  range.stageFlags = VK_SHADER_STAGE_ALL;

  std::array<VkDescriptorSetLayout, 2> layouts = 
  {
    m_SceneDescriptorLayout,
    bindless_descriptor_layout,
  };
  
  VkPipelineLayoutCreateInfo inf{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .pNext = nullptr};
  inf.setLayoutCount = layouts.size();
  inf.pSetLayouts = layouts.data();
  inf.pushConstantRangeCount = 1;
  inf.pPushConstantRanges = &range;
  VK_CHECK_RESULT(vkCreatePipelineLayout(device, &inf, nullptr, &bindless_pipeline_layout));


  m_DeletionQueue.push_function([=, this](){
    vkDestroyDescriptorSetLayout(device, bindless_descriptor_layout, nullptr);
    vkDestroyPipelineLayout(device, bindless_pipeline_layout, nullptr);
    vkDestroyDescriptorPool(device, bindless_descriptor_pool, nullptr);
  });
 
}


// batch (bindless) descriptor update
// FIXME: kind of ass way to ensure pImageInfo is valid cus of vector realloc
// FIXME: images are stored in gltf_file.images, instead of building a new vector of AllocatedImage and have duplicated data
// it should ref ptr to that gltf
void Engine::update_descriptors()
{

  if (descriptor_updates.size() == 0) return;

  std::vector<VkWriteDescriptorSet> writes;
  std::vector<VkDescriptorImageInfo> info;
  writes.reserve(descriptor_updates.size() * 2);
  info.reserve(descriptor_updates.size() * 2);

  
  for (int i = 0; i < descriptor_updates.size(); i++)
  {
    AllocatedImage img = descriptor_updates[i];
    
    if (img.texture_idx != UINT32_MAX)
    {
      VkWriteDescriptorSet w;
      w = {};
      w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      w.pNext = nullptr;
      w.dstSet = bindless_descriptor_set;
      w.descriptorCount = 1;
      w.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
      w.dstArrayElement = img.texture_idx;
      w.dstBinding = SAMPLED_IMAGE_BINDING;


      VkDescriptorImageInfo inf{};
      inf.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
      inf.imageView = img.imageView;

      info.push_back(inf);
      w.pImageInfo = &info.back();
      writes.push_back(w);
    }

    if (img.image_idx != UINT32_MAX)
    {
      VkWriteDescriptorSet w{};
      w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      w.pNext = nullptr;
      w.dstSet = bindless_descriptor_set;
      w.descriptorCount = 1;
      w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
      w.dstArrayElement = img.image_idx;
      w.dstBinding = STORAGE_IMAGE_BINDING;

      VkDescriptorImageInfo inf{};
      inf.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
      inf.imageView = img.imageView;
      inf.sampler = nullptr;

      // w.pImageInfo = &inf;
      info.push_back(inf);
      w.pImageInfo = &info.back();
      writes.push_back(w);
    }
  }
  vkUpdateDescriptorSets(device, writes.size() , writes.data(), 0, nullptr);
}

//FIXME: this whole function is wack i should divide it up .. or move to init_descriptors or smth
void Engine::init_pipelines()
{
  init_background_pipelines(); // compute backgrounds

  init_bindless_pipeline_layout();
  init_depth_prepass_pipeline();
  init_shadow_map_pipeline();
  init_indirect_cull_pipeline();

  VkShaderModule bindlessFrag, bindlessVert;
  AR_LOG_ASSERT(
    vkutil::load_shader_module("shaders/bindless/bindless.frag.spv", device, &bindlessFrag),
    "Error when building the bindless shader module frag"
  );

  
  AR_LOG_ASSERT(
    vkutil::load_shader_module("shaders/bindless/bindless.vert.spv", device, &bindlessVert),
    "Error when building the bindless shader module vert"
  );

  PipelineBuilder b;
  b.set_shaders(bindlessVert, bindlessFrag);
  b.set_color_attachment_format(m_DrawImage.imageFormat);
  b.set_depth_format(m_DepthImage.imageFormat);
  b.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
  b.set_polygon_mode(VK_POLYGON_MODE_FILL);
  b.set_cull_mode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
  b.set_multisampling_none();
  b.disable_blending();
  b.enable_depthtest(true, VK_COMPARE_OP_EQUAL);
  
  
  b.PipelineLayout = bindless_pipeline_layout;
  std_pipeline = b.build_pipeline(device);


  vkDestroyShaderModule(device, bindlessFrag, nullptr);
  vkDestroyShaderModule(device, bindlessVert, nullptr);

  m_DeletionQueue.push_function([=, this](){
    vkDestroyPipeline(device, std_pipeline, nullptr);
  });


  vklog::label_pipeline(device, std_pipeline, "bindless pipeline!");

  
  
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
layoutBuilder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
layoutBuilder.add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
layoutBuilder.add_binding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
layoutBuilder.add_binding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
layoutBuilder.add_binding(5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

    zpassDescriptorLayout = layoutBuilder.build(device, VK_SHADER_STAGE_VERTEX_BIT);
  }
 


  std::array<VkDescriptorSetLayout, 2> layouts = {zpassDescriptorLayout, bindless_descriptor_layout};
  
  VkPipelineLayoutCreateInfo layout = vkinit::pipeline_layout_create_info();
  layout.setLayoutCount = layouts.size();
  layout.pSetLayouts = layouts.data();
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
    builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    builder.add_binding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    builder.add_binding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

    m_ShadowSetLayout = builder.build(device, VK_SHADER_STAGE_VERTEX_BIT);
  }

  

  VkPipelineLayoutCreateInfo layoutInfo = vkinit::pipeline_layout_create_info();
  layoutInfo.setLayoutCount = 1;
  layoutInfo.pSetLayouts = &m_ShadowSetLayout;
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

  // VulkanImGuiBackend::init(this);
  // return;
  
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


  VulkanImGuiBackend::init(this);


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


  
  // return;

  
  // ImGui_ImplVulkan_CreateFontsTexture();

  m_DeletionQueue.push_function([=, this]() {
    ImGui_ImplVulkan_Shutdown();
    vkDestroyDescriptorPool(device, imguiPool, nullptr);
  });

  // FIXME: temporary imgui color space fix, PR is on the way.
  // ImGuiStyle& style = ImGui::GetStyle();
  // for (int i = 0; i < ImGuiCol_COUNT; i++)
  // {
  //   ImVec4& colour = style.Colors[i];
  //   colour.x = powf(colour.x, 2.2f);
  //   colour.y = powf(colour.y, 2.2f);
  //   colour.z = powf(colour.z, 2.2f);
  //   colour.w = colour.w;
  // }
}


void Engine::init_indirect_cull_pipeline()
{
  VkPipelineLayoutCreateInfo computeLayout = vkinit::pipeline_layout_create_info();
  // computeLayout.pSetLayouts = &m_DrawDescriptorLayout;
  // computeLayout.setLayoutCount = 1;
  
  VkPushConstantRange pcs{};
  pcs.offset = 0;
  pcs.size = sizeof(indirect_cull_pcs);
  pcs.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  computeLayout.pPushConstantRanges = &pcs;
  computeLayout.pushConstantRangeCount = 1;

  VK_CHECK_RESULT(vkCreatePipelineLayout(device, &computeLayout, nullptr, &cullPipelineLayout));

  VkShaderModule cullShader;
  AR_LOG_ASSERT(
    vkutil::load_shader_module("shaders/culling/indirect_cull.comp.spv", device, &cullShader),
    "Error loading Gradient Compute Effect Shader"
  );  

  VkPipelineShaderStageCreateInfo stageInfo = vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_COMPUTE_BIT, cullShader);

  VkComputePipelineCreateInfo computePipelineCreateInfo{};
  computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  computePipelineCreateInfo.pNext = nullptr;
  computePipelineCreateInfo.layout = cullPipelineLayout;
  computePipelineCreateInfo.stage = stageInfo;
    

  VK_CHECK_RESULT(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &cullPipeline));


  vkDestroyShaderModule(device, cullShader, nullptr);

  m_DeletionQueue.push_function([&]() {
    vkDestroyPipelineLayout(device, cullPipelineLayout, nullptr);
    vkDestroyPipeline(device, cullPipeline, nullptr);
  });

}


void Engine::do_culling(VkCommandBuffer cmd)
{
 
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cullPipeline);
  // vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.layout, 0, 1, &m_DrawDescriptors, 0, nullptr);
  
  indirect_cull_pcs pcs;
  pcs.ids = (VkDeviceAddress) indirectDrawBuffer.info.pMappedData;
  pcs.draw_count = mainDrawContext.mem.size();



	VkBufferDeviceAddressInfo deviceAdressInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = indirectDrawBuffer.buffer };
	pcs.ids = (VkDeviceAddress) vkGetBufferDeviceAddress(device, &deviceAdressInfo);


	VkBufferDeviceAddressInfo deviceAdressInfo2{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = bigTransformBuffer.buffer };
	pcs.td = (VkDeviceAddress) vkGetBufferDeviceAddress(device, &deviceAdressInfo2);



	VkBufferDeviceAddressInfo deviceAdressInfo3{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = bigDrawDataBuffer.buffer };
	pcs.ddb = (VkDeviceAddress) vkGetBufferDeviceAddress(device, &deviceAdressInfo3);

  pcs.view = sceneData.view;


  // dont understand code just do it??

  glm::mat4 proj = sceneData.proj;
  glm::mat4 projT = glm::transpose(proj);


  auto normalise = [](glm::vec4 p){ return p / glm::length(glm::vec3(p)); };
  
  glm::vec4 frustumX = normalise(projT[3] + projT[0]);
  glm::vec4 frustumY = normalise(projT[3] + projT[1]);

  pcs.frustum = {frustumX.x, frustumX.z, frustumY.y, frustumY.z};

  
  // end


	
  vkCmdPushConstants(cmd, cullPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pcs), &pcs);

  vkCmdDispatch(cmd, std::ceil(mainDrawContext.mem.size() / 256.0), 1, 1);


  VkBufferMemoryBarrier2 mbar{.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2, .pNext = nullptr};
  mbar.buffer = indirectDrawBuffer.buffer;
  mbar.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
  mbar.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  mbar.dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
  mbar.dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
  mbar.size = VK_WHOLE_SIZE;
  
  VkDependencyInfo info{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .pNext = nullptr};
  info.bufferMemoryBarrierCount = 1;
  info.pBufferMemoryBarriers = &mbar;

  vkCmdPipelineBarrier2(cmd, &info); 
}


} // namespace aurora
