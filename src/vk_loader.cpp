#include "vk_loader.h"
#include "aurora_pch.h"
#include "engine.h"
#include "vk_initialisers.h"
#include "vk_types.h"

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/math.hpp>
#include <fastgltf/types.hpp>

#include "logger.h"

#include "ar_asserts.h"

#include "vk_images.h"
#include "stb_image.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

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
      
  constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble | fastgltf::Options::LoadExternalBuffers;
  fastgltf::Parser parser;
  auto data = fastgltf::GltfDataBuffer::FromPath(filepath);
  
  if (data.error() != fastgltf::Error::None)
  {
    AR_CORE_ERROR("Error loading GLTF File!");
    return {};
  }

  auto asset_exp = parser.loadGltf(data.get(), filepath.parent_path(), gltfOptions);

  if (auto error = asset_exp.error(); error != fastgltf::Error::None)
  {
    AR_CORE_ERROR("Error parsing GLTF File!");
    return {};
  }
  fastgltf::Asset& asset = asset_exp.get();
  
  std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = {
    {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
    {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
  };

  file.descriptorPool.init(engine->m_Device.logical, asset.materials.size(), sizes);
  

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
    vkCreateSampler(engine->m_Device.logical, &sampl, nullptr, &newSampler);
    file.samplers.push_back(newSampler);
  }

  std::vector<std::shared_ptr<MeshAsset>> meshes;
  std::vector<std::shared_ptr<Node>> nodes;
  std::vector<AllocatedImage> images;
  std::vector<std::shared_ptr<GLTFMaterial>> materials;

  //for (fastgltf::Image& image : asset->images)
  //{
  //  images.push_back(engine->m_ErrorCheckerboardImage);
  //}
  
  for (fastgltf::Image& image : asset.images)
  {
       std::optional<AllocatedImage> img = load_image(engine, asset, image);
    if (img.has_value())
    {
      images.push_back(*img);
      file.images[image.name.c_str()] = *img;
    }
    else
    {
      images.push_back(engine->m_ErrorCheckerboardImage);
      AR_CORE_WARN("gltf failed to load a texture!");
    }
  }
  


  file.materialDataBuffer = engine->create_buffer(sizeof(GLTFMetallic_Roughness::MaterialConstants) * asset.materials.size(), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
  int data_index = 0;
  GLTFMetallic_Roughness::MaterialConstants* sceneMaterialConstants = (GLTFMetallic_Roughness::MaterialConstants*) file.materialDataBuffer.info.pMappedData;
  
  for (fastgltf::Material& mat : asset.materials)
  {
    std::shared_ptr<GLTFMaterial> newMat = std::make_shared<GLTFMaterial>();
    materials.push_back(newMat);
    file.materials[mat.name.c_str()] = newMat;
    
    GLTFMetallic_Roughness::MaterialConstants constants;
    constants.colorFactors.x = mat.pbrData.baseColorFactor[0];
    constants.colorFactors.y = mat.pbrData.baseColorFactor[1];
    constants.colorFactors.z = mat.pbrData.baseColorFactor[2];
    constants.colorFactors.w = mat.pbrData.baseColorFactor[3];
    
    constants.metal_rough_factors.x = mat.pbrData.metallicFactor;
    constants.metal_rough_factors.y = mat.pbrData.roughnessFactor;

    sceneMaterialConstants[data_index] = constants;

    MaterialPass passType = MaterialPass::MainColour;
    if (mat.alphaMode == fastgltf::AlphaMode::Blend)
    {
      passType = MaterialPass::Transparent;
    }

    GLTFMetallic_Roughness::MaterialResources materialResources;
    materialResources.colorImage = engine->m_WhiteImage;
    materialResources.colorSampler = engine->m_DefaultSamplerLinear;
    materialResources.metalRoughImage = engine->m_WhiteImage;
    materialResources.metalRoughSampler = engine->m_DefaultSamplerLinear;

    materialResources.dataBuffer = file.materialDataBuffer.buffer;
    materialResources.dataBufferOffset = data_index * sizeof(GLTFMetallic_Roughness::MaterialConstants);

    if (mat.pbrData.baseColorTexture.has_value())
    {
      size_t img = asset.textures[mat.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
      size_t sampler = asset.textures[mat.pbrData.baseColorTexture.value().textureIndex].samplerIndex.value();

      materialResources.colorImage = images[img];
      materialResources.colorSampler = file.samplers[sampler];
    }

    newMat->data = engine->metalRoughMaterial.write_material(engine->m_Device.logical, passType, materialResources, file.descriptorPool);
    data_index++;
  }
  
  std::vector<uint32_t> indices;
  std::vector<Vertex> vertices;

  for(fastgltf::Mesh& mesh : asset.meshes)
  {
    std::shared_ptr<MeshAsset> newmesh = std::make_shared<MeshAsset>();
    meshes.push_back(newmesh);
    file.meshes[mesh.name.c_str()] = newmesh;
    newmesh->name = mesh.name;
    
    indices.clear();
    vertices.clear();

    for (auto&& p : mesh.primitives)
    {
      GeoSurface newSurface;
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
        
        auto lambda = [&](glm::vec3 v, size_t index) {
          Vertex newvtx;
          newvtx.position = v;
          newvtx.normal = {1, 0, 0};
          newvtx.color = glm::vec4{1.0f};
          newvtx.uv_x = 0;
          newvtx.uv_y = 0;
          vertices[initial_vtx + index] = newvtx;
        };

        fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, posAccessor, lambda);
      }

      auto normals = p.findAttribute("NORMAL");
      if (normals != p.attributes.end())
      {
        auto lambda = [&](glm::vec3 v, size_t index) {
          vertices[initial_vtx + index].normal = v;
        };

         fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, asset.accessors[(*normals).accessorIndex], lambda);
      }

      auto uv = p.findAttribute("TEXCOORD_0");
      if (uv != p.attributes.end())
      {
        auto lambda = [&](glm::vec2 v, size_t index) {
          vertices[initial_vtx + index].uv_x = v.x;
          vertices[initial_vtx + index].uv_y = v.y;
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
        newSurface.material = materials[p.materialIndex.value()];
      }
      else
      {
        newSurface.material = materials[0];
      }
      
      
      glm::vec3 minpos = vertices[initial_vtx].position;
      glm::vec3 maxpos = vertices[initial_vtx].position;
      for (int i = initial_vtx; i < vertices.size(); i++)
      {
        minpos = glm::min(minpos, vertices[i].position);
        maxpos = glm::max(maxpos, vertices[i].position);
      }

      newSurface.bounds.origin = (maxpos + minpos) / 2.0f;
      newSurface.bounds.extents = (maxpos-minpos) / 2.0f;
      newSurface.bounds.sphereRadius = glm::length(newSurface.bounds.extents);

      newmesh->surfaces.push_back(newSurface);
    }
    newmesh->meshBuffers = engine->upload_mesh(vertices, indices);
  }
  
  // now we load the nodes?
  
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

    std::visit(fastgltf::visitor {
    [&](fastgltf::math::fmat4x4 matrix) {
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

void LoadedGLTF::draw(const glm::mat4& topMatrix, DrawContext& ctx)
{
  for (auto& n : topNodes)
  {
    n->draw(topMatrix, ctx);
  }
}

void LoadedGLTF::clearAll()
{
  // cleanup gpu memory
  VkDevice dv = creator->m_Device.logical;

  descriptorPool.destroy_pools(dv);
  creator->destroy_buffer(materialDataBuffer);

  for (auto& [k, v] : meshes)
  {
    creator->destroy_buffer(v->meshBuffers.indexBuffer);
    creator->destroy_buffer(v->meshBuffers.vertexBuffer);
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
    vkDestroySampler(dv, sampler, nullptr);
  }

}

std::optional<AllocatedImage> load_image(Engine* engine, fastgltf::Asset& asset, fastgltf::Image& image)
{
    AllocatedImage newImage {};

    int width, height, nrChannels;

    std::visit(
        fastgltf::visitor {
            [](auto& arg) {},
            [&](fastgltf::sources::URI& filePath) {
                AR_CORE_ERROR("from filepath!");
                assert(filePath.fileByteOffset == 0); // We don't support offsets with stbi.
                assert(filePath.uri.isLocalPath()); // We're only capable of loading
                                                    // local files.

                const std::string path(filePath.uri.path().begin(),
                    filePath.uri.path().end()); // Thanks C++.
                unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 4);
                if (data) {
                    VkExtent3D imagesize;
                    imagesize.width = width;
                    imagesize.height = height;
                    imagesize.depth = 1;

                    newImage = engine->create_image(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT,false);

                    stbi_image_free(data);
                }
            },
            [&](fastgltf::sources::Vector& vector) {
                AR_CORE_ERROR("vector load");
                unsigned char* data = stbi_load_from_memory((unsigned char*)vector.bytes.data(), static_cast<int>(vector.bytes.size()),
                    &width, &height, &nrChannels, 4);
                if (data) {
                    VkExtent3D imagesize;
                    imagesize.width = width;
                    imagesize.height = height;
                    imagesize.depth = 1;

                    newImage = engine->create_image(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT,false);

                    stbi_image_free(data);
                }
            },
            [&](fastgltf::sources::BufferView& view) {
                auto& bufferView = asset.bufferViews[view.bufferViewIndex];
                auto& buffer = asset.buffers[bufferView.bufferIndex];
                std::visit(fastgltf::visitor { // We only care about VectorWithMime here, because we
                                               // specify LoadExternalBuffers, meaning all buffers
                                               // are already loaded into a vector.
                               [](auto& arg) {
                                },
                                // FIXME: in the tutorial this was fastgltf::sources::Vector because they asserted it would be casue of the options
                                // using fastlgltf::sources::Array works for me now but might cause other problems,, keep in mind!
                               [&](fastgltf::sources::Array& vector) {
                                   unsigned char* data = stbi_load_from_memory((unsigned char*)(vector.bytes.data() + bufferView.byteOffset),
                                       static_cast<int>(bufferView.byteLength),
                                       &width, &height, &nrChannels, 4);
                                   if (data) {
                                       VkExtent3D imagesize;
                                       imagesize.width = width;
                                       imagesize.height = height;
                                       imagesize.depth = 1;

                                       newImage = engine->create_image(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM,
                                           VK_IMAGE_USAGE_SAMPLED_BIT,false);

                                       stbi_image_free(data);
                                   }
                               } },
                    buffer.data);
            },
        },
        image.data);

    // if any of the attempts to load the data failed, we havent written the image
    // so handle is null
    if (newImage.image == VK_NULL_HANDLE) {
      AR_CORE_ERROR("NULL  IMAGE LOADED---");
        return {};
    } else {
        return newImage;
    }
}
} // namespace aurora
