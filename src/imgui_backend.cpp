#include "imgui_backend.h"
#include "imgui_internal.h"
#include "la_asserts.h"
#include "input_structures.glsl"
#include "logger.h"
#include "vk_descriptors.h"
#include "vk_initialisers.h"
#include "vk_pipelines.h"
#include "vk_swapchain.h"
#include "vk_types.h"
#include "window.h"

#include <imgui.h>

#include <backends/imgui_impl_glfw.h>

namespace Lucerna {

static const uint32_t MAX_IDX_COUNT = 1000000;
static const uint32_t MAX_VTX_COUNT = 1000000;


static_assert(
  sizeof(ImDrawVert) == sizeof(ImVertexFormat),
  "ImDrawVert and ImGuiVertexFormat size mismatch");

static_assert(
  sizeof(ImDrawIdx) == sizeof(std::uint16_t) || sizeof(ImDrawIdx) == sizeof(uint32_t),
  "Only uint16_t or uint32_t indices are supported");

uint32_t to_bindless_idx(ImTextureID id)
{
  return static_cast<uint32_t>(id);
}

void VulkanImGuiBackend::init(Engine* engine)
{
  // create index buffer
  indexBuffer = engine->create_buffer(sizeof(ImDrawIdx) * MAX_IDX_COUNT, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

  // create vertex buffer
  vertexBuffer = engine->create_buffer(sizeof(ImVertexFormat) * MAX_VTX_COUNT, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

  vklog::label_buffer(engine->device, vertexBuffer.buffer, "ImGui Vertex Buffer");
  vklog::label_buffer(engine->device, indexBuffer.buffer, "ImGui Index Buffer");
  
  // upload imgui font
  auto& io = ImGui::GetIO();
  io.BackendRendererName = "Lucerna ImGui Backend";
  io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

  {
    auto* pixels = static_cast<std::uint8_t*>(nullptr);
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width , &height);
    fontImage = engine->create_image(
      pixels,
      {(uint32_t) width, (uint32_t) height, 1},
      VK_FORMAT_R8G8B8A8_UNORM,
      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
    io.Fonts->SetTexID((ImTextureID) (uint64_t) fontImage.texture_idx);
  }

  VkDevice device = engine->device;

  VkShaderModule frag, vert;
  LA_LOG_ASSERT(vkutil::load_shader_module("shaders/imgui/imgui_backend.frag.spv", device, &frag),
  "Failed to load imgui backend fragment shader!");
  LA_LOG_ASSERT(vkutil::load_shader_module("shaders/imgui/imgui_backend.vert.spv", device, &vert),
  "Failed to load imgui backend vertex shader!");

  VkPushConstantRange range{
    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
    .offset = 0,
    .size = sizeof(imgui_pcs)
  };

  
  VkDescriptorSetLayout sets[] = {engine->bindless_descriptor_layout};
  
  VkPipelineLayoutCreateInfo pipelineLayout = vkinit::pipeline_layout_create_info();
  pipelineLayout.pSetLayouts = &sets[0];
  pipelineLayout.pPushConstantRanges = &range;
  pipelineLayout.pushConstantRangeCount = 1;

  VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayout, nullptr, &pipLayout));
    
  {
    PipelineBuilder b;
    b.set_shaders(vert, frag);
    b.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    b.set_polygon_mode(VK_POLYGON_MODE_FILL);
    b.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    b.set_multisampling_none();
    
    b.m_ColorBlendAttachment.blendEnable = VK_TRUE;
    b.m_ColorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    b.m_ColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    b.m_ColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    b.m_ColorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    b.m_ColorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    b.m_ColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    b.m_ColorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    
    b.disable_depthtest();
    b.set_color_attachment_format(engine->m_Swapchain.format);
    b.PipelineLayout = engine->bindless_pipeline_layout;
    pipeline = b.build_pipeline(device);
  }

  vkDestroyShaderModule(device, vert, nullptr);
  vkDestroyShaderModule(device, frag, nullptr);

  
}

void VulkanImGuiBackend::draw(
  VkCommandBuffer cmd,
  VkDevice device,
  VkImageView swapchainImageView,
  VkExtent2D swapchainExtent
)
{
  Engine* engine = Engine::get();

  const ImDrawData* drawData = ImGui::GetDrawData();

  LA_ASSERT(drawData != nullptr);

  
  if (drawData->TotalVtxCount == 0)
  {
    return;
  }

  LA_LOG_ASSERT(drawData->TotalIdxCount < MAX_IDX_COUNT && drawData->TotalVtxCount < MAX_VTX_COUNT, 
  "Custom ImGuiBackend surpassed hardcoded upper limit for indices/vertices, buffer resize is not implemented yet!");
  
  copy_buffers(cmd, device);
  
  VkRenderingAttachmentInfo colourAttachemnt = vkinit::attachment_info(swapchainImageView, nullptr);
  VkRenderingInfo renderInfo = vkinit::rendering_info(swapchainExtent, &colourAttachemnt, nullptr);

  vkCmdBeginRendering(cmd, &renderInfo);

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);


  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, engine->bindless_pipeline_layout, 0, 1, &engine->global_descriptor_set, 0, nullptr);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, engine->bindless_pipeline_layout, 1, 1, &engine->bindless_descriptor_set, 0, nullptr);


  float targetWidth = (float) swapchainExtent.width;
  float targetHeight = (float) swapchainExtent.height;

  VkViewport viewport = {
    .x = 0,
    .y = 0,
    .width = targetWidth,
    .height = targetHeight,
    .minDepth = 0.0f,
    .maxDepth = 1.0f,
  };

  vkCmdSetViewport(cmd, 0, 1, &viewport);

  const auto clipOffset = drawData->DisplayPos;
  const auto clipScale = drawData->FramebufferScale;

  size_t globalIdxOffset = 0;
  size_t globalVtxOffset = 0;

  vkCmdBindIndexBuffer(cmd, indexBuffer.buffer, 0, sizeof(ImDrawIdx) == sizeof(uint16_t) ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);

  for (int cmdListID = 0; cmdListID < drawData->CmdListsCount; ++cmdListID)
  {
    const auto& cmdList = *drawData->CmdLists[cmdListID];
    for (int cmdID = 0; cmdID < cmdList.CmdBuffer.Size; ++cmdID)
    {
      const auto& imCmd = cmdList.CmdBuffer[cmdID];
      if (imCmd.UserCallback)
      {
        if (imCmd.UserCallback != ImDrawCallback_ResetRenderState)
        {
          imCmd.UserCallback(&cmdList, &imCmd);
          continue;
        }
        LA_LOG_ASSERT(false, "ImDrawCallback_RenderState is not supported");
      }

      if (imCmd.ElemCount == 0)
      {
        continue;
      }

      auto clipMin = ImVec2(
        (imCmd.ClipRect.x - clipOffset.x) * clipScale.x,
        (imCmd.ClipRect.y - clipOffset.y) * clipScale.y);

      auto clipMax = ImVec2(
        (imCmd.ClipRect.z - clipOffset.x) * clipScale.x,
        (imCmd.ClipRect.w - clipOffset.y) * clipScale.y);

      clipMin.x = std::clamp(clipMin.x, 0.0f, targetWidth);
      clipMax.x = std::clamp(clipMax.x, 0.0f, targetWidth);
      clipMin.y = std::clamp(clipMin.y, 0.0f, targetHeight);
      clipMax.y = std::clamp(clipMax.y, 0.0f, targetHeight);

      if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y)
      {
        continue;
      }

      uint32_t textureId = 4;
      if (imCmd.TextureId != ImTextureID())
      {
        textureId = to_bindless_idx(imCmd.TextureId);
      }
     

      const auto scale = glm::vec2(
        2.0f / drawData->DisplaySize.x,
        2.0f / drawData->DisplaySize.y);
      
      const auto translate = glm::vec2(
        -1.0f - drawData->DisplayPos.x * scale.x,
        -1.0f - drawData->DisplayPos.y * scale.y);

      const auto scissorX = (int32_t) clipMin.x;
      const auto scissorY = (int32_t) clipMin.y;
      const auto scissorWidth = (uint32_t) (clipMax.x - clipMin.x);
      const auto scissorHeight = (uint32_t) (clipMax.y - clipMin.y);

      VkRect2D scissor{
        .offset = {scissorX, scissorY},
        .extent = {scissorWidth, scissorHeight}
      };


      vkCmdSetScissor(cmd, 0, 1, &scissor);

      imgui_pcs pcs{};

      VkBufferDeviceAddressInfo vertexBDA {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = vertexBuffer.buffer
      };
  
      pcs.vertexBuffer= vkGetBufferDeviceAddress(device, &vertexBDA);
      pcs.textureID = textureId;
      pcs.scale = scale;
      pcs.translate = translate;

      vkCmdPushConstants(cmd, pipLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(imgui_pcs), &pcs);

      vkCmdDrawIndexed(cmd, 
        imCmd.ElemCount,
        1,
        imCmd.IdxOffset + globalIdxOffset,
        imCmd.VtxOffset + imCmd.VtxOffset + globalVtxOffset,
        0
      );
      
      
    }

    globalIdxOffset += cmdList.IdxBuffer.Size;
    globalVtxOffset += cmdList.VtxBuffer.Size;
  }

   
  vkCmdEndRendering(cmd);
}


void VulkanImGuiBackend::copy_buffers(VkCommandBuffer cmd, VkDevice device)
{
  Engine* engine = Engine::get();
  const ImDrawData* drawData = ImGui::GetDrawData();

  {
    VkBufferMemoryBarrier2 idxBufferBarrier{
      .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
      .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
      .srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
      .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
      .buffer = indexBuffer.buffer,
      .offset = 0,
      .size = VK_WHOLE_SIZE
    };
    VkBufferMemoryBarrier2 vtxBufferBarrier{
      .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
      .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
      .srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
      .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
      .buffer = vertexBuffer.buffer,
      .offset = 0,
      .size = VK_WHOLE_SIZE
    };

    VkBufferMemoryBarrier2 barriers[] = {idxBufferBarrier, vtxBufferBarrier};
    VkDependencyInfo info{
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .bufferMemoryBarrierCount = 2,
      .pBufferMemoryBarriers = &barriers[0]
    };

    vkCmdPipelineBarrier2(cmd, &info);
  }

  size_t currentIndexOffset = 0;
  size_t currentVertexoffset = 0;

  for (int i = 0; i < drawData->CmdListsCount; i++)
  {
    ImDrawList* cmdList = drawData->CmdLists[i];

    
    AllocatedBuffer staging = engine->create_buffer(cmdList->IdxBuffer.size_in_bytes() + cmdList->VtxBuffer.size_in_bytes(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    void* data = staging.info.pMappedData;

    memcpy(data, cmdList->VtxBuffer.begin(), cmdList->VtxBuffer.size_in_bytes());
    memcpy(((uint8_t*) data) + cmdList->VtxBuffer.size_in_bytes(), cmdList->IdxBuffer.begin() , cmdList->IdxBuffer.size_in_bytes());
    
    engine->immediate_submit([=](VkCommandBuffer cmd){
      VkBufferCopy2 idxRegion, vtxRegion;
      VkCopyBufferInfo2 idxCopy, vtxCopy;

      vtxRegion = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
        .srcOffset = 0,
        .dstOffset = currentVertexoffset * sizeof(ImVertexFormat),
        .size = (VkDeviceSize) cmdList->VtxBuffer.size_in_bytes(),
      };

      vtxCopy = {
        .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
        .pNext = nullptr,
        .srcBuffer = staging.buffer,
        .dstBuffer = vertexBuffer.buffer,
        .regionCount = 1,
        .pRegions = &vtxRegion,
      };

      vkCmdCopyBuffer2(cmd, &vtxCopy);

      idxRegion = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
        .srcOffset = (VkDeviceSize) cmdList->VtxBuffer.size_in_bytes(),
        .dstOffset = currentIndexOffset * sizeof(ImDrawIdx),
        .size = (VkDeviceSize) cmdList->IdxBuffer.size_in_bytes()
      };

      idxCopy = {
        .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
        .pNext = nullptr,
        .srcBuffer = staging.buffer,
        .dstBuffer = indexBuffer.buffer,
        .regionCount = 1,
        .pRegions = &idxRegion
      };

      vkCmdCopyBuffer2(cmd, &idxCopy);
    });


    currentIndexOffset += cmdList->IdxBuffer.size();
    currentVertexoffset += cmdList->VtxBuffer.size();
    
    engine->destroy_buffer(staging);

  }



  {
    VkBufferMemoryBarrier2 idxBufferBarrier{
      .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
      .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
      .srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
      .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
      .buffer = indexBuffer.buffer,
      .offset = 0,
      .size = VK_WHOLE_SIZE
    };
    VkBufferMemoryBarrier2 vtxBufferBarrier{
      .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
      .srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
      .srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
      .dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT,
      .buffer = vertexBuffer.buffer,
      .offset = 0,
      .size = VK_WHOLE_SIZE
    };

    VkBufferMemoryBarrier2 barriers[] = {idxBufferBarrier, vtxBufferBarrier};
    VkDependencyInfo info{
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .bufferMemoryBarrierCount = 2,
      .pBufferMemoryBarriers = &barriers[0]
    };

    vkCmdPipelineBarrier2(cmd, &info);

  }
}

void VulkanImGuiBackend::cleanup(Engine* engine)
{
  vkDestroyPipelineLayout(engine->device, pipLayout, nullptr);
  vkDestroyPipeline(engine->device, pipeline, nullptr);                                     
  engine->destroy_buffer(indexBuffer);
  engine->destroy_buffer(vertexBuffer);
  engine->destroy_image(fontImage);
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
    
  std::string str = std::format("avg ms:  {}", avg);
  
  ImGui::Begin("Frame Graph", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::PlotLines("##", [](void *data, int idx) -> float {
                    auto ft = static_cast<std::deque<float> *>(data);
                    return idx < ft->size() ? ft->at(idx) : 0.0f;
                }, &frametimes, frametimes.size(), 0, nullptr, avg - 8*sd, avg + 8*sd, ImVec2{190, 100});

    ImGui::Text("avg: %.3f", avg);
    ImGui::Text(" sd: %.3f\t cv: %.3f", sd, sd/avg);
    ImGui::Text("min: %.3f\tmax: %.3f", min, max);

    uint32_t* cnt = (uint32_t*) Engine::get()->opaque_set.buffers.indirect_count.info.pMappedData;
    ImGui::Text("indirect count: %i", *cnt);
    
  ImGui::End();
}



void FrameGraph::add_sample(float sample)
{
  frametimes.push_back(sample);
  if (frametimes.size() == 120) frametimes.pop_front();
}
} // namespace Lucerna
