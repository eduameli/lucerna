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
  AR_CORE_INFO("Loading Meshes from GLTF File at {}", filepath.c_str());
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


std::optional<std::shared_ptr<LoadedGLTF>> load_gltf(Engine* engine, std::filesystem::path filepath)
{
  AR_CORE_INFO("Loading GLTF File at {}", filepath.c_str());

  std::shared_ptr<LoadedGLTF> scene = std::make_shared<LoadedGLTF>();
  scene->creator = engine;
  LoadedGLTF& file = *scene.get();
  

  constexpr auto gltfOptions = 
    fastgltf::Options::DontRequireValidAssetMember |
    fastgltf::Options::AllowDouble | 
    fastgltf::Options::LoadExternalImages;
  
  AR_CORE_INFO("Loading GLTF File at {}", filepath.c_str());
  fastgltf::Parser parser;
  auto data = fastgltf::GltfDataBuffer::FromPath(filepath);
  
  if (data.error() != fastgltf::Error::None)
  {
    AR_CORE_ERROR("Error loading GLTF File!");
    return {};
  }

  auto asset = parser.loadGltf(data.get(), filepath.parent_path(), gltfOptions);
  if (auto error = asset.error(); error != fastgltf::Error::None)
  {
    AR_CORE_ERROR("Error parsing GLTF File!");
    return {};
  }

  
  std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = {
    {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
    {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
  };

  file.descriptorPool.init(engine->m_Device.logical, asset->materials.size(), sizes);
  

  for (fastgltf::Sampler& sampler : asset->samplers)
  {
    VkSamplerCreateInfo sampl{};
    sampl.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampl.pNext = nullptr;
    sampl.maxLod = VK_LOD_CLAMP_NONE;
    sampl.minLod = 0;
    sampl.magFilter = extract_filter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
    sampl.minFilter = extract_filter(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

    sampl.mipmapMode = extract_mipmap_mode(sampler.minFilter.value_or(fastgltf::Filter::Nearest));
    
    VkSampler newSampler;
    vkCreateSampler(engine->m_Device.logical, &sampl, nullptr, &newSampler);
    file.samplers.push_back(newSampler);
  }

  // adapt to new api
  return scene;
}

VkFilter extract_filter(fastgltf::Filter filter)
{
  switch (filter)
  {
    case fastgltf::Filter::Nearest:
    case fastgltf::Filter::NearestMipMapNearest:
    case fastgltf::Filter::NearestMipMapLinear:
      return VK_FILTER_NEAREST;
    case fastgltf::Filter::Linear:
    case fastgltf::Filter::LinearMipMapNearest:
    case fastgltf::Filter::LinearMipMapLinear:
    default:
      return VK_FILTER_LINEAR;
  }
}

VkSamplerMipmapMode extract_mipmap_mode(fastgltf::Filter filter)
{
  switch (filter)
  {
    case fastgltf::Filter::NearestMipMapNearest:
    case fastgltf::Filter::LinearMipMapNearest:
      return VK_SAMPLER_MIPMAP_MODE_NEAREST;
    case fastgltf::Filter::NearestMipMapLinear:
    case fastgltf::Filter::LinearMipMapLinear:
    default:
      return VK_SAMPLER_MIPMAP_MODE_LINEAR;
  }
}

} // namespace aurora
