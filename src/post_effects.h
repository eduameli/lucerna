#pragma once
#include "vk_types.h"

namespace Aurora
{

  class Engine;

//should it be a singleton?
  class BloomEffect 
  {
    public:
      static void prepare();
      static void run(VkCommandBuffer cmd, VkImageView targetImage);
      static void cleanup(Engine* engine);
    public:
      struct BloomPushConstants
      {
        glm::vec2 srcResolution;
        float filterRadius;
        int finalPass{0};
      };
    public:
    private:
      static inline std::vector<AllocatedImage> blurredMips{};
      static inline VkPipelineLayout pipelineLayout{};
      static inline VkPipeline upsamplePipeline{};
      static inline VkPipeline downsamplePipeline{};
      static inline VkDescriptorSet descriptorSet{}; // just use one img
      static inline VkSampler sampler{};
      static inline VkDescriptorSetLayout descriptorLayout{};
      static inline VkShaderModule downsample{};
      static inline VkShaderModule upsample{};
  };

} // aurora namespace
