#pragma once
#include "vk_types.h"
#include "aurora_pch.h"
#include "vk_descriptors.h"

namespace Aurora {
  
  struct Engine;

  struct GLTFMetallic_Roughness
    {
    MaterialPipeline opaquePipeline;
    MaterialPipeline transparentPipeline;
    VkDescriptorSetLayout materialLayout;
    
    struct MaterialConstants
    {
      glm::vec4 colorFactors;
      glm::vec4 metal_rough_factors;
      glm::vec4 extra[14]; // uniform buffers need minimum requirement for alignment, 256bytes is a good default alignment that gpus meet 
    };

    struct MaterialResources
    {
      AllocatedImage colorImage;
      VkSampler colorSampler;
      AllocatedImage metalRoughImage;
      VkSampler metalRoughSampler;
      VkBuffer dataBuffer;
      uint32_t dataBufferOffset;
    };

    DescriptorWriter writer;
    void build_pipelines(Engine* engine);
    void clear_resources(VkDevice device); // FIXME: not implemented??
    MaterialInstance write_material(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator);
  };

  struct MeshNode : public Node {

	std::shared_ptr<MeshAsset> mesh;

	virtual void draw(const glm::mat4& topMatrix, DrawContext& ctx) override;
};
} // namespace aurora
