#include "vk_loader.h"
#include "stb_image.h"
#include "aurora_pch.h"
#include "engine.h"
#include "vk_initialisers.h"
#include "vk_types.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>

#include "logger.h"

namespace Aurora
{

//FIXME: errors print error message -> string
std::optional<std::vector<std::shared_ptr<MeshAsset>>> load_gltf_meshes(Engine* engine, std::filesystem::path filepath)
{
  AR_CORE_INFO("Loading GLTF File at {}", filepath.c_str());
  fastgltf::Parser parser;
  auto data = fastgltf::GltfDataBuffer::FromPath(filepath);
  
  if (data.error() != fastgltf::Error::None)
  {
    AR_CORE_ERROR("Error loading GLTF File!");
    return {};
  }

  auto asset = parser.loadGltf(data.get(), filepath.parent_path(), fastgltf::Options::LoadExternalBuffers);
  if (auto error = asset.error(); error != fastgltf::Error::None)
  {
    AR_CORE_ERROR("Error parsing GLTF File!");
    return {};
  }

  std::vector<std::shared_ptr<MeshAsset>> meshes;

  std::vector<uint32_t> indices;
  std::vector<Vertex> vertices;
  
  for (fastgltf::Mesh& mesh : asset->meshes)
  {
    MeshAsset newmesh;
    newmesh.name = mesh.name;
    
    indices.clear();
    vertices.clear();
    
    for (auto&& p : mesh.primitives)
    {
      GeoSurface newSurface;
      newSurface.startIndex = static_cast<uint32_t>(indices.size());
      newSurface.count = static_cast<uint32_t>(asset->accessors[p.indicesAccessor.value()].count);
      
      size_t initial_vtx = vertices.size();
      
      // load indices
      {
        fastgltf::Accessor& indexaccessor = asset->accessors[p.indicesAccessor.value()];
        indices.reserve(indices.size() + indexaccessor.count);

        fastgltf::iterateAccessor<std::uint32_t>(asset.get(), indexaccessor,
          [&](std::uint32_t idx) {
            indices.push_back(idx + initial_vtx);
        });
      }

      // load vertices
      {
        fastgltf::Accessor& posAccessor = asset->accessors[p.findAttribute("POSITION")->accessorIndex];
        vertices.resize(vertices.size() + posAccessor.count);
        
        // load vertex positions!
        fastgltf::iterateAccessorWithIndex<glm::vec3>(asset.get(), posAccessor, 
        [&](glm::vec3 v, size_t index) {
          Vertex newvtx;
          newvtx.position = v;
          newvtx.normal = {1, 0, 0};
          newvtx.color = glm::vec4 {1.0f};
          newvtx.uv_y = 0;
          newvtx.uv_x = 0;
          vertices[initial_vtx + index] = newvtx;
        });
      } 
        // load vertex normals..
      auto normals = p.findAttribute("NORMAL");
      if (normals != p.attributes.end())
      {
        //FIXME: do other iterateAccesor like this...
        auto lambda = [&](glm::vec3 v, size_t index) {
          vertices[initial_vtx + index].normal = v; 
        };
        fastgltf::iterateAccessorWithIndex<glm::vec3>(asset.get(), asset->accessors[(*normals).accessorIndex], lambda);
      }

        // load uvs
      auto uv = p.findAttribute("TEXCOORD_0");
      if (uv != p.attributes.end())
      {
        auto lambda = [&](glm::vec2 v, size_t index) {
          vertices[initial_vtx + index].uv_x = v.x;
          vertices[initial_vtx + index].uv_y = v.y;
        };
        fastgltf::iterateAccessorWithIndex<glm::vec2>(asset.get(), asset->accessors[(*uv).accessorIndex], lambda);
      }

      auto colors = p.findAttribute("COLOR_O");
      if (colors != p.attributes.end())
      {
        auto lambda = [&](glm::vec4 v, size_t index) {
          vertices[initial_vtx + index].color = v;
        };

        fastgltf::iterateAccessorWithIndex<glm::vec4>(asset.get(), asset->accessors[(*colors).accessorIndex], lambda);
      }

      newmesh.surfaces.push_back(newSurface);

    }

    constexpr bool OverrideColors = false;
    if (OverrideColors)
    {
      for (Vertex& vtx : vertices)
      {
        vtx.color = glm::vec4(vtx.normal, 1.0f);
      }
    }

    newmesh.meshBuffers = engine->upload_mesh(vertices, indices);
    meshes.emplace_back(std::make_shared<MeshAsset>(std::move(newmesh)));

  }
  
  return meshes;
}


} // namespace aurora
