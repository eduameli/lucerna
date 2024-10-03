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
  VkImageUsageFlags usages{};
  usages |= VK_IMAGE_USAGE_STORAGE_BIT;
  usages |= VK_IMAGE_USAGE_SAMPLED_BIT;

  for (uint32_t i = 0; i < 6; i++)
  {
    VkExtent3D mipSize = 
    {
      static_cast<uint32_t>(size.width /  glm::exp2(i)),
      static_cast<uint32_t>(size.height / glm::exp2(i)), 
      1
    };
    AllocatedImage img = engine.create_image(mipSize, format, usages);
    std::string identifier = "Bloom Mipmap Level" + std::to_string(i); 
    vkutil::set_debug_object_name(engine.m_Device.logical, img.image, identifier.c_str());
    blurredMips.push_back(img);
  }
    
  // FIXME: kind of ass having to do one full immediate submit just to transfer an img
  engine.immediate_submit([=](VkCommandBuffer cmd){
    for (auto img : blurredMips)
    {
      vkutil::transition_image(cmd, img.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    }
  });


  // create descriptor layout // FIXME: could reuse? well im gonna use descriptor indexing eventually anyways so might as well
  // leave it like this until then
  // 1 image & pcs
  {
    DescriptorLayoutBuilder builder;
    builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
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
  
  VkSamplerCreateInfo sampl{.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr};
  sampl.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  sampl.minLod = 0.0f;
  sampl.maxLod = 0.0f;
  sampl.mipLodBias = 0.0f;


  sampl.magFilter = VK_FILTER_LINEAR;
  sampl.minFilter = VK_FILTER_LINEAR;
  sampl.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampl.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampl.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampl.unnormalizedCoordinates = VK_FALSE;

  vkCreateSampler(engine.m_Device.logical, &sampl, nullptr, &sampler);
  
  vkDestroyShaderModule(engine.m_Device.logical, downsample, nullptr);
  vkDestroyShaderModule(engine.m_Device.logical, upsample, nullptr);
}

void BloomEffect::run(VkCommandBuffer cmd, VkImageView targetImage)
{
  // run compute downsample
  // run compute upsample
  // mix with target img

  Engine& engine = Engine::get();
  VkExtent3D size = {engine.internalExtent.width, engine.internalExtent.height, 1};

  BloomPushConstants pcs{};
  pcs.srcResolution = {size.width, size.height};
  pcs.filterRadius = 0.005;
  pcs.finalPass = 0;
  
  // draw img to mip0
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, downsamplePipeline);

  VkDescriptorSet firstSet = engine.get_current_frame().frameDescriptors.allocate(engine.m_Device.logical, descriptorLayout);
  {
    DescriptorWriter writer;
    writer.write_image(0, engine.m_DrawImage.imageView, sampler, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    writer.write_image(1, blurredMips[0].imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    writer.update_set(engine.m_Device.logical, firstSet);
  }
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &firstSet, 0, nullptr);

  vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(BloomPushConstants), &pcs);
  size.width *= 0.5;
  size.height *= 0.5;
  vkCmdDispatch(cmd, std::ceil(size.width / 16.0), std::ceil(size.height / 16.0), 1);

  VkImageMemoryBarrier2 imgBarrier{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2, .pNext = nullptr};
  imgBarrier.srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
  imgBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
  imgBarrier.dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
  imgBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
  imgBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
  imgBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
  imgBarrier.image = blurredMips[0].image;
  imgBarrier.subresourceRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
  
  VkDependencyInfo depInfo{ .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .pNext = nullptr};
  depInfo.imageMemoryBarrierCount = 1;
  depInfo.pImageMemoryBarriers = &imgBarrier;

  vkCmdPipelineBarrier2(cmd, &depInfo);

  // mip to mip downsample
   
  for (uint32_t i = 1; i < 6; i++)
  {
    pcs.srcResolution = {size.width, size.height};


    //FIXME : per frame descriptor set allocs not freed :/
    VkDescriptorSet set = engine.get_current_frame().frameDescriptors.allocate(engine.m_Device.logical, descriptorLayout);
    {
      DescriptorWriter writer;
      writer.write_image(0, blurredMips[i - 1].imageView, sampler, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
      writer.write_image(1, blurredMips[i].imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
      writer.update_set(engine.m_Device.logical, set);
    }
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &set, 0, nullptr);

    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(BloomPushConstants), &pcs);
    size.width *= 0.5;
    size.height *= 0.5;
    vkCmdDispatch(cmd, std::ceil(size.width / 16.0), std::ceil(size.height / 16.0), 1);
    

    VkImageMemoryBarrier2 imgBarrier{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2, .pNext = nullptr};
    imgBarrier.srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    imgBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    imgBarrier.dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    imgBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    imgBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    imgBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    imgBarrier.image = blurredMips[i].image;
    imgBarrier.subresourceRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
    
    VkDependencyInfo depInfo{ .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .pNext = nullptr};
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &imgBarrier;

    vkCmdPipelineBarrier2(cmd, &depInfo);

    // barrier until compute dispatch is finished
  }
  
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, upsamplePipeline); 
  
  for (uint32_t i = 5; i > 0; i--)
  {
    VkDescriptorSet set = engine.get_current_frame().frameDescriptors.allocate(engine.m_Device.logical, descriptorLayout);
    {
      DescriptorWriter writer;
      writer.write_image(0, blurredMips[i].imageView, sampler, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
      writer.write_image(1, blurredMips[i-1].imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
      writer.update_set(engine.m_Device.logical, set);
    }
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &set, 0, nullptr);
    
    pcs.srcResolution = {size.width, size.height};
    size = 
    {
      size.width * 2,
      size.height * 2, 
      1
    };
    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(BloomPushConstants), &pcs);
    vkCmdDispatch(cmd, std::ceil(size.width / 16.0), std::ceil(size.height / 16.0), 1);

    VkImageMemoryBarrier2 imgBarrier{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2, .pNext = nullptr};
    imgBarrier.srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    imgBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    imgBarrier.dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    imgBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    imgBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    imgBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    imgBarrier.image = blurredMips[i-1].image;
    imgBarrier.subresourceRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
    
    VkDependencyInfo depInfo{ .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .pNext = nullptr};
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &imgBarrier;

    vkCmdPipelineBarrier2(cmd, &depInfo);
  }
  
  
  
  // mip0 to draw texture 
  pcs.finalPass = 1; 

  VkDescriptorSet set = engine.get_current_frame().frameDescriptors.allocate(engine.m_Device.logical, descriptorLayout);
  {
    DescriptorWriter writer;
    writer.write_image(0, blurredMips[0].imageView, sampler, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    writer.write_image(1, engine.m_DrawImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    writer.update_set(engine.m_Device.logical, set);
  }
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &set, 0, nullptr);
  vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(BloomPushConstants), &pcs);
  
  pcs.srcResolution = {size.width, size.height};
  size = 
  {
    size.width * 2,
    size.height * 2
  };
  
  vkCmdDispatch(cmd, std::ceil(size.width / 16.0), std::ceil(size.height / 16.0), 1);
}

void BloomEffect::cleanup(Engine* engine)
{
  for (auto img : blurredMips)
  {
    engine->destroy_image(img);
  }

  VkDevice dv = engine->m_Device.logical;
  vkDestroySampler(dv, sampler, nullptr);
  vkDestroyPipelineLayout(dv, pipelineLayout, nullptr);
  vkDestroyPipeline(dv, upsamplePipeline, nullptr);
  vkDestroyPipeline(dv, downsamplePipeline, nullptr);
  vkDestroyDescriptorSetLayout(dv, descriptorLayout, nullptr);
}

} // aurora namespace
