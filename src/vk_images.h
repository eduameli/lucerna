#pragma once
#include "lucerna_pch.h"
#include <volk.h>
namespace vkutil
{

void transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);
void copy_image_to_image(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize);
void generate_mipmaps(VkCommandBuffer cmd, VkImage image, VkExtent2D extent); //FIXME: load KTX or DDS - or generate in parallel using compute
std::vector<VkImageView> get_image_views(VkDevice device, VkFormat format, const std::vector<VkImage>& images);
}
