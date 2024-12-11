#include "cvars.h"
#include "gfx_effects.h"

#include "engine.h"
#include "vk_initialisers.h"
#include "ar_asserts.h"
#include "vk_images.h"
#include "vk_pipelines.h"
#include <glm/packing.hpp>
#include <glm/gtx/string_cast.hpp>
#include <vulkan/vulkan_core.h>

namespace Aurora
{

AutoCVar_Int bloomEnabled{"bloom.enabled", "", 1, CVarFlags::EditCheckbox};

void bloom::prepare()
{
  Engine* engine = Engine::get();
  VkDevice device = engine->device;

  // create mip chain
  VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;
  VkExtent3D size = {engine->internalExtent.width / 2, engine->internalExtent.height / 2, 1};

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
    AllocatedImage img = engine->create_image(mipSize, format, usages);
    std::string identifier = "Bloom Mipmap Level" + std::to_string(i); 
    vklog::label_image(device, img.image, identifier.c_str());
    blurredMips.push_back(img);
  }
    
  // FIXME: kind of ass having to do one full immediate submit just to transfer an img
  engine->immediate_submit([=](VkCommandBuffer cmd){
    for (auto img : blurredMips)
    {
      vkutil::transition_image(cmd, img.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    }
  });

  {
    DescriptorLayoutBuilder builder;
    builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    descriptorLayout = builder.build(device, VK_SHADER_STAGE_COMPUTE_BIT);
  }


  // create compute pipelines 
  VkPipelineLayoutCreateInfo computeLayout{ .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .pNext = nullptr};
  computeLayout.pSetLayouts = &descriptorLayout;
  computeLayout.setLayoutCount = 1;

  VkPushConstantRange pcs{};
  pcs.offset = 0;
  pcs.size = sizeof(BloomPushConstants);
  pcs.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  computeLayout.pPushConstantRanges = &pcs;
  computeLayout.pushConstantRangeCount = 1;

  VK_CHECK_RESULT(vkCreatePipelineLayout(device, &computeLayout, nullptr, &pipelineLayout));
  
  VkShaderModule downsample, upsample;
  AR_LOG_ASSERT(
    vkutil::load_shader_module("shaders/bloom/downsample.comp.spv", device, &downsample),
    "Error loading (Bloom) Downsample Compute Effect Shader"
  );

  AR_LOG_ASSERT(
    vkutil::load_shader_module("shaders/bloom/upsample.comp.spv", device, &upsample),
    "Error loading (Bloom) Upsample Compute Effect Shader"
  ); 
  
  VkPipelineShaderStageCreateInfo stageInfo = vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_COMPUTE_BIT, downsample);
  VkComputePipelineCreateInfo pipelineInfo{ .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, .pNext = nullptr};
  pipelineInfo.layout = pipelineLayout;
  pipelineInfo.stage = stageInfo;
  VK_CHECK_RESULT(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &downsamplePipeline));
  
  pipelineInfo.stage.module = upsample;
  VK_CHECK_RESULT(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &upsamplePipeline));
 

  descriptorSet = engine->globalDescriptorAllocator.allocate(device, descriptorLayout);
  
  VkSamplerCreateInfo samplerInfo{.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr};
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

  samplerInfo.magFilter = VK_FILTER_LINEAR;
  samplerInfo.minFilter = VK_FILTER_LINEAR;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

  vkCreateSampler(device, &samplerInfo, nullptr, &sampler);
  
  vkDestroyShaderModule(device, downsample, nullptr);
  vkDestroyShaderModule(device, upsample, nullptr);

  engine->m_DeletionQueue.push_function([device, engine] {
    for (auto img : blurredMips)
    {
      engine->destroy_image(img);
    }
    vkDestroySampler(device, sampler, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    vkDestroyPipeline(device, upsamplePipeline, nullptr);
    vkDestroyPipeline(device, downsamplePipeline, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorLayout, nullptr);

  });

}

void bloom::run(VkCommandBuffer cmd, VkImageView targetImage)
{

  if (bloomEnabled.get() == false) return;

  // run compute downsample
  // run compute upsample
  // mix with target img

  Engine* engine = Engine::get();
  VkExtent3D size = {engine->internalExtent.width, engine->internalExtent.height, 1};

  bloom_pcs pcs{};
  pcs.srcResolution = {size.width, size.height};
  pcs.filterRadius = 0.005;
  pcs.mipLevel = 0;
  
  // draw img to mip0
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, downsamplePipeline);

  VkDescriptorSet firstSet = engine->get_current_frame().frameDescriptors.allocate(engine->device, descriptorLayout);
  {
    DescriptorWriter writer;
    writer.write_image(0, engine->m_DrawImage.imageView, sampler, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    writer.write_image(1, blurredMips[0].imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    writer.update_set(engine->device, firstSet);
  }
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &firstSet, 0, nullptr);

  vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(bloom_pcs), &pcs);
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
    VkDescriptorSet set = engine->get_current_frame().frameDescriptors.allocate(engine->device, descriptorLayout);
    {
      DescriptorWriter writer;
      writer.write_image(0, blurredMips[i - 1].imageView, sampler, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
      writer.write_image(1, blurredMips[i].imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
      writer.update_set(engine->device, set);
    }
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &set, 0, nullptr);

    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(bloom_pcs), &pcs);
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
    VkDescriptorSet set = engine->get_current_frame().frameDescriptors.allocate(engine->device, descriptorLayout);
    {
      DescriptorWriter writer;
      writer.write_image(0, blurredMips[i].imageView, sampler, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
      writer.write_image(1, blurredMips[i-1].imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
      writer.update_set(engine->device, set);
    }
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &set, 0, nullptr);
    
    pcs.srcResolution = {size.width, size.height};
    size.width *= 2;
    size.height *= 2;

    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(bloom_pcs), &pcs);
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
  pcs.mipLevel = 1; 

  VkDescriptorSet set = engine->get_current_frame().frameDescriptors.allocate(engine->device, descriptorLayout);
  {
    DescriptorWriter writer;
    writer.write_image(0, blurredMips[0].imageView, sampler, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    writer.write_image(1, engine->m_DrawImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    writer.update_set(engine->device, set);
  }
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &set, 0, nullptr);
  vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(BloomPushConstants), &pcs);
  
  pcs.srcResolution = {size.width, size.height};
  size.width *= 2;
  size.height *= 2;

  vkCmdDispatch(cmd, std::ceil(size.width / 16.0), std::ceil(size.height / 16.0), 1);

  // FIXME: do i need a barrier here?
}


void ssao::prepare()
{
  /*
    create compute pipeline - only push constants
    create output image
  */

  Engine* engine = Engine::get();
  VkDevice device = engine->device;
  
  {
    DescriptorLayoutBuilder builder;
    builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    builder.add_binding(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    builder.add_binding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    descLayout = builder.build(device, VK_SHADER_STAGE_COMPUTE_BIT);
  }

  {
    DescriptorLayoutBuilder builder;
    builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    builder.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    blurDescLayout = builder.build(device, VK_SHADER_STAGE_COMPUTE_BIT);
  }

  VkPushConstantRange range{};
  range.offset = 0;
  range.size = sizeof(ssao_pcs);
  range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  VkPipelineLayoutCreateInfo layout = vkinit::pipeline_layout_create_info();
  layout.pushConstantRangeCount = 1;
  layout.pPushConstantRanges = &range;
  layout.setLayoutCount = 1;
  layout.pSetLayouts = &descLayout;

  vkCreatePipelineLayout(device, &layout, nullptr, &pipelineLayout);

  VkPipelineLayoutCreateInfo ly = vkinit::pipeline_layout_create_info();
  ly.pushConstantRangeCount = 1;
  VkPushConstantRange range2 = VkPushConstantRange{
    .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
    .offset = 0,
    .size = sizeof(uint32_t) // NOTE: place the pcs struct here
  };
  ly.pPushConstantRanges = &range;
  ly.setLayoutCount = 1;
  ly.pSetLayouts = &blurDescLayout;
  vkCreatePipelineLayout(device, &ly, nullptr, &blurPipelineLayout);
    
  VkShaderModule ssaoShader;
  AR_LOG_ASSERT(
    vkutil::load_shader_module("shaders/ssao/ssao.comp.spv", device, &ssaoShader),
    "Error loading SSAO Effect Shader"
  );  

  VkShaderModule blurShader;
  AR_LOG_ASSERT(
    vkutil::load_shader_module("shaders/ssao/bilateral_filter.comp.spv", device , &blurShader),
    "Error loading Bilateral Blur SSAO Effect Shader" 
  );

  VkPipelineShaderStageCreateInfo stageInfo = vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_COMPUTE_BIT, ssaoShader);
  
  VkComputePipelineCreateInfo computePipelineCreateInfo{};
  computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  computePipelineCreateInfo.pNext = nullptr;
  computePipelineCreateInfo.layout = pipelineLayout;
  computePipelineCreateInfo.stage = stageInfo;
  
  VK_CHECK_RESULT(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &ssaoPipeline));

  computePipelineCreateInfo.layout = blurPipelineLayout;
  computePipelineCreateInfo.stage = vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_COMPUTE_BIT, blurShader);
  VK_CHECK_RESULT(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &blurPipeline));
  

  // create output image
  VkFormat format = VK_FORMAT_R8_UNORM;
  VkImageUsageFlags usages = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT; /*last one for debugging*/
  VkExtent3D size = engine->internalExtent;
  outputAmbient = engine->create_image(size, format, usages);
  outputBlurred = engine->create_image(size, format, usages);
  vklog::label_image(device, outputAmbient.image, "SSAO Output Ambient Texture");
  vklog::label_image(device, outputBlurred.image, "SSAO Output Blurred Ambient Texture");  // noise image

  std::uniform_real_distribution<float> randomFloats(0.0, 1.0); // random floats between [0.0, 1.0]
  std::default_random_engine generator(std::chrono::system_clock::now().time_since_epoch().count()); // using default seed!
  
  // NOTE: use bayern matrix pattern and the bilateral filter?
  std::vector<uint32_t> ssaoNoise;
  for (unsigned int i = 0; i < 16; i++)
  {
      glm::vec4 noise(
          randomFloats(generator) * 2.0 - 1.0, 
          randomFloats(generator) * 2.0 - 1.0, 
          0.0f,
          1.0f); 
      ssaoNoise.push_back(glm::packSnorm4x8(noise));
  }
  
  noiseImage = engine->create_image((void*) ssaoNoise.data(), VkExtent3D{4, 4, 1}, VK_FORMAT_R8G8B8A8_SNORM, VK_IMAGE_USAGE_SAMPLED_BIT); 



  //FIXME: i might be a dumbass and u dont do this!!
  engine->immediate_submit([=](VkCommandBuffer cmd){
    vkutil::transition_image(cmd, outputAmbient.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    vkutil::transition_image(cmd, outputBlurred.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
  });
  
  VkSamplerCreateInfo sampl{.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr};
  sampl.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampl.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampl.magFilter = VK_FILTER_NEAREST;
  sampl.minFilter = VK_FILTER_NEAREST;
  vkCreateSampler(device, &sampl, nullptr, &depthSampler);
  
  sampl.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  sampl.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  vkCreateSampler(device, &sampl, nullptr, &noiseSampler);
  
  ssaoUniform = engine->create_buffer(sizeof(u_ssao), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
  std::array<glm::vec3, 64> samples;

  auto lerp = [](float a, float b, float f) {
    return a + f * (b - a ); 
  };

  for (unsigned int i = 0; i < 64; ++i)
  {
      glm::vec3 sample(
          randomFloats(generator) * 2.0 - 1.0, 
          randomFloats(generator) * 2.0 - 1.0, 
          randomFloats(generator)
      );
      sample  = glm::normalize(sample);
      
      // accelerating interpolation function
      float scale = (float)i / 64.0; 
      scale   = lerp(0.1f, 1.0f, scale * scale);
      sample *= scale;

      samples[i] = sample;  
  }
  
  memcpy(ssaoUniform.info.pMappedData, samples.data(), sizeof(glm::vec3) * 64);
  // allocate uniform buffer 64 vec3

  vkDestroyShaderModule(device, ssaoShader, nullptr);
  vkDestroyShaderModule(device, blurShader, nullptr);
  
  engine->m_DeletionQueue.push_function([device, engine](){
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    engine->destroy_image(outputAmbient);
    vkDestroyPipeline(device, ssaoPipeline, nullptr);
    vkDestroyDescriptorSetLayout(device, descLayout, nullptr);
    vkDestroySampler(device, depthSampler, nullptr);
    vkDestroySampler(device, noiseSampler, nullptr);
    engine->destroy_buffer(ssaoUniform);
    engine->destroy_image(noiseImage);

    engine->destroy_image(outputBlurred);
    vkDestroyPipeline(device, blurPipeline, nullptr);
    vkDestroyPipelineLayout(device, blurPipelineLayout, nullptr);
    vkDestroyDescriptorSetLayout(device, blurDescLayout, nullptr);
  });

}

// FIXME: NEED BARRIER OR SMTH CAUSE IF IT TAKES TOO LONG IT MESSES WITH FORWARD PASS
void ssao::run(VkCommandBuffer cmd, VkImageView depth)
{
  /*
  bind pipeline
  write descriptor set -- depth binding 0 output binding 1
  */
  
  Engine* engine = Engine::get();
  VkExtent3D size = engine->internalExtent;

  VkDescriptorSet set = engine->get_current_frame().frameDescriptors.allocate(engine->device, descLayout);
  {
    DescriptorWriter writer;
    writer.write_image(0, depth, depthSampler, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    writer.write_image(1, outputAmbient.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    writer.write_buffer(2, ssaoUniform.buffer, sizeof(glm::vec3)*64, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    writer.write_image(3, noiseImage.imageView, noiseSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    writer.update_set(engine->device, set);
  }

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, ssaoPipeline);
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &set, 0, nullptr);

  float near = *CVarSystem::get()->get_float_cvar("camera.near");
  float far = *CVarSystem::get()->get_float_cvar("camera.far");
  float fov = *CVarSystem::get()->get_float_cvar("camera.fov");
  VkExtent2D extent = engine->m_DrawExtent; 
  float aspectRatio = (float) extent.width / (float) extent.height;
  
  ssao_pcs pcs{};
  pcs.kernelRadius = *CVarSystem::get()->get_float_cvar("ssao.kernel_radius");
  pcs.inv_viewproj = glm::inverse(Engine::get()->sceneData.viewproj);
  vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ssao_pcs), &pcs);

  vkCmdDispatch(cmd, std::ceil(size.width / 16.0), std::ceil(size.height / 16.0), 1);
  
  {
    VkImageMemoryBarrier2 imgBarrier{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2, .pNext = nullptr};
    imgBarrier.srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    imgBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    imgBarrier.dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    imgBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    imgBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    imgBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    imgBarrier.image = outputAmbient.image;
    imgBarrier.subresourceRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
  
    VkDependencyInfo depInfo{ .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .pNext = nullptr};
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &imgBarrier;

    vkCmdPipelineBarrier2(cmd, &depInfo);
  }

  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, blurPipeline);

  VkDescriptorSet set2 = engine->get_current_frame().frameDescriptors.allocate(engine->device, blurDescLayout);
  {
    DescriptorWriter writer;
    writer.write_image(0, outputAmbient. imageView, engine->m_DefaultSamplerNearest, VK_IMAGE_LAYOUT_GENERAL,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    writer.write_image(1, outputBlurred.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    writer.update_set(engine->device, set2);
  }
  
  bilateral_filter_pcs blurpcs{};
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, blurPipelineLayout, 0, 1, &set2, 0, nullptr);
  vkCmdPushConstants(cmd, blurPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(bilateral_filter_pcs), &blurpcs);
  
  vkCmdDispatch(cmd, std::ceil(size.width / 16.0), std::ceil(size.height / 16.0), 1);
  
  
  VkImageMemoryBarrier2 imgBarrier{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2, .pNext = nullptr};
  imgBarrier.srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
  imgBarrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
  imgBarrier.dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
  imgBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
  imgBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
  imgBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
  imgBarrier.image = outputBlurred.image;
  imgBarrier.subresourceRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
  
  VkDependencyInfo depInfo{ .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .pNext = nullptr};
  depInfo.imageMemoryBarrierCount = 1;
  depInfo.pImageMemoryBarriers = &imgBarrier;

  vkCmdPipelineBarrier2(cmd, &depInfo);
  // FIXME: do i need a barrier here??
}

} // aurora namespace
