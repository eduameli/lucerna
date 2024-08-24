#pragma once
#include "aurora_pch.h"
#include "vk_types.h"
#include <filesystem>

namespace Aurora {

// forward declaration
class Engine;

struct GeoSurface
{
  uint32_t startIndex;
  uint32_t count;
};

struct MeshAsset
{
  std::string name;
  std::vector<GeoSurface> surfaces;
  GPUMeshBuffers meshBuffers;
};

} // namespace aurora


