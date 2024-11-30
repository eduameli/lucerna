#pragma once
#include "aurora_pch.h"
#include "vk_types.h"
#include <filesystem>
#include "vk_descriptors.h"
#include "fastgltf/core.hpp"
namespace Aurora {

// forward declaration
class Engine;


//FIXME: also use optional for loading ShaderModule when making pipelines!

struct LoadedGLTF : public IRenderable
{
  std::unordered_map<std::string, std::shared_ptr<MeshAsset>> meshes;
  std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
  std::unordered_map<std::string, AllocatedImage> images;
  std::unordered_map<std::string, std::shared_ptr<GLTFMaterial>> materials;

  std::vector<std::shared_ptr<Node>> topNodes;
  std::vector<VkSampler> samplers;

  std::vector<glm::mat4> gltf_transforms;
  
  DescriptorAllocatorGrowable descriptorPool;

  AllocatedBuffer materialDataBuffer;

  Engine* creator;
  
  void clearAll();

  ~LoadedGLTF() {clearAll();}
  
  virtual void queue_draw(const glm::mat4& topMatrix, DrawContext& ctx);

private:
};

std::optional<std::shared_ptr<LoadedGLTF>> load_gltf(Engine* engine, std::filesystem::path filepath);
std::optional<std::shared_ptr<LoadedGLTF>> load_gltf_asset(Engine* engine, std::filesystem::path filepath);
VkFilter extract_filter(fastgltf::Filter filter);
VkSamplerMipmapMode extract_mipmap_mode(fastgltf::Filter filter);
std::optional<AllocatedImage> load_image(Engine* engine, fastgltf::Asset& asset, fastgltf::Image& image);


uint32_t build_material(Engine* engine, BindlessMaterial mat);

// FIXME: add better pbr / emission here instead of pcs
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

struct MeshNode : public Node
{
	std::shared_ptr<MeshAsset> mesh;
	virtual void queue_draw(const glm::mat4& topMatrix, DrawContext& ctx) override;
};



} // namespace aurora


