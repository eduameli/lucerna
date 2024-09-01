#pragma once
#include "aurora_pch.h"
#include "vk_types.h"
#include <filesystem>

namespace Aurora {

// forward declaration
class Engine;

struct GLTFMaterial
{
  MaterialInstance data;
};

struct GeoSurface
{
  uint32_t startIndex;
  uint32_t count;
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


} // namespace aurora


