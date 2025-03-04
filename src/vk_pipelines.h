#pragma once
#include "lucerna_pch.h"
#include <volk.h>
#include <vulkan/vulkan_core.h>

namespace Lucerna {

class PipelineBuilder
{
  public:
    PipelineBuilder() { clear(); }
    VkPipeline build_pipeline(VkDevice device);
    void set_shaders(VkShaderModule vertexShader, VkShaderModule fragmentShader);
    void set_input_topology(VkPrimitiveTopology topology);
    void set_polygon_mode(VkPolygonMode mode);
    void set_cull_mode(VkCullModeFlags cullMode, VkFrontFace);
    void set_multisampling_none();
    void disable_blending();
    void enable_blending_additive();
    void enable_blending_alphablend();
    void set_color_attachment_format(VkFormat format);
    void set_depth_format(VkFormat format);
    void disable_depthtest();
    void enable_depthtest(bool depthWriteEnable, VkCompareOp op);
  public:
    VkPipelineLayout PipelineLayout;
    VkPipelineColorBlendAttachmentState m_ColorBlendAttachment{};
  private:
  private:
    std::vector<VkPipelineShaderStageCreateInfo> m_ShaderStages{};
    VkPipelineInputAssemblyStateCreateInfo m_InputAssembly{};
    VkPipelineRasterizationStateCreateInfo m_Rasterizer{};
    // VkPipelineColorBlendAttachmentState m_ColorBlendAttachment{};
    VkPipelineMultisampleStateCreateInfo m_Multisampling{};
    VkPipelineDepthStencilStateCreateInfo m_DepthStencil{};
    VkPipelineRenderingCreateInfo m_RenderInfo{};
    VkFormat m_ColorAttachmentFormat{};

    void clear();
};


namespace vkutil
{
// NOTE: std::filesystem::path and optional that retuns "placeholder" shader pipeline if it fails.
bool load_shader_module(const char* filepath, VkDevice device, VkShaderModule* outShaderModule);

// SPIR-V REFLECTION
VkPipelineLayout get_pipeline_layout_from_spv();

} // namespace vkutil

}
