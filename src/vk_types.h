#pragma once

#include <vulkan/vulkan.h>
#include "vk_mem_alloc.h"

namespace Aurora
{
  struct AllocatedImage
  {
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VkExtent3D imageExtent;
    VkFormat imageFormat;
  };
}
