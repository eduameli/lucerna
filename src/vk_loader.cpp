#include "vk_loader.h"
#include "aurora_pch.h"
#include "cvars.h"
#include "engine.h"
#include "vk_initialisers.h"
#include "vk_types.h"

#include <cstdint>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/math.hpp>
#include <fastgltf/types.hpp>

#include "logger.h"

#include "ar_asserts.h"

#include "vk_images.h"
#include "stb_image.h"

#include <filesystem>

#include <volk.h>
#include "vk_pipelines.h"

namespace Aurora

{

glm::vec2 octahedron_wrap(glm::vec2 v) {
	glm::vec2 w = 1.0f - glm::abs(glm::vec2(v.y, v.x));
	if (v.x < 0.0f) w.x = -w.x;
	if (v.y < 0.0f) w.y = -w.y;
	return w;
}

glm::vec2 enconde_normal(glm::vec3 n) {
	n /= (glm::abs(n.x) + glm::abs(n.y) + glm::abs(n.z));
	n = glm::vec3(n.z > 0.0f ? glm::vec2(n.x, n.y) : octahedron_wrap(glm::vec2(n.x, n.y)), n.z);
	n = glm::vec3(glm::vec2(n.x, n.y) * 0.5f + 0.5f, n.z);

	return glm::vec2(n.x, n.y);
}

// FIXME: repeated vertex info buffers if meshes r repeated...
std::optional<std::shared_ptr<LoadedGLTF>> load_gltf(Engine* engine, std::filesystem::path filepath)
{
  AR_CORE_INFO("Loading GLTF at {}", filepath.c_str());

  std::shared_ptr<LoadedGLTF> scene = std::make_shared<LoadedGLTF>();
  scene->creator = engine;
  LoadedGLTF& file = *scene.get();
      
  constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble | fastgltf::Options::LoadExternalBuffers;
  fastgltf::Parser parser(fastgltf::Extensions::KHR_materials_emissive_strength);
  auto data = fastgltf::GltfDataBuffer::FromPath(filepath);
  
  if (data.error() != fastgltf::Error::None)
  {
    AR_CORE_ERROR("Error loading GLTF File! 1");
    return {};
  }

  auto asset_exp = parser.loadGltf(data.get(), filepath.parent_path(), gltfOptions);

  if (auto error = asset_exp.error(); error != fastgltf::Error::None)
  {
    AR_CORE_ERROR("Error parsing GLTF File! 2");
    return {};
  }

  fastgltf::Asset& asset = asset_exp.get();
  

  for (fastgltf::Sampler& sampler : asset.samplers)
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
    vkCreateSampler(engine->device, &sampl, nullptr, &newSampler);


    VkWriteDescriptorSet w{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = nullptr};
    w.dstSet = engine->bindless_descriptor_set;
    w.descriptorCount = 1;
    w.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    w.dstArrayElement = engine->samplerCounter;
    w.dstBinding = engine->SAMPLER_BINDING;

    VkDescriptorImageInfo info{};
    info.sampler = newSampler;
    w.pImageInfo = &info;
    vkUpdateDescriptorSets(engine->device, 1, &w, 0, nullptr);

    engine->samplerCounter++;
    file.samplers.push_back(newSampler);

    
   
  }

  std::vector<std::shared_ptr<MeshAsset>> meshes;
  std::vector<std::shared_ptr<Node>> nodes;
  std::vector<AllocatedImage> images;
  std::vector<std::shared_ptr<BindlessMaterial>> mats;
  
  for (fastgltf::Image& image : asset.images)
  {
    std::optional<AllocatedImage> img = load_image(engine, asset, image, filepath);
    if (img.has_value())
    {
      images.push_back(*img);
      file.images[image.name.c_str()] = *img;
    }
    else
    {
      images.push_back(engine->m_ErrorCheckerboardImage);
      AR_CORE_WARN("Failed to load a texture from GLTF, (using placeholder)");
    }
  }

  std::vector<uint32_t> mat_idxs;
  
  for (fastgltf::Material& mat : asset.materials)
  {
    BindlessMaterial m;
    if (mat.pbrData.baseColorTexture.has_value())
    {
      
      size_t img = asset.textures[mat.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
      size_t sampler = asset.textures[mat.pbrData.baseColorTexture.value().textureIndex].samplerIndex.value();

      AllocatedImage i  = images[img];

      m.albedo = i.texture_idx + (sampler << 24);
     
    }
    m.modulate = {mat.pbrData.baseColorFactor.x(), mat.pbrData.baseColorFactor.y(), mat.pbrData.baseColorFactor.z()};

    m.emissions = {glm::vec3(mat.emissiveFactor.x(), mat.emissiveFactor.y(), mat.emissiveFactor.z())};
    m.strength = mat.emissiveStrength;

    mat_idxs.push_back(engine->mainDrawContext.materials.size());
    engine->mainDrawContext.materials.push_back(m);
  }


  
  std::vector<uint32_t>& indices = engine->mainDrawContext.indices;
  std::vector<Vertex>& vertices = engine->mainDrawContext.vertices;
  std::vector<glm::vec3>& positions = engine->mainDrawContext.positions;

  uint32_t idx_counter = 0;
  
  for(fastgltf::Mesh& mesh : asset.meshes)
  {
    std::shared_ptr<MeshAsset> newmesh = std::make_shared<MeshAsset>();
    meshes.push_back(newmesh);
    file.meshes[mesh.name.c_str()] = newmesh;
    newmesh->name = mesh.name;
    
    for (auto&& p : mesh.primitives)
    {
      GeoSurface newSurface{};
      newSurface.startIndex = static_cast<uint32_t>(indices.size());
      newSurface.count = static_cast<uint32_t>(asset.accessors[p.indicesAccessor.value()].count);

      size_t initial_vtx = vertices.size();
      
      {
        fastgltf::Accessor& indexaccessor = asset.accessors[p.indicesAccessor.value()];
        indices.reserve(indices.size() + indexaccessor.count);

        fastgltf::iterateAccessor<std::uint32_t>(asset, indexaccessor,
          [&](std::uint32_t idx) {
              indices.push_back(idx + initial_vtx);
          });
      }


      {
        fastgltf::Accessor& posAccessor = asset.accessors[p.findAttribute("POSITION")->accessorIndex];
        vertices.resize(vertices.size() + posAccessor.count);
        positions.resize(positions.size() + posAccessor.count);
        
        auto lambda = [&](glm::vec3 v, size_t index) {
          Vertex newvtx;
          newvtx.normal_uv = {1, 0, 0, 0};
          newvtx.color = glm::vec4{1.0f};
          vertices[initial_vtx + index] = newvtx;
          positions[initial_vtx + index] = v;
        };

        fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, posAccessor, lambda);
      }

      auto normals = p.findAttribute("NORMAL");
      if (normals != p.attributes.end())
      {
        auto lambda = [&](glm::vec3 v, size_t index) {
          glm::vec2 normal = enconde_normal(v);
          vertices[initial_vtx + index].normal_uv = {normal.x, normal.y, 0, 0};
        };

         fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, asset.accessors[(*normals).accessorIndex], lambda);
      }

      auto uv = p.findAttribute("TEXCOORD_0");
      if (uv != p.attributes.end())
      {
        auto lambda = [&](glm::vec2 v, size_t index) {
          vertices[initial_vtx + index].normal_uv.z = v.x;
          vertices[initial_vtx + index].normal_uv.w = v.y;
        };

         fastgltf::iterateAccessorWithIndex<glm::vec2>(asset, asset.accessors[(*uv).accessorIndex], lambda);
      }

      auto colors = p.findAttribute("COLOR_O");
      if (colors != p.attributes.end())
      {
        auto lambda = [&](glm::vec4 v, size_t index) {
          vertices[initial_vtx + index].color = v;
        };

         fastgltf::iterateAccessorWithIndex<glm::vec4>(asset, asset.accessors[(*colors).accessorIndex], lambda);
      }

      if (p.materialIndex.has_value())
      {
        newSurface.mat_idx = mat_idxs[p.materialIndex.value()];
      }
      else
      {
        // use placeholder
        newSurface.mat_idx = mat_idxs[0];
      }

      glm::vec3  minpos = positions[initial_vtx];
      glm::vec3  maxpos = positions[initial_vtx];
      for (int i = initial_vtx; i < positions.size(); i++) {
          minpos = glm::min(minpos, positions[i]);
          maxpos = glm::max(maxpos, positions[i]);
      }

      newSurface.bounds.origin = (maxpos + minpos) / 2.f;
      newSurface.bounds.extents = (maxpos - minpos) / 2.f;
      newSurface.bounds.sphereRadius = glm::length(newSurface.bounds.extents);
      // calculate origin and extents from the min/max, use extent lenght for radius
      newmesh->surfaces.push_back(newSurface);
      
    }
  }



  for (fastgltf::Node& node : asset.nodes)
  {
    std::shared_ptr<Node> newNode;

    if (node.meshIndex.has_value())
    {
      newNode = std::make_shared<MeshNode>();
      static_cast<MeshNode*>(newNode.get())->mesh = meshes[*node.meshIndex];
    }
    else
    {
      newNode = std::make_shared<Node>();
    }

    nodes.push_back(newNode);
    file.nodes[node.name.c_str()];


    std::visit(fastgltf::visitor { [&](fastgltf::math::fmat4x4 matrix) {
                                      memcpy(&newNode->localTransform, matrix.data(), sizeof(matrix));
                                  },
                   [&](fastgltf::TRS transform) {
                       glm::vec3 tl(transform.translation[0], transform.translation[1],
                           transform.translation[2]);
                       glm::quat rot(transform.rotation[3], transform.rotation[0], transform.rotation[1],
                           transform.rotation[2]);
                       glm::vec3 sc(transform.scale[0], transform.scale[1], transform.scale[2]);

                       glm::mat4 tm = glm::translate(glm::mat4(1.f), tl);
                       glm::mat4 rm = glm::toMat4(rot);
                       glm::mat4 sm = glm::scale(glm::mat4(1.f), sc);

                       newNode->localTransform = tm * rm * sm;
                   } },
        node.transform);
    
  }
 

  for (int i = 0; i < asset.nodes.size(); i++)
  {
    fastgltf::Node& node = asset.nodes[i];
    std::shared_ptr<Node>& sceneNode = nodes[i];
    
    for (auto& c : node.children)
    {
      sceneNode->children.push_back(nodes[c]);
      nodes[c]->parent = sceneNode;
    }
  }
  
  for (auto& node : nodes)
  {
    if (node->parent.lock() == nullptr)
    {
      file.topNodes.push_back(node);
      node->refresh_transform(glm::mat4{1.0f});
    }
  }

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

void LoadedGLTF::queue_draw(const glm::mat4& topMatrix, DrawContext& ctx)
{
  for (auto& n : topNodes)
  {
    n->queue_draw(topMatrix, ctx);
  }
}

void LoadedGLTF::clearAll()
{
  // cleanup gpu memory

  for (auto& [k, v] : meshes)
  {
    creator->destroy_buffer(v->meshBuffers.indexBuffer);
    creator->destroy_buffer(v->meshBuffers.vertexBuffer);
    creator->destroy_buffer(v->meshBuffers.positionBuffer);
  }

  for (auto& [k, v] : images)
  {
    if (v.image == creator->m_ErrorCheckerboardImage.image)
    {
      // dont destroy default placeholder image
      continue;
    }
    creator->destroy_image(v);
  }
  
  for (auto& sampler : samplers)
  {
    vkDestroySampler(creator->device, sampler, nullptr);
  }

}

std::optional<AllocatedImage> load_image(Engine* engine, fastgltf::Asset& asset, fastgltf::Image& image, std::filesystem::path fpath)
{

  AllocatedImage newImage{};
  int width, height, nrChannels;

  std::visit(
    fastgltf::visitor{
        [](auto &arg) {
          AR_CORE_INFO("bruh");
        },
        [&](fastgltf::sources::URI &filePath) {
          assert(filePath.fileByteOffset ==
                 0); // We don't support offsets with stbi.
          assert(filePath.uri.isLocalPath()); // We're only capable of loading
                                              // local files.

          // const std::string path(filePath.uri.path().begin(),
          //                        filePath.uri.path().end()); // Thanks C++.

          // FIXME: handle loading dds or ktx textures here no mips.
          
          std::string path = fpath.parent_path().append(filePath.uri.string());
          // path.pop_back();
          // path.pop_back();
          // path.pop_back();
          // path.push_back('p');
          // path.push_back('n');
          // path.push_back('g');

          
          unsigned char *data =
              stbi_load(path.c_str(), &width, &height, &nrChannels, 4);
          if (data) {
            AR_CORE_INFO("LOADED IMG!!!");
            VkExtent3D imagesize;
            imagesize.width = width;
            imagesize.height = height;
            imagesize.depth = 1;

            newImage = engine->create_image(
                data, imagesize, VK_FORMAT_R8G8B8A8_UNORM,
                VK_IMAGE_USAGE_SAMPLED_BIT, true);

            stbi_image_free(data);
          }
        },
        [&](fastgltf::sources::Vector &vector) {
          AR_CORE_ERROR("vector load");
          unsigned char *data =
              stbi_load_from_memory((unsigned char *)vector.bytes.data(),
                                    static_cast<int>(vector.bytes.size()),
                                    &width, &height, &nrChannels, 4);
          if (data) {
            VkExtent3D imagesize;
            imagesize.width = width;
            imagesize.height = height;
            imagesize.depth = 1;

            newImage = engine->create_image(
                data, imagesize, VK_FORMAT_R8G8B8A8_UNORM,
                VK_IMAGE_USAGE_SAMPLED_BIT, true);

            stbi_image_free(data);
          }
        },
        [&](fastgltf::sources::BufferView &view) {
          auto &bufferView = asset.bufferViews[view.bufferViewIndex];
          auto &buffer = asset.buffers[bufferView.bufferIndex];
          std::visit(
              fastgltf::visitor{
                  // We only care about VectorWithMime here, because we
                  // specify LoadExternalBuffers, meaning all buffers
                  // are already loaded into a vector.
                  [](auto &arg) {
                    AR_LOG_ASSERT(
                        false,
                        "GLTF Loading only supports GLB format");
                  },
                  // FIXME: in the tutorial this was fastgltf::sources::Vector
                  // because they asserted it would be casue of the options
                  // using fastlgltf::sources::Array works for me now but
                  // might cause other problems,, keep in mind!
                  [&](fastgltf::sources::Array &vector) {
                    unsigned char *data = stbi_load_from_memory(
                        (unsigned char *)(vector.bytes.data() +
                                          bufferView.byteOffset),
                        static_cast<int>(bufferView.byteLength), &width,
                        &height, &nrChannels, 4);
                    if (data) {
                      VkExtent3D imagesize;
                      imagesize.width = width;
                      imagesize.height = height;
                      imagesize.depth = 1;

                      newImage = engine->create_image(
                          data, imagesize, VK_FORMAT_R8G8B8A8_UNORM,
                          VK_IMAGE_USAGE_SAMPLED_BIT, true);

                      stbi_image_free(data);
                    }
                  }},
              buffer.data);
        },
    },
    image.data
  );

  if (newImage.image == VK_NULL_HANDLE) {
    AR_CORE_ERROR("[resource] Loaded image is VK_NULL_HANDLE");
    return {};
  } else {
    return newImage;
  }
}

void MeshNode::queue_draw(const glm::mat4& topMatrix, DrawContext& ctx)
{
  glm::mat4 nodeMatrix = topMatrix * worldTransform;
  int mesh_idx = ctx.transforms.size();
  ctx.transforms.push_back(glm::mat4x3(nodeMatrix));

  for (auto& s : mesh->surfaces)
  {
    ctx.draw_datas.push_back({
      .material_idx = s.mat_idx,
      .transform_idx = (uint32_t) mesh_idx,
      .indexCount = s.count,
      .firstIndex = s.startIndex,
    });

    ctx.bounds.push_back(s.bounds);
  }

  Node::queue_draw(topMatrix, ctx);
}

} // namespace aurora
