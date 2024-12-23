#pragma once
#include "aurora_pch.h"
#include "vk_types.h"
#include "engine.h"

namespace Aurora
{
  class VulkanImGuiBackend
  {
    public:
      static void init(Engine* engine);
      static void draw(VkCommandBuffer cmd, VkDevice device, VkImageView swapchainImageView, VkExtent2D swapchainExtent);
      static void cleanup(Engine* engine);
    public:
    private:
      static void copy_buffers(VkCommandBuffer cmd, VkDevice device);
    private:
      static inline AllocatedBuffer indexBuffer{};
      static inline AllocatedBuffer vertexBuffer{};
      static inline AllocatedImage fontImage{};
      static inline VkPipeline pipeline{};
      static inline VkPipelineLayout pipLayout{};
  };

} // namespace aurora
