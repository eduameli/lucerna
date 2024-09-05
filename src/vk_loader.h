#pragma once
#include "aurora_pch.h"
#include "vk_types.h"
#include <filesystem>
#include "vk_descriptors.h"
#include "fastgltf/core.hpp"

namespace Aurora {

// forward declaration
class Engine;

struct GLTFMaterial
{
  MaterialInstance data;
};

struct Bounds
{
  glm::vec3 origin;
  float sphereRadius;
  glm::vec3 extents;
};

struct GeoSurface
{
  uint32_t startIndex;
  uint32_t count;
  Bounds bounds;
  std::shared_ptr<GLTFMaterial> material;
};

struct MeshAsset
{
  std::string name;
  std::vector<GeoSurface> surfaces;
  GPUMeshBuffers meshBuffers;
};

//FIXME: also use optional for loading ShaderModule when making pipelines!
std::optional<std::vector<std::shared_ptr<MeshAsset>>> load_gltf_meshes(Engine* engine, std::filesystem::path filepath);

struct LoadedGLTF : public IRenderable
{
  std::unordered_map<std::string, std::shared_ptr<MeshAsset>> meshes;
  std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
  std::unordered_map<std::string, AllocatedImage> images;
  std::unordered_map<std::string, std::shared_ptr<GLTFMaterial>> materials;

  std::vector<std::shared_ptr<Node>> topNodes;

  std::vector<VkSampler> samplers;

  DescriptorAllocatorGrowable descriptorPool;

  AllocatedBuffer materialDataBuffer;

  Engine* creator;
  
  void clearAll();

  ~LoadedGLTF() {clearAll();}
  
  virtual void draw(const glm::mat4& topMatrix, DrawContext& ctx);

private:
};

std::optional<std::shared_ptr<LoadedGLTF>> load_gltf(Engine* engine, std::filesystem::path filepath); 
VkFilter extract_filter(fastgltf::Filter filter);
VkSamplerMipmapMode extract_mipmap_mode(fastgltf::Filter filter);
std::optional<AllocatedImage> load_image(Engine* engine, fastgltf::Asset& asset, fastgltf::Image& image);
} // namespace aurora


