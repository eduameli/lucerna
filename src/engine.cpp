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

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"

#include "ar_asserts.h"
#include "logger.h"
#include "vk_device.h"
#include "vk_swapchain.h"
#include <volk.h>

#include <glm/packing.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
// NOTE: needs to create instance ... contains device ... surface swapchain logic .. frame drawing

#include <glm/gtx/string_cast.hpp>
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
  
  init_default_data();
  mainCamera.init();
   
  std::string structurePath = "assets/shadows_demo.glb";
  auto structureFile = load_gltf(this, structurePath);

  AR_LOG_ASSERT(structureFile.has_value(), "structure.glb loaded correctly!");

  loadedScenes["structure"] = *structureFile;

} 


void Engine::shutdown()
{
  vkDeviceWaitIdle(m_Device.logical);
  s_Instance = nullptr;
  
  loadedScenes.clear();

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
 
  vkDestroyDescriptorSetLayout(m_Device.logical, m_SceneDescriptorLayout, nullptr);

  m_DeletionQueue.flush();
  
  vkDestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);

  vkDestroyDevice(m_Device.logical, nullptr);
  vkDestroyInstance(m_Instance, nullptr);
}

bool Engine::should_quit()
{
  return !glfwWindowShouldClose(Window::get()) || glfwGetKey(Window::get(), GLFW_KEY_ESCAPE);
}

void Engine::run()
{
  while (should_quit())
  {
    auto start = std::chrono::system_clock::now();
    glfwPollEvents();

    if (stopRendering)
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
      continue;
    }

    if (resizeRequested)
    {
      resize_swapchain();
    }
    
    update_scene();

    draw();
    
    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    stats.frametime = elapsed.count() / 1000.0f;
  }
}

void Engine::resize_swapchain()
{
  // FIX: build from previous swapchain
  vkDeviceWaitIdle(m_Device.logical);

  for (int i = 0; i < m_Swapchain.views.size(); i++)
  {
    vkDestroyImageView(m_Device.logical, m_Swapchain.views[i], nullptr);
  }

  SwapchainContextBuilder builder{m_Device, m_Surface};
  SwapchainContext newSwapchain = builder
    .set_preferred_format(VkFormat::VK_FORMAT_B8G8R8A8_SRGB)
    .set_preferred_colorspace(VkColorSpaceKHR::VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
    .set_preferred_present(VkPresentModeKHR::VK_PRESENT_MODE_MAILBOX_KHR)
    .build(m_Swapchain.handle);
  
  vkDestroySwapchainKHR(m_Device.logical, m_Swapchain.handle, nullptr);
  m_Swapchain = newSwapchain;
  resizeRequested = false;
}

void Engine::update_scene()
{

  auto start = std::chrono::system_clock::now();

  //mainDrawContext.OpaqueSurfaces.clear();
  //mainDrawContext.TransparentSurfaces.clear();

  mainCamera.update();
  glm::mat4 view = mainCamera.get_view_matrix();
  glm::mat4 projection = glm::perspective(glm::radians(70.0f), (float) m_DrawExtent.width / (float) m_DrawExtent.height, 10000.0f, 0.1f);
  
  projection[1][1] *= -1;

  sceneData.view = view;
  sceneData.proj = projection;
  sceneData.viewproj = projection * view;

  if (lightView)
  {
    sceneData.viewproj = lightProj * lView;
    sceneData.view = lView;
    sceneData.proj = lightProj;
  }
  
	sceneData.ambientColour = glm::vec4(0.1f);
	sceneData.sunlightColour = glm::vec4(1.0f);
	sceneData.sunlightDirection = glm::vec4(0.0f, 1.0f, 0.0f, 0.01f);
  

  loadedScenes["structure"]->queue_draw(glm::mat4{1.0f}, mainDrawContext);
   
  auto end = std::chrono::system_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  stats.scene_update_time = elapsed.count() / 1000.0f;
}





void Engine::draw()
{
  VK_CHECK_RESULT(vkWaitForFences(m_Device.logical, 1, &get_current_frame().renderFence, true, 1000000000));
  
  get_current_frame().deletionQueue.flush();
  get_current_frame().frameDescriptors.clear_pools(m_Device.logical);

  uint32_t swapchainImageIndex;
  VkResult e = vkAcquireNextImageKHR(m_Device.logical, m_Swapchain.handle, 1000000000, get_current_frame().swapchainSemaphore, nullptr, &swapchainImageIndex);
  if (e == VK_ERROR_OUT_OF_DATE_KHR)
  {
    resizeRequested = true;
    return;
  }

  m_DrawExtent.height = glm::min(m_Swapchain.extent2d.height, m_DrawImage.imageExtent.height) * m_RenderScale;
  m_DrawExtent.width = glm::min(m_Swapchain.extent2d.width, m_DrawImage.imageExtent.width) * m_RenderScale;

  VK_CHECK_RESULT(vkResetFences(m_Device.logical, 1, &get_current_frame().renderFence));

  VkCommandBuffer cmd = get_current_frame().mainCommandBuffer;
  VK_CHECK_RESULT(vkResetCommandBuffer(cmd, 0));
  
  VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
  VK_CHECK_RESULT(vkBeginCommandBuffer(cmd, &cmdBeginInfo))

  vkutil::transition_image(cmd, m_DrawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

  // draw compute
  draw_background(cmd);
  
  vkutil::transition_image(cmd, m_ShadowDepthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
  draw_shadow_pass(cmd);
  vkutil::transition_image(cmd, m_ShadowDepthImage.image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL);

  vkutil::transition_image(cmd, m_DrawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  vkutil::transition_image(cmd, m_DepthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
  
  draw_geometry(cmd);
  
  vkutil::transition_image(cmd, m_DrawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  vkutil::transition_image(cmd, m_Swapchain.images[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  
  vkutil::copy_image_to_image(cmd, m_DrawImage.image, m_Swapchain.images[swapchainImageIndex], m_DrawExtent, m_Swapchain.extent2d);
  
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
 
  
  VkPresentInfoKHR presentInfo = vkinit::present_info();
  presentInfo.pSwapchains = &m_Swapchain.handle;
  presentInfo.swapchainCount = 1;
  presentInfo.pWaitSemaphores = &get_current_frame().renderSemaphore;
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pImageIndices = &swapchainImageIndex;

  VkResult presentResult = vkQueuePresentKHR(m_Device.present, &presentInfo);
  if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR)
  {
    resizeRequested = true;
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

  vkCmdDispatch(cmd, std::ceil(m_DrawExtent.width / 16.0), std::ceil(m_DrawExtent.height / 16.0), 1);
}

// FIXME: vertex buffer should be separated for better cache when doing shadow pass
void Engine::draw_shadow_pass(VkCommandBuffer cmd)
{
  std::vector<uint32_t> opaque_draws;
  opaque_draws.reserve(mainDrawContext.OpaqueSurfaces.size());
  
  for (uint32_t i = 0; i < mainDrawContext.OpaqueSurfaces.size(); i++)
  {
    if (is_visible(mainDrawContext.OpaqueSurfaces[i], sceneData.viewproj))
    {
      opaque_draws.push_back(i);
    }
  }
  
  VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(m_ShadowDepthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
  VkRenderingInfo renderInfo = vkinit::rendering_info({m_ShadowExtent.width, m_ShadowExtent.height}, nullptr, &depthAttachment);
  vkCmdBeginRendering(cmd, &renderInfo);
  
  // update sceneData to have light pos  
  // pcs has model matrix, mat4 in ubo? :sob:
  // filtering is done when shading? so do u rlly need anything else? (idk).
  
  struct ShadowPassUBO
  {
    glm::mat4 viewproj;
  } data;

  
  lightProj = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 20.0f, 0.1f); //FIXME: arbritary znear zfar planes
  lightProj[1][1] *= -1;  //FIXME: do i need this??
  
  float x_value = glm::sin(frameNumber * spinSpeed)* 5.0;
  float z_value = glm::cos(frameNumber * spinSpeed)* 5.0;

  lView = glm::lookAt(
    glm::vec3{x_value, 5.0f, z_value},
    glm::vec3{0.0f, 0.0f, 0.0f},
    glm::vec3{0.0f, 1.0f, 0.0f}
  );

  sceneData.sunlightDirection = glm::vec4{x_value, 1.0f, z_value, 1.0f}; // .w for sun power
  

  data.viewproj = lightProj * lView; 
  sceneData.lightViewProj = lightProj * lView;
  
  
  AllocatedBuffer shadowPass = create_buffer(sizeof(ShadowPassUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
  get_current_frame().deletionQueue.push_function([=, this] {
    destroy_buffer(shadowPass);
  });

  ShadowPassUBO* shadowPassUniform = (ShadowPassUBO*) shadowPass.allocation->GetMappedData();
  *shadowPassUniform = data;

  VkDescriptorSet shadowDescriptor = get_current_frame().frameDescriptors.allocate(m_Device.logical, m_ShadowSetLayout);
  DescriptorWriter writer;
  writer.write_buffer(0, shadowPass.buffer, sizeof(ShadowPassUBO), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  writer.update_set(m_Device.logical, shadowDescriptor);
  

  // use m_ShadowPipeline
  VkViewport viewport{};
  viewport.x = 0;
  viewport.y = 0;
  viewport.width = m_ShadowExtent.width;
  viewport.height = m_ShadowExtent.height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset.x = 0;
  scissor.offset.y = 0;
  scissor.extent.width = m_ShadowExtent.width;
  scissor.extent.height = m_ShadowExtent.height;
  vkCmdSetScissor(cmd , 0, 1, &scissor);
  
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ShadowPipeline);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ShadowPipelineLayout, 0, 1, &shadowDescriptor, 0, nullptr);
  
  VkBuffer lastIndexBuffer = VK_NULL_HANDLE;
  auto draw = [&](const RenderObject& draw) {
    if (draw.indexBuffer != lastIndexBuffer)
    {
      lastIndexBuffer = draw.indexBuffer;
      vkCmdBindIndexBuffer(cmd, draw.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    }

    GPUDrawPushConstants pcs{};
    pcs.worldMatrix = draw.transform; // worldMatrix == modelMatrix
    pcs.vertexBuffer = draw.vertexBufferAddress;

    vkCmdPushConstants(cmd, draw.material->pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &pcs);
    vkCmdDrawIndexed(cmd, draw.indexCount, 1, draw.firstIndex, 0, 0);

  };
  

  for (auto& r : opaque_draws)
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

  std::vector<uint32_t> opaque_draws;
  opaque_draws.reserve(mainDrawContext.OpaqueSurfaces.size());
  
  for (uint32_t i = 0; i < mainDrawContext.OpaqueSurfaces.size(); i++)
  {
    if (is_visible(mainDrawContext.OpaqueSurfaces[i], sceneData.viewproj))
    {
      opaque_draws.push_back(i);
    }

  }
   
  // FIXME: Another way of doing this is that we would calculate a sort key , and then our opaque_draws would be something like 20 bits draw index,
  // and 44 bits for sort key/hash. That way would be faster than this as it can be sorted through faster methods.
  std::sort(opaque_draws.begin(), opaque_draws.end(), [&](const auto& iA, const auto& iB) {
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


  VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(m_DrawImage.imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
  VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(m_DepthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
  VkRenderingInfo renderInfo = vkinit::rendering_info(m_DrawExtent, &colorAttachment, &depthAttachment);
  vkCmdBeginRendering(cmd, &renderInfo);
  
  // descriptor set for scene data created every frame! temporal per frame data - skinned mesh? (UBO)
  
  AllocatedBuffer gpuSceneDataBuffer = create_buffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
  get_current_frame().deletionQueue.push_function([=, this] {
    destroy_buffer(gpuSceneDataBuffer);
  });

  GPUSceneData* sceneUniformData = (GPUSceneData*) gpuSceneDataBuffer.allocation->GetMappedData();
  *sceneUniformData = sceneData;
  
  VkDescriptorSet globalDescriptor = get_current_frame().frameDescriptors.allocate(m_Device.logical, m_SceneDescriptorLayout);
  DescriptorWriter writer;
  writer.write_buffer(0, gpuSceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  writer.write_image(1, m_ShadowDepthImage.imageView, m_DefaultSamplerNearest, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  writer.update_set(m_Device.logical, globalDescriptor);

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
        vkCmdSetScissor(cmd , 0, 1, &scissor);
      }
      
      vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.material->pipeline->layout, 1, 1, &draw.material->materialSet, 0, nullptr);

    }

    if (draw.indexBuffer != lastIndexBuffer)
    {
      lastIndexBuffer = draw.indexBuffer;
      vkCmdBindIndexBuffer(cmd, draw.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    }

    GPUDrawPushConstants pcs{};
    pcs.worldMatrix = draw.transform;
    pcs.vertexBuffer = draw.vertexBufferAddress;
    // world matrix is the model matrix??

    vkCmdPushConstants(cmd, draw.material->pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &pcs);

    vkCmdDrawIndexed(cmd, draw.indexCount, 1, draw.firstIndex, 0, 0);

    stats.drawcall_count++;
    stats.triangle_count += draw.indexCount / 3;
  };

  for (auto& r : opaque_draws)
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
    ImGui::SliderFloat("Render Scale", &m_RenderScale, 0.3f, 1.0f);
  }

  ImGui::Begin("Stats");
    ImGui::Text("frametime %f ms", stats.frametime);
    ImGui::Text("draw time %f ms", stats.mesh_draw_time);
    ImGui::Text("update time %f ms", stats.scene_update_time);
    ImGui::Text("triangles %i", stats.triangle_count);
    ImGui::Text("draws %i", stats.drawcall_count);
  ImGui::End();

  ImGui::Begin("Shadow Mapping Settings");
    ImGui::Checkbox("Follow Sun", &lightView);
    ImGui::SliderFloat("Spin Speed %.6f", &spinSpeed, 0.0f, 0.01f);
  ImGui::End();
  

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
  return true;
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

  VK_CHECK_RESULT(vkCreateImageView(m_Device.logical, &viewInfo, nullptr, &newImage.imageView));

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

void Engine::destroy_image(const AllocatedImage& img)
{
  vkDestroyImageView(m_Device.logical, img.imageView, nullptr);
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
  VkSamplerCreateInfo sampl{};
  sampl.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampl.magFilter = VK_FILTER_NEAREST;
  sampl.minFilter = VK_FILTER_NEAREST;
  vkCreateSampler(m_Device.logical, &sampl, nullptr, &m_DefaultSamplerNearest);

  sampl.magFilter = VK_FILTER_LINEAR;
  sampl.minFilter = VK_FILTER_LINEAR;

  vkCreateSampler(m_Device.logical, &sampl, nullptr, &m_DefaultSamplerLinear);



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

  defaultData = metalRoughMaterial.write_material(m_Device.logical, MaterialPass::MainColour, materialResources, globalDescriptorAllocator);




  m_DeletionQueue.push_function([&]{
    vkDestroySampler(m_Device.logical, m_DefaultSamplerLinear, nullptr);
    vkDestroySampler(m_Device.logical, m_DefaultSamplerNearest, nullptr);
   
    // FIXME: metalRoughnessMaterial.clear_resources() instead!
    
    metalRoughMaterial.clear_resources(m_Device.logical);

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
  }

  glfwCreateWindowSurface(m_Instance, Window::get(), nullptr, &m_Surface);
  
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
  
  VkValidationFeatureEnableEXT vfeat = VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT;
  VkValidationFeaturesEXT feats{ .sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT, .pNext = nullptr};
  feats.enabledValidationFeatureCount = 1;
  feats.pEnabledValidationFeatures = &vfeat;

  if (m_UseValidationLayers)
  {
    instance.enabledLayerCount = static_cast<uint32_t>(m_ValidationLayers.size());
    instance.ppEnabledLayerNames = m_ValidationLayers.data();
    instance.pNext = &feats;

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
}

void Engine::init_swapchain()
{
  SwapchainContextBuilder builder{m_Device, m_Surface};
  m_Swapchain = builder
    .set_preferred_format(VkFormat::VK_FORMAT_B8G8R8A8_SRGB)
    .set_preferred_colorspace(VkColorSpaceKHR::VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
    .set_preferred_present(VkPresentModeKHR::VK_PRESENT_MODE_MAILBOX_KHR)
    .build();
    
  AR_CORE_INFO("Using {}", vkutil::stringify_present_mode(m_Swapchain.presentMode));
  
  //FIXME: why swapchain? it should be based on the native res 
  VkExtent3D drawImageExtent = {m_Swapchain.extent2d.width, m_Swapchain.extent2d.height, 1}; 
  m_DrawExtent = {drawImageExtent.width, drawImageExtent.height};
	VkImageUsageFlags drawImageUsages{};
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
	drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  VkImageUsageFlags depthImageUsages{};
  depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  depthImageUsages |= VK_IMAGE_USAGE_SAMPLED_BIT;
  
  m_DrawImage = create_image(drawImageExtent, VK_FORMAT_R16G16B16A16_SFLOAT, drawImageUsages, false);
  m_DepthImage = create_image(drawImageExtent, VK_FORMAT_D32_SFLOAT, depthImageUsages, false);
  m_ShadowDepthImage = create_image(m_ShadowExtent, VK_FORMAT_D32_SFLOAT, depthImageUsages, false);
  
  vkutil::set_debug_object_name(m_Device.logical, m_DrawImage.image, "draw image");
  vkutil::set_debug_object_name(m_Device.logical, m_DepthImage.image, "depth image");
  vkutil::set_debug_object_name(m_Device.logical, m_ShadowDepthImage.image, "shadow mapping depth img");

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
  globalDescriptorAllocator.init(m_Device.logical, 10, sizes);

  // compute background descriptor layout? 
  {
    DescriptorLayoutBuilder builder;
    builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    m_DrawDescriptorLayout = builder.build(m_Device.logical, VK_SHADER_STAGE_COMPUTE_BIT);
  }
  m_DrawDescriptors = globalDescriptorAllocator.allocate(m_Device.logical, m_DrawDescriptorLayout);
  
  // scene data descriptor layout
  {
    DescriptorLayoutBuilder builder;
    builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    builder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    m_SceneDescriptorLayout = builder.build(m_Device.logical, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT); 
  } 
  
  {
    DescriptorLayoutBuilder builder;
    builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    m_SingleImageDescriptorLayout = builder.build(m_Device.logical, VK_SHADER_STAGE_FRAGMENT_BIT);
  }

  // links the draw image to the descriptor for the compute shader in the pipeline!
  DescriptorWriter writer;
  writer.write_image(0, m_DrawImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
  writer.update_set(m_Device.logical, m_DrawDescriptors);
  
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
    m_Frames[i].frameDescriptors.init(m_Device.logical, 1000, frameSizes);
    
    m_DeletionQueue.push_function([&, i]() {
      m_Frames[i].frameDescriptors.destroy_pools(m_Device.logical);
    });

  }

  m_DeletionQueue.push_function([&]() {
    globalDescriptorAllocator.destroy_pools(m_Device.logical);
    vkDestroyDescriptorSetLayout(m_Device.logical, m_DrawDescriptorLayout, nullptr);
    vkDestroyDescriptorSetLayout(m_Device.logical, m_SingleImageDescriptorLayout, nullptr);
  });

}

void Engine::init_pipelines()
{
  init_background_pipelines();
  metalRoughMaterial.build_pipelines(this);

  // shadow pipelines
  VkDevice device = m_Device.logical;

  VkShaderModule shadowFrag;
  AR_LOG_ASSERT(
    vkutil::load_shader_module("shaders/shadow_pass/shadow.frag.spv", device, &shadowFrag),
    "Error when building the shadow pass fragment shader module"
  );

  VkShaderModule shadowVert;
  AR_LOG_ASSERT(
    vkutil::load_shader_module("shaders/shadow_pass/shadow.vert.spv", device, &shadowVert),
    "Error when building the shadow pass vertex shader module"
  );

  { 
    DescriptorLayoutBuilder builder;
    builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    m_ShadowSetLayout = builder.build(m_Device.logical, VK_SHADER_STAGE_VERTEX_BIT);
  }


  VkPushConstantRange range{};
  range.offset = 0;
  range.size = sizeof(GPUDrawPushConstants);
  range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  
  VkPipelineLayoutCreateInfo layoutInfo = vkinit::pipeline_layout_create_info();
  layoutInfo.setLayoutCount = 1;
  layoutInfo.pSetLayouts = &m_ShadowSetLayout;
  layoutInfo.pPushConstantRanges = &range;
  layoutInfo.pushConstantRangeCount = 1;

  VK_CHECK_RESULT(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &m_ShadowPipelineLayout));

  PipelineBuilder builder;
  builder.set_shaders(shadowVert, shadowFrag);
  builder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
  builder.set_polygon_mode(VK_POLYGON_MODE_FILL);
  builder.set_cull_mode(VK_CULL_MODE_FRONT_BIT, VK_FRONT_FACE_CLOCKWISE);
  builder.set_multisampling_none();
  builder.disable_blending();
  builder.enable_depthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
  builder.set_depth_format(VK_FORMAT_D32_SFLOAT);
  
  builder.PipelineLayout = m_ShadowPipelineLayout;

  m_ShadowPipeline = builder.build_pipeline(device);
  
  vkDestroyShaderModule(device, shadowFrag, nullptr);
  vkDestroyShaderModule(device, shadowVert, nullptr);

  m_DeletionQueue.push_function([&](){
    vkDestroyDescriptorSetLayout(m_Device.logical, m_ShadowSetLayout, nullptr);
    vkDestroyPipelineLayout(m_Device.logical, m_ShadowPipelineLayout, nullptr);
    vkDestroyPipeline(m_Device.logical, m_ShadowPipeline, nullptr);
  });
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

  VkShaderModule gradientShader;
  AR_LOG_ASSERT(
    vkutil::load_shader_module("shaders/push_gradient.spv", m_Device.logical, &gradientShader),
    "Error loading Gradient Compute Effect Shader"
  );  

  VkShaderModule skyShader;
  AR_LOG_ASSERT(
    vkutil::load_shader_module("shaders/sky_shader.spv", m_Device.logical, &skyShader),
    "Error loading Sky Compute Effect Shader"
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

  vkDestroyShaderModule(m_Device.logical, gradientShader, nullptr);
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
  ImGui_ImplGlfw_InitForVulkan(Window::get(), true);
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

} // namespace aurora
