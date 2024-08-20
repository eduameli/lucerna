#pragma once
#include <vulkan/vulkan.h>
#include "aurora_pch.h"

namespace vkutil
{
// NOTE: std::filesystem::path and optional that retuns "placeholder" shader pipeline if it fails.
bool load_shader_module(const char* filepath, VkDevice device, VkShaderModule* outShaderModule);

} // namespace vkutil

class PipelineBuilder
{
  public:
    PipelineBuilder() { clear(); }
    VkPipeline build_pipeline(VkDevice device);
  public:
  private:
  private:
    std::vector<VkPipelineShaderStageCreateInfo> m_ShaderStages;
    VkPipelineInputAssemblyStateCreateInfo m_InputAssembly;
    VkPipelineRasterizationStateCreateInfo m_Rasterizer;
    VkPipelineColorBlendAttachmentState m_ColorBlendAttachment;
    VkPipelineMultisampleStateCreateInfo m_Multisampling;
    VkPipelineLayout m_PipelineLayout;
    VkPipelineDepthStencilStateCreateInfo m_DepthStencil;
    VkPipelineRenderingCreateInfo m_RenderInfo;
    VkFormat m_ColorAttachmentFormat;

    void clear();
};

