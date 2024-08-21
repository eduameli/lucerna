#include "vk_pipelines.h"
#include <fstream>
#include "aurora_pch.h"
#include "vk_initialisers.h"

// FIX: return optional & use std::path?
bool vkutil::load_shader_module(const char *filepath, VkDevice device, VkShaderModule* outShaderModule)
{
  std::ifstream file(filepath, std::ios::ate | std::ios::binary);
  
  if (!file.is_open())
  {
    AR_CORE_ERROR("FAILED TO OPEN FILE!");
    return false;
  }

  size_t fileSize = (size_t) file.tellg();

  std::vector<uint32_t> buffer (fileSize / sizeof(uint32_t));

  file.seekg(0);

  file.read((char*) buffer.data(), fileSize);

  file.close();

  VkShaderModuleCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  info.pNext = nullptr;
  info.codeSize = buffer.size() * sizeof(uint32_t);
  info.pCode = buffer.data();

  VkShaderModule shaderModule;
  if (vkCreateShaderModule(device, &info, nullptr, &shaderModule) != VK_SUCCESS)
  {
    return false;
  }

  *outShaderModule = shaderModule;
  return true;
}

void PipelineBuilder::clear()
{
  m_InputAssembly = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
  m_Rasterizer = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
  m_Multisampling = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
  m_DepthStencil = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
  m_RenderInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
  
  m_ColorBlendAttachment = {};
  m_PipelineLayout = {};

  m_ShaderStages.clear();
}

VkPipeline PipelineBuilder::build_pipeline(VkDevice device)
{
  VkPipelineViewportStateCreateInfo viewportState{};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.pNext = nullptr;
  
  viewportState.viewportCount = 1;
  viewportState.scissorCount = 1;
  
  // FIXME: dummy color blending, !!!no blending!!!
  VkPipelineColorBlendStateCreateInfo colorBlending = {};
  colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.pNext = nullptr;

  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.logicOp = VK_LOGIC_OP_COPY;
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &m_ColorBlendAttachment;
   
  // (UNUSED), normally used for specifying vertex attribute format
  VkPipelineVertexInputStateCreateInfo vertexInputInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.pNext = &m_RenderInfo;

  pipelineInfo.stageCount = static_cast<uint32_t>(m_ShaderStages.size());
  pipelineInfo.pStages = m_ShaderStages.data();
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &m_InputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &m_Rasterizer;
  pipelineInfo.pMultisampleState = &m_Multisampling;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.pDepthStencilState = &m_DepthStencil;
  pipelineInfo.layout = m_PipelineLayout;

  VkDynamicState state[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
  VkPipelineDynamicStateCreateInfo dynamicInfo{};
  dynamicInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicInfo.dynamicStateCount = 2;
  dynamicInfo.pDynamicStates = &state[0];

  pipelineInfo.pDynamicState = &dynamicInfo;

  VkPipeline newPipeline;
  VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline));
  return newPipeline;
}

void PipelineBuilder::set_shaders(VkShaderModule vertexShader, VkShaderModule fragmentShader)
{
  m_ShaderStages.clear();
  m_ShaderStages.push_back(
    vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, vertexShader));
  m_ShaderStages.push_back(
    vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader));
}

void PipelineBuilder::set_input_topology(VkPrimitiveTopology topology)
{
  m_InputAssembly.topology = topology;
  // NOTE: used for triangle strips and line strips
  m_InputAssembly.primitiveRestartEnable = VK_FALSE;
}

void PipelineBuilder::set_cull_mode(VkCullModeFlags cullMode, VkFrontFace frontFace)
{
  m_Rasterizer.cullMode = cullMode;
  m_Rasterizer.frontFace = frontFace;
}

void PipelineBuilder::set_multisampling_none()
{
  m_Multisampling.sampleShadingEnable = VK_FALSE;
  m_Multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  m_Multisampling.minSampleShading = 1.0f;
  m_Multisampling.pSampleMask = nullptr;
  m_Multisampling.alphaToOneEnable = VK_FALSE;
  m_Multisampling.alphaToCoverageEnable = VK_FALSE;
}

void PipelineBuilder::disable_blending()
{
  m_ColorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  m_ColorBlendAttachment.blendEnable = VK_FALSE;
}

void PipelineBuilder::set_color_attachment_format(VkFormat format)
{
  m_ColorAttachmentFormat = format;
  m_RenderInfo.colorAttachmentCount = 1;
  m_RenderInfo.pColorAttachmentFormats = &m_ColorAttachmentFormat;
}

void PipelineBuilder::set_depth_format(VkFormat format)
{
  m_RenderInfo.depthAttachmentFormat = format;
}

void PipelineBuilder::disable_depthtest()
{
  m_DepthStencil.depthTestEnable = VK_FALSE;
  m_DepthStencil.depthWriteEnable = VK_FALSE;
  m_DepthStencil.depthCompareOp = VK_COMPARE_OP_NEVER;
  m_DepthStencil.depthBoundsTestEnable = VK_FALSE;
  m_DepthStencil.stencilTestEnable = VK_FALSE;
  m_DepthStencil.front = {};
  m_DepthStencil.back = {};
  m_DepthStencil.minDepthBounds = 0.0f;
  m_DepthStencil.maxDepthBounds = 1.0f;
}
