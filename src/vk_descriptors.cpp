#include "vk_descriptors.h"

void DescriptorLayoutBuilder::add_binding(uint32_t binding, VkDescriptorType type)
{
  VkDescriptorSetLayoutBinding newbind{};
  newbind.binding = binding;
  newbind.descriptorCount = 1;
  newbind.descriptorType = type;

  bindings.push_back(newbind);
}

void DescriptorLayoutBuilder::clear()
{
  bindings.clear();
}

VkDescriptorSetLayout DescriptorLayoutBuilder::build(VkDevice device, VkShaderStageFlags shaderStages, void* pNext, VkDescriptorSetLayoutCreateFlags flags)
{
  // NOTE: no support for per-binding stqge flags, stage flags are forced for the whole descriptor set.
  for (auto& b : bindings)
  {
    b.stageFlags |= shaderStages;
  }

  VkDescriptorSetLayoutCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  info.pNext = pNext;

  info.pBindings = bindings.data();
  info.bindingCount = (uint32_t) bindings.size();
  info.flags = flags;

  VkDescriptorSetLayout set;
  VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &info, nullptr, &set));
  return set; 
}

void DescriptorAllocator::init_pool(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios)
{
  std::vector<VkDescriptorPoolSize> poolSizes;
  for (PoolSizeRatio ratio : poolRatios)
  {
    VkDescriptorPoolSize size{};
    size.type = ratio.type;
    size.descriptorCount = uint32_t(ratio.ratio * maxSets);
    poolSizes.push_back(size);
  }

  VkDescriptorPoolCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  info.flags = 0;
  info.maxSets = maxSets;
  info.poolSizeCount = (uint32_t) poolSizes.size();
  info.pPoolSizes = poolSizes.data();
  
  VK_CHECK_RESULT(vkCreateDescriptorPool(device, &info, nullptr, &descriptorPool));
}

void DescriptorAllocator::destroy_pool(VkDevice device)
{
  vkDestroyDescriptorPool(device, descriptorPool, nullptr);
}

void DescriptorAllocator::clear_descriptors(VkDevice device)
{
  vkResetDescriptorPool(device, descriptorPool, 0);
}

VkDescriptorSet DescriptorAllocator::allocate(VkDevice device, VkDescriptorSetLayout layout)
{
  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.pNext = nullptr;
  allocInfo.descriptorPool = descriptorPool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &layout;
  
  VkDescriptorSet ds;
  VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &ds));
  return ds;
}
