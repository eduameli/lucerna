#pragma once
#include "aurora_pch.h"
#include <vulkan/vulkan.h>

namespace vkutil
{

void transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);
void copy_image_to_image(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize);
std::vector<VkImageView> get_image_views(VkDevice device, VkFormat format, const std::vector<VkImage>& images);
}
