#include "vk_images.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"


#include "vk_initialisers.h"
#include "ar_asserts.h"

void vkutil::transition_image(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout)
{
    VkImageMemoryBarrier2 imageBarrier{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2, .pNext = nullptr };
    imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
    imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

    imageBarrier.oldLayout = currentLayout;
    imageBarrier.newLayout = newLayout;

    VkImageAspectFlags aspectMask = (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL || newLayout == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    imageBarrier.subresourceRange = vkinit::image_subresource_range(aspectMask);
    imageBarrier.image = image;

    VkDependencyInfo depInfo {};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.pNext = nullptr;

    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &imageBarrier;

    vkCmdPipelineBarrier2(cmd, &depInfo);
}


void vkutil::copy_image_to_image(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize)
{
  VkImageBlit2 blitRegion{ .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2, .pNext = nullptr };

  blitRegion.srcOffsets[1].x = srcSize.width;
  blitRegion.srcOffsets[1].y = srcSize.height;
  blitRegion.srcOffsets[1].z = 1;

  blitRegion.dstOffsets[1].x = dstSize.width;
  blitRegion.dstOffsets[1].y = dstSize.height;
  blitRegion.dstOffsets[1].z = 1;

  blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  blitRegion.srcSubresource.baseArrayLayer = 0;
  blitRegion.srcSubresource.layerCount = 1;
  blitRegion.srcSubresource.mipLevel = 0;

  blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  blitRegion.dstSubresource.baseArrayLayer = 0;
  blitRegion.dstSubresource.layerCount = 1;
  blitRegion.dstSubresource.mipLevel = 0;

  VkBlitImageInfo2 info{ .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2, .pNext = nullptr };
  info.dstImage = destination;
  info.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

  info.srcImage = source;
  info.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  info.filter = VK_FILTER_LINEAR;
  info.regionCount = 1;
  info.pRegions = &blitRegion;

  vkCmdBlitImage2(cmd, &info);
}

std::vector<VkImageView> vkutil::get_image_views(VkDevice device, VkFormat format, const std::vector<VkImage>& images)
{
  std::vector<VkImageView> views(images.size());

  for (size_t i = 0; i < images.size(); i++)
  {
    VkImageViewCreateInfo createInfo{ .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, .pNext = nullptr};
    createInfo.image = images[i];
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = format;
    
    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    
    createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;

    VK_CHECK_RESULT(vkCreateImageView(device, &createInfo, nullptr, &views[i]));
  }
  return std::move(views);
}

void vkutil::generate_mipmaps(VkCommandBuffer cmd, VkImage image, VkExtent2D extent)
{
  int mipLevels = int(std::floor(std::log2(std::max(extent.width, extent.height)))) + 1;

  for (int mip = 0; mip < mipLevels; mip++)
  {
    VkExtent2D halfSize = {extent.width / 2, extent.height / 2};

    VkImageMemoryBarrier2 imageBarrier {.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2, .pNext = nullptr};
    imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
    imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

    imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    
    VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageBarrier.subresourceRange = vkinit::image_subresource_range(aspectMask);
    imageBarrier.subresourceRange.levelCount = 1;
    imageBarrier.subresourceRange.baseMipLevel = mip;
    imageBarrier.image = image;

    VkDependencyInfo depInfo {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .pNext = nullptr};
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &imageBarrier;

    vkCmdPipelineBarrier2(cmd, &depInfo);

    if (mip < mipLevels - 1)
    {
      VkImageBlit2 blitRegion {.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2, .pNext = nullptr};
      blitRegion.srcOffsets[1].x = extent.width;
      blitRegion.srcOffsets[1].y = extent.height;
      blitRegion.srcOffsets[1].z = 1;
      
      blitRegion.dstOffsets[1].x = halfSize.width;
      blitRegion.dstOffsets[1].y = halfSize.height;
      blitRegion.dstOffsets[1].z = 1;

      blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      blitRegion.srcSubresource.baseArrayLayer = 0;
      blitRegion.srcSubresource.layerCount = 1;
      blitRegion.srcSubresource.mipLevel = mip;
  
      blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      blitRegion.dstSubresource.baseArrayLayer = 0;
      blitRegion.dstSubresource.layerCount = 1;
      blitRegion.dstSubresource.mipLevel = mip + 1;

      VkBlitImageInfo2 blitInfo {.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2, .pNext = nullptr};
      blitInfo.dstImage = image;
      blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      blitInfo.srcImage = image;
      blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      blitInfo.filter = VK_FILTER_LINEAR;
      blitInfo.regionCount = 1;
      blitInfo.pRegions = &blitRegion;

      vkCmdBlitImage2(cmd, &blitInfo);
      extent = halfSize;
    }
  }

  transition_image(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

