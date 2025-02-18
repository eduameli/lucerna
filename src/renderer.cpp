#include "renderer.h"
#include "engine.h"
#include "imgui_backend.h"
#include "input_structures.glsl"
#include "logger.h"
#include "vk_types.h"
#include "vulkan/vulkan_core.h"
#include <backends/imgui_impl_glfw.h>

namespace Lucerna {

void Renderer::draw(VkCommandBuffer cmd)
{
}

void Renderer::draw_editor(VkCommandBuffer cmd, VkImageView target)
{

  static Engine* engine = Engine::get();
  
  
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  static bool show_overdraw{ false }, show_ssao{ false }, show_collision{ false }, show_debug_lines{ false }, show_overlay{ true };
  // debug overlay!

  static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
  ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;

  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::SetNextWindowViewport(viewport->ID);

  window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
  window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;


  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::Begin("Lucerna Engine", nullptr, window_flags);

  
  ImGui::DockSpace(ImGui::GetID("Lucerna Engine"), ImVec2{0.0f, 0.0f},  dockspace_flags);



  ImGui::Begin("Viewport", NULL);
    ImGui::Image((ImTextureID) (uint64_t) Engine::get()->m_DrawImage.image_idx, ImGui::GetContentRegionAvail());

    if (show_overlay)
    {
      const uint32_t lwidth = 15;
      ImVec2 origin = ImGui::GetWindowPos();
      origin.y += ImGui::GetWindowHeight();
      origin.y -= lwidth*7;

      origin.x += 5;
      origin.y -= 5;
    
      ImDrawList* list = ImGui::GetForegroundDrawList();
      ImVec2 extent = ImGui::GetWindowSize();

      list->AddRectFilled({origin.x -5, origin.y -5}, {origin.x + 5 + lwidth*19, origin.y + lwidth*7 + 5}, IM_COL32(5, 45, 5, 135));
      list->AddText(origin, IM_COL32(255, 255, 255, 255), "lucerna-dev (pre-alpha)");
      list->AddText({origin.x, origin.y + lwidth*1}, IM_COL32(255, 255, 255, 255), std::format("[instance ver. {}]", engine->stats.instanceVersion).c_str());
      list->AddText({origin.x, origin.y + lwidth*2}, IM_COL32(255, 255, 255, 255), std::format("gpu: {}", engine->stats.gpuName).c_str());
      list->AddText({origin.x, origin.y + lwidth*3}, IM_COL32(255, 255, 255, 255), std::format("resolution: {}x{}", extent.x, extent.y).c_str());
      list->AddText({origin.x, origin.y + lwidth*4}, IM_COL32(255, 255, 255, 255), std::format("present mode: {}", vkutil::stringify_present_mode(engine->m_Swapchain.presentMode)).c_str());
      list->AddText({origin.x, origin.y + lwidth*5}, IM_COL32(255, 255, 255, 255), std::format("frame: {}", engine->frameNumber).c_str());
      list->AddText({origin.x, origin.y + lwidth*6}, IM_COL32(255, 255, 255, 255), std::format("opaque {} | transparent {}", engine->opaque_set.draw_datas.size(), engine->transparent_set.draw_datas.size()).c_str());
    }
  ImGui::End();
  
  ImGui::PopStyleVar(3);


  // FIXME: options dont do anything yet, just to get an idea of what functionality can be here.
 
  if (ImGui::BeginMenuBar())
  {
    ImGui::PushItemFlag(ImGuiItemFlags_AutoClosePopups, false);
    
    if (ImGui::BeginMenu("Settings"))
    {
      ImGui::Text("Reload Scene");
      ImGui::Text("Reload Renderer");
      ImGui::Text("Restore Default Settings");
      ImGui::Text("Open Config");

      ImGui::EndMenu();
    }
    
    if (ImGui::BeginMenu("Debug"))
    {
      ImGui::MenuItem("Display Overdraw", NULL, &show_overdraw);
      ImGui::MenuItem("Display SSAO", NULL, &show_ssao);
      ImGui::MenuItem("Show Overlay", NULL, &show_overlay);
      ImGui::MenuItem("Show Debug Lines", NULL, &show_debug_lines);
      ImGui::MenuItem("Show Collision Shapes", NULL, &show_collision);
      ImGui::EndMenu();
    }
    
    if (ImGui::BeginMenu("Scene"))
    {
      ImGui::Text("Reload Scene");
      ImGui::Text("Load Scene");
      ImGui::Text("Load GLTF");
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("About"))
    {
      ImGui::Text("lucerna v0.1.0");
      ImGui::Text("by eduameli");
      ImGui::SameLine();
      ImGui::TextLinkOpenURL("(repo)", "https://gitlab.com/eduameli/lucerna");
      
      ImGui::EndMenu();
    }
    
    ImGui::EndMenuBar();
    ImGui::PopItemFlag();
  }
  ImGui::End();






  // FIXME:  resizing is based on the glfw window not the Viewport - this needs to be fixed

  
  FrameGraph::render_graph();
  CVarSystem::get()->draw_editor();

  ImGui::Begin("Texture Picker");
    static int32_t texture_idx = 0;
    ImGui::Text("Bindless Texture Picker");
    ImGui::InputInt("texture idx", &texture_idx);

    float min = glm::min(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y);
    ImGui::Image((ImTextureID) (uint64_t) texture_idx, ImVec2{min, min});
  ImGui::End();

  ImGui::EndFrame();
  ImGui::Render();

  VulkanImGuiBackend::draw(cmd, Engine::get()->device, target, Engine::get()->m_Swapchain.extent2d);

}



void Renderer::cull_draw_set(VkCommandBuffer cmd, DrawSet& draw_set)
{
  if (draw_set.draw_datas.size() == 0)
    return;

  vklog::start_debug_label(cmd, draw_set.name.c_str(), MARKER_GREEN);
  
  DrawContext& mainDrawContext = Engine::get()->mainDrawContext;
  VkDevice device = Engine::get()->device;
  VkDescriptorSetLayout compact_descriptor_layout = Engine::get()->compact_descriptor_layout;
  GPUSceneData sceneData = Engine::get()->sceneData;
  VkPipelineLayout cullPipelineLayout = Engine::get()->cullPipelineLayout;
  VkPipeline compactPipeline = Engine::get()->compactPipeline;
  VkPipeline writeIndirectPipeline = Engine::get()->writeIndirectPipeline;


   vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, Engine::get()->cullPipeline);

  // this should be part of the mainDrawContext - not allocated every frame etc... its literally only updated once and then can just bind it when u change pipeline

  VkDescriptorSet cullDescriptor = Engine::get()->get_current_frame().frameDescriptors.allocate(device, compact_descriptor_layout);
  DescriptorWriter writer;
  writer.write_buffer(0, draw_set.buffers.draw_data.buffer, draw_set.draw_datas.size() * sizeof(DrawData), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER); // FIXME: .buffer .buffer :sob:
  writer.write_buffer(1, mainDrawContext.sceneBuffers.transformBuffer.buffer, mainDrawContext.transforms.size() * sizeof(glm::mat4x3), 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
  writer.write_buffer(2, mainDrawContext.sceneBuffers.boundsBuffer.buffer, mainDrawContext.sphere_bounds.size() * sizeof(glm::vec4) , 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
  writer.update_set(device, cullDescriptor);
  
  indirect_cull_pcs pcs;
  pcs.draw_count = draw_set.draw_datas.size();

	VkBufferDeviceAddressInfo deviceAdressInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = draw_set.buffers.indirect_draws.buffer };
	pcs.ids = (VkDeviceAddress) vkGetBufferDeviceAddress(device, &deviceAdressInfo);


  VkBufferDeviceAddressInfo indirectCountBuffer{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = draw_set.buffers.indirect_count.buffer };
  pcs.indirect_count = (VkDeviceAddress) vkGetBufferDeviceAddress(device, &indirectCountBuffer);

  VkBufferDeviceAddressInfo partialBuff{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = draw_set.buffers.partialSums.buffer };
  pcs.partial = (VkDeviceAddress) vkGetBufferDeviceAddress(device, &partialBuff);

  VkBufferDeviceAddressInfo outCull{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = draw_set.buffers.outputCompact.buffer };
  pcs.outb = (VkDeviceAddress) vkGetBufferDeviceAddress(device, &outCull);
  
  pcs.view = sceneData.view;


  // dont understand code just do it??
  glm::mat4 projT = glm::transpose(sceneData.proj);


  auto normalise = [](glm::vec4 p){ return p / glm::length(glm::vec3(p)); };
  
  glm::vec4 frustumX = normalise(projT[3] + projT[0]);
  glm::vec4 frustumY = normalise(projT[3] + projT[1]);

  pcs.frustum = {frustumX.x, frustumX.z, frustumY.y, frustumY.z};
  // end


  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cullPipelineLayout, 0, 1, &cullDescriptor, 0, nullptr);
	
  vkCmdPushConstants(cmd, cullPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pcs), &pcs);
  vkCmdDispatch(cmd, std::ceil(draw_set.draw_datas.size() / 1024.0), 1, 1);


  VkBufferMemoryBarrier2 mbar{.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2, .pNext = nullptr};
  mbar.buffer = draw_set.buffers.partialSums.buffer;
  mbar.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
  mbar.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  mbar.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
  mbar.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  mbar.size = VK_WHOLE_SIZE;
  
  VkDependencyInfo info{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .pNext = nullptr};
  info.bufferMemoryBarrierCount = 1;
  info.pBufferMemoryBarriers = &mbar;

  vkCmdPipelineBarrier2(cmd, &info);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compactPipeline);
  
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cullPipelineLayout, 0, 1, &cullDescriptor, 0, nullptr);
  vkCmdPushConstants(cmd, cullPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pcs), &pcs);
  vkCmdDispatch(cmd, std::ceil(draw_set.draw_datas.size() / 1024.0), 1, 1);

  {
    VkBufferMemoryBarrier2 mbar{.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2, .pNext = nullptr};
    mbar.buffer = draw_set.buffers.outputCompact.buffer;
    mbar.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    mbar.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    mbar.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    mbar.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    mbar.size = VK_WHOLE_SIZE;
  
    VkDependencyInfo info{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .pNext = nullptr};
    info.bufferMemoryBarrierCount = 1;
    info.pBufferMemoryBarriers = &mbar;

    vkCmdPipelineBarrier2(cmd, &info);
  }

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, writeIndirectPipeline);

  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, cullPipelineLayout, 0, 1, &cullDescriptor, 0, nullptr);
  vkCmdPushConstants(cmd, cullPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pcs), &pcs);
  vkCmdDispatch(cmd, std::ceil(draw_set.draw_datas.size() / 1024.0), 1, 1);

  
  {
    VkBufferMemoryBarrier2 mbar{.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2, .pNext = nullptr};
    mbar.buffer = draw_set.buffers.indirect_draws.buffer;
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


  vklog::end_debug_label(cmd);

}
  
} // namespace Lucerna
