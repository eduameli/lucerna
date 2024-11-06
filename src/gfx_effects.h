#pragma once
#include "vk_types.h"

namespace Aurora
{

  class Engine;

//should it be a singleton?
  class bloom 
  {
    public:
      static void prepare();
      static void run(VkCommandBuffer cmd, VkImageView targetImage);
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
  };
  
  class ssao
  {
    public:
      static void prepare();
      static void run(VkCommandBuffer cmd, VkImageView depth);
    public:
      static inline AllocatedImage outputAmbient;
    private:
    private:
      static inline VkPipelineLayout pipelineLayout{};
      static inline VkPipeline ssaoPipeline{};
      static inline VkDescriptorSetLayout descLayout{};
  };
} // aurora namespace
