#include "post_effects.h"
#include "engine.h"
#include "vk_initialisers.h"
#include "ar_asserts.h"
#include "vk_images.h"
#include "vk_pipelines.h"

namespace Aurora
{
void BloomEffect::prepare()
{
  // create pipelines layouts descriptors
  // create images
  
  Engine& engine = Engine::get();
  
  // create mip chain
  VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;
  VkExtent3D size = {engine.m_DrawExtent.width / 2, engine.m_DrawExtent.height / 2, 1};
  VkImageUsageFlags usages = VK_IMAGE_USAGE_SAMPLED_BIT;

  blurredMipmaps.imageFormat = format;
  blurredMipmaps.imageExtent = size;

  VkImageCreateInfo imgInfo = vkinit::image_create_info(format, usages, size);
  imgInfo.mipLevels = 5;

  VmaAllocationCreateInfo allocInfo{};
  allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
  allocInfo.requiredFlags = VkMemoryPropertyFlags{VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT};

  VK_CHECK_RESULT(vmaCreateImage(engine.m_Allocator, &imgInfo, &allocInfo, &blurredMipmaps.image, &blurredMipmaps.allocation, nullptr));
  VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;

  VkImageViewCreateInfo viewInfo = vkinit::imageview_create_info(format, blurredMipmaps.image, aspectFlags);
  viewInfo.subresourceRange.levelCount = imgInfo.mipLevels;

  VK_CHECK_RESULT(vkCreateImageView(engine.m_Device.logical, &viewInfo, nullptr, &blurredMipmaps.imageView));

  vkutil::set_debug_object_name(engine.m_Device.logical, blurredMipmaps.image, "Bloom Blur Mipmaps");
 
  // create descriptor layout // FIXME: could reuse? well im gonna use descriptor indexing eventually anyways so might as well
  // leave it like this until then
  // 1 image & pcs
  {
    DescriptorLayoutBuilder builder;
    builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    descriptorLayout = builder.build(engine.m_Device.logical, VK_SHADER_STAGE_COMPUTE_BIT);
  }


  // create compute pipeline
  VkPipelineLayoutCreateInfo computeLayout{ .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .pNext = nullptr};
  computeLayout.pSetLayouts = &descriptorLayout;
  computeLayout.setLayoutCount = 1;

  VkPushConstantRange pcs{};
  pcs.offset = 0;
  pcs.size = sizeof(BloomPushConstants);
  pcs.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  computeLayout.pPushConstantRanges = &pcs;
  computeLayout.pushConstantRangeCount = 1;

  VK_CHECK_RESULT(vkCreatePipelineLayout(engine.m_Device.logical, &computeLayout, nullptr, &pipelineLayout));

  AR_LOG_ASSERT(
    vkutil::load_shader_module("shaders/bloom/downsample.comp.spv", engine.m_Device.logical, &downsample),
    "Error loading (Bloom) Downsample Compute Effect Shader"
  );

  AR_LOG_ASSERT(
    vkutil::load_shader_module("shaders/bloom/upsample.comp.spv", engine.m_Device.logical, &upsample),
    "Error loading (Bloom) Upsample Compute Effect Shader"
  ); 
  
  VkPipelineShaderStageCreateInfo stageInfo = vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_COMPUTE_BIT, downsample);
  VkComputePipelineCreateInfo pipelineInfo{ .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, .pNext = nullptr};
  pipelineInfo.layout = pipelineLayout;
  pipelineInfo.stage = stageInfo;

  VK_CHECK_RESULT(vkCreateComputePipelines(engine.m_Device.logical, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &downsamplePipeline));
  
  pipelineInfo.stage.module = upsample;

  VK_CHECK_RESULT(vkCreateComputePipelines(engine.m_Device.logical, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &upsamplePipeline));
 
  descriptorSet = engine.globalDescriptorAllocator.allocate(engine.m_Device.logical, descriptorLayout);

  {
    DescriptorWriter writer;
    writer.write_image(0, engine.m_DrawImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    writer.update_set(engine.m_Device.logical, descriptorSet);
  }

  // memory not cleaned!
  // shader module / mipchain img / pipelines
}

void BloomEffect::run(VkCommandBuffer cmd, VkImageView targetImage)
{
  // run compute downsample
  // run compute upsample
  // mix with target img
  
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, downsamplePipeline);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

  BloomPushConstants pcs{};
  pcs.mipmap = 0.0;
  vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(BloomPushConstants), &pcs);
  
  // 1024 x 800 but it shoudl be dependant on what downsample it is..
  vkCmdDispatch(cmd, std::ceil(40), std::ceil(25), 1);

  /*
  ComputeEffect effect = m_BackgroundEffects[m_BackgroundEffectIndex]; 

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.layout, 0, 1, &m_DrawDescriptors, 0, nullptr);
  
  ComputePushConstants pc;
  vkCmdPushConstants(cmd, effect.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &effect.data);

  vkCmdDispatch(cmd, std::ceil(m_DrawExtent.width / 16.0), std::ceil(m_DrawExtent.height / 16.0), 1);
  */
}

void BloomEffect::cleanup()
{

}

} // aurora namespace
