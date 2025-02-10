#pragma once
#include "lucerna_pch.h"
#include "vk_types.h"
#include "engine.h"
#include "vulkan/vulkan_core.h"

namespace Lucerna {
  class VulkanImGuiBackend
  {
    public:
      static void init(Engine* engine);
      static void render_ui(VkCommandBuffer cmd, VkImageView target);
      static void cleanup(Engine* engine);
    public:
    private:
      static void draw(VkCommandBuffer cmd, VkDevice device, VkImageView swapchainImageView, VkExtent2D swapchainExtent);
      static void copy_buffers(VkCommandBuffer cmd, VkDevice device);
    private:
      static inline AllocatedBuffer indexBuffer{};
      static inline AllocatedBuffer vertexBuffer{};
      static inline AllocatedImage fontImage{};
      static inline VkPipeline pipeline{};
      static inline VkPipelineLayout pipLayout{};
  };


  
  struct FrameGraph
  {
    static void add_sample(float sample);
    static void render_graph();
    static inline std::deque<float> frametimes{};
    static inline float average = 0;
  };

} // namespace Lucerna
