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
    void set_shaders(VkShaderModule vertexShader, VkShaderModule fragmentShader);
    void set_input_topology(VkPrimitiveTopology topology);
    void set_cull_mode(VkCullModeFlags cullMode, VkFrontFace);
    void set_multisampling_none();
    void disable_blending();
    void set_color_attachment_format(VkFormat format);
    void set_depth_format(VkFormat format);
    void disable_depthtest();
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

