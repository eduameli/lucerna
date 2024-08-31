#include "vk_descriptors.h"
#include "logger.h"
#include "ar_asserts.h"

namespace Aurora {

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

void DescriptorAllocatorGrowable::init(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios)
{
  ratios.clear();

  for (auto r : poolRatios)
  {
    ratios.push_back(r);
  }

  VkDescriptorPool newPool = create_pool(device, maxSets, poolRatios);
  setsPerPool = std::min(static_cast<uint32_t>(maxSets * 1.5), 4092u);
  readyPools.push_back(newPool);
}

void DescriptorAllocatorGrowable::clear_pools(VkDevice device)
{
  for (auto p : readyPools)
  {
    vkResetDescriptorPool(device, p, 0);
  }

  for (auto p : fullPools)
  {
    vkResetDescriptorPool(device, p, 0);
    readyPools.push_back(p);
  }

  fullPools.clear();
}

void DescriptorAllocatorGrowable::destroy_pools(VkDevice device)
{
  for (auto p : readyPools)
  {
    vkDestroyDescriptorPool(device, p, nullptr);
  }
  for (auto p : fullPools)
  {
    vkDestroyDescriptorPool(device, p, nullptr);
  }
  readyPools.clear();
  fullPools.clear();
}

VkDescriptorSet DescriptorAllocatorGrowable::allocate(VkDevice device, VkDescriptorSetLayout layout, void* pNext)
{
  VkDescriptorPool poolToUse = get_pool(device);

  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.pNext = pNext;
  allocInfo.descriptorPool = poolToUse;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &layout;

  VkDescriptorSet ds;
  VkResult result = vkAllocateDescriptorSets(device, &allocInfo, &ds);
  if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL)
  {
    fullPools.push_back(poolToUse);

    poolToUse = get_pool(device);
    allocInfo.descriptorPool = poolToUse;
    VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &ds));
  }

  readyPools.push_back(poolToUse);
  return ds;
}

VkDescriptorPool DescriptorAllocatorGrowable::get_pool(VkDevice device)
{
  VkDescriptorPool newPool;
  if (readyPools.size() != 0)
  {
    newPool = readyPools.back();
    readyPools.pop_back();
  }
  else
  {
    newPool = create_pool(device, setsPerPool, ratios);
    setsPerPool = std::min(static_cast<uint32_t>(setsPerPool * 1.5), 4092u);
  }
  return newPool;
}



VkDescriptorPool DescriptorAllocatorGrowable::create_pool(VkDevice device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios)
{
  std::vector<VkDescriptorPoolSize> poolSizes;
  for (PoolSizeRatio ratio : poolRatios)
  {
    poolSizes.push_back(VkDescriptorPoolSize{
      .type = ratio.type,
      .descriptorCount = static_cast<uint32_t>(ratio.ratio * setCount)
    });
  }

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.flags = 0;
  poolInfo.maxSets = setCount;
  poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
  poolInfo.pPoolSizes = poolSizes.data();

  VkDescriptorPool newPool;
  VK_CHECK_RESULT(vkCreateDescriptorPool(device, &poolInfo, nullptr, &newPool));
  return newPool;
}

void DescriptorWriter::write_buffer(int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type)
{
  VkDescriptorBufferInfo& info = bufferInfos.emplace_back(VkDescriptorBufferInfo{
    .buffer = buffer,
    .offset = offset,
    .range = size,
  });

  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstBinding = binding;
  write.dstSet = VK_NULL_HANDLE;
  write.descriptorCount = 1;
  write.descriptorType = type;
  write.pBufferInfo = &info;

  writes.push_back(write);
}

void DescriptorWriter::write_image(int binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type)
{
  VkDescriptorImageInfo& info = imageInfos.emplace_back(VkDescriptorImageInfo{
    .sampler = sampler,
    .imageView = image,
    .imageLayout = layout
  });

  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstBinding = binding;
  write.dstSet = VK_NULL_HANDLE;
  write.descriptorCount = 1;
  write.descriptorType = type;
  write.pImageInfo = &info;

  writes.push_back(write);
}

void DescriptorWriter::clear()
{
  imageInfos.clear();
  writes.clear();
  bufferInfos.clear();
}

void DescriptorWriter::update_set(VkDevice device, VkDescriptorSet set)
{
  for (VkWriteDescriptorSet& write : writes)
  {
    write.dstSet = set;
  }
  vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

} // namespace aurora
