#pragma once
#include <volk.h>
#include "aurora_pch.h"

struct DescriptorLayoutBuilder
{
  std::vector<VkDescriptorSetLayoutBinding> bindings;
  void add_binding(uint32_t binding, VkDescriptorType type);
  void clear();
  VkDescriptorSetLayout build(VkDevice device, VkShaderStageFlags shaderStages, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);
};

struct DescriptorAllocator 
{
  public:
    struct PoolSizeRatio
    {
      VkDescriptorType type;
      float ratio;
    };
    VkDescriptorPool descriptorPool;
  public:
    void init_pool(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios);
    void destroy_pool(VkDevice device);
    void clear_descriptors(VkDevice device);
    VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout);
  private:
};
