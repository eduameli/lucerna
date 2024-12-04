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

  std::vector<std::shared_ptr<Node>> topNodes;
  std::vector<VkSampler> samplers;
  Engine* creator;

  void clearAll();

  ~LoadedGLTF() {clearAll();}
  
  virtual void queue_draw(const glm::mat4& topMatrix, DrawContext& ctx);

private:
};

std::optional<std::shared_ptr<LoadedGLTF>> load_gltf(Engine* engine, std::filesystem::path filepath);
VkFilter extract_filter(fastgltf::Filter filter);
VkSamplerMipmapMode extract_mipmap_mode(fastgltf::Filter filter);
std::optional<AllocatedImage> load_image(Engine* engine, fastgltf::Asset& asset, fastgltf::Image& image, std::filesystem::path fpath);

struct MeshNode : public Node
{
	std::shared_ptr<MeshAsset> mesh;
	virtual void queue_draw(const glm::mat4& topMatrix, DrawContext& ctx) override;
};



} // namespace aurora


