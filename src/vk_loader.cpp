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
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include "vk_pipelines.h"
#include "glm/gtx/string_cast.hpp"
#include "glm/packing.hpp"
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


  std::filesystem::path current_path = std::filesystem::current_path();

  // std::filesystem::current_path(filepath.parent_path());
  

  std::shared_ptr<LoadedGLTF> scene = std::make_shared<LoadedGLTF>();
  scene->creator = engine;
  LoadedGLTF& file = *scene.get();
      
  constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble | fastgltf::Options::LoadExternalBuffers;
  fastgltf::Parser parser;
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
  
  std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = {
    {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
    {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
  };
  file.descriptorPool.init(engine->device, asset.materials.size(), sizes);
  

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
    file.samplers.push_back(newSampler);
  }

  std::vector<std::shared_ptr<MeshAsset>> meshes;
  std::vector<std::shared_ptr<Node>> nodes;
  std::vector<AllocatedImage> images;
  std::vector<std::shared_ptr<GLTFMaterial>> materials;
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
  
  file.materialDataBuffer = engine->create_buffer(sizeof(GLTFMetallic_Roughness::MaterialConstants) * asset.materials.size(), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
  int data_index = 0;
  GLTFMetallic_Roughness::MaterialConstants* sceneMaterialConstants = (GLTFMetallic_Roughness::MaterialConstants*) file.materialDataBuffer.info.pMappedData;

  std::vector<uint32_t> mat_idxs;
  
  for (fastgltf::Material& mat : asset.materials)
  {
    //bindless material start
    BindlessMaterial m;
    // AR_CORE_WARN("material...");
    if (mat.pbrData.baseColorTexture.has_value())
    {
      
      size_t img = asset.textures[mat.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
      size_t sampler = asset.textures[mat.pbrData.baseColorTexture.value().textureIndex].samplerIndex.value();

      AllocatedImage i  = images[img];
      VkSampler s = file.samplers[sampler];


      m.albedo_idx = i.sampler_idx;
    }

    m.colorTint = {mat.pbrData.baseColorFactor.x(), mat.pbrData.baseColorFactor.y(), mat.pbrData.baseColorFactor.z()};

    
    uint32_t mat_idx = engine->mainDrawContext.freeMaterials.allocate();
    mat_idxs.push_back(mat_idx);
    engine->mainDrawContext.materials[mat_idx] = m;

    // bindless material end

    
    std::shared_ptr<GLTFMaterial> newMat = std::make_shared<GLTFMaterial>();
    materials.push_back(newMat);
    file.materials[mat.name.c_str()] = newMat;
    
    GLTFMetallic_Roughness::MaterialConstants constants;
    constants.colorFactors.x = mat.pbrData.baseColorFactor[0];
    constants.colorFactors.y = mat.pbrData.baseColorFactor[1];
    constants.colorFactors.z = mat.pbrData.baseColorFactor[2];
    constants.colorFactors.w = mat.pbrData.baseColorFactor[3];
    

    constants.metal_rough_factors.x = mat.pbrData.metallicFactor * 10.0f;
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

    newMat->data = engine->metalRoughMaterial.write_material(engine->device, passType, materialResources, file.descriptorPool);

    
    newMat->data.albedo_idx = materialResources.colorImage.sampler_idx;
    engine->combined_sampler[materialResources.colorImage.sampler_idx] = materialResources.colorSampler;
    // queue_descriptor_update(descriptor_write, optional sampler)
    // at the end of the frame call updates
    
    data_index++;
  }


  // update descriptor set here .. with the sampler!
  // for images do it 
  
  
  std::vector<uint32_t> indices;
  std::vector<Vertex> vertices;
  std::vector<glm::vec3> positions;

  uint32_t idx_counter = 0;
  
  for(fastgltf::Mesh& mesh : asset.meshes)
  {
    std::shared_ptr<MeshAsset> newmesh = std::make_shared<MeshAsset>();
    meshes.push_back(newmesh);
    file.meshes[mesh.name.c_str()] = newmesh;
    newmesh->name = mesh.name;
    
    // indices.clear();
    // vertices.clear();
    // positions.clear();

    // AR_CORE_WARN("NEW MESH");

    uint32_t initals_idx = 0;
    
    for (auto&& p : mesh.primitives)
    {
      // AR_CORE_ERROR("new surface!");
      
      GeoSurface newSurface{};
      newSurface.startIndex = static_cast<uint32_t>(indices.size());
      newSurface.count = static_cast<uint32_t>(asset.accessors[p.indicesAccessor.value()].count);

      size_t initial_vtx = vertices.size();
      // AR_CORE_WARN("inital vtx {}", initial_vtx);
      
      {
        fastgltf::Accessor& indexaccessor = asset.accessors[p.indicesAccessor.value()];
        indices.reserve(indices.size() + indexaccessor.count);

        fastgltf::iterateAccessor<std::uint32_t>(asset, indexaccessor,
          [&](std::uint32_t idx) {
              indices.push_back(idx + initial_vtx);
              // AR_CORE_INFO("push idx {}", idx + initial_vtx);
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




    // for (auto i : indices)
    // {
    //   engine->mainDrawContext.indices.push_back(i + initals_idx);
    //   // AR_CORE_INFO("idx {}", i);
    // }
    // initals_idx = engine->mainDrawContext.indices.size();

    // for (auto v : vertices)
    // {
    //   engine->mainDrawContext.vertices.push_back(v);
    // }

    // for(auto p : positions)
    // {
    //   engine->mainDrawContext.positions.push_back(p);
    // }

    // per primitive
    // newmesh->meshBuffers = engine->upload_mesh(positions, vertices, indices);
  }



 for (auto i : indices)
    {
      engine->mainDrawContext.indices.push_back(i);
      // AR_CORE_INFO("idx {}", i);
    }
    // initals_idx = engine->mainDrawContext.indices.size();

    for (auto v : vertices)
    {
      // AR_
      engine->mainDrawContext.vertices.push_back(v);
    }

    for(auto p : positions)
    {
      engine->mainDrawContext.positions.push_back(p);
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


  std::filesystem::current_path(current_path);
  return scene;
}

// if it already exist dont add to material buffer just return the same idx


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
  descriptorPool.destroy_pools(creator->device);
  creator->destroy_buffer(materialDataBuffer);

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
        [](auto &arg) {},
        [&](fastgltf::sources::URI &filePath) {
          AR_CORE_ERROR("from filepath! {}", filePath.uri.c_str());
          assert(filePath.fileByteOffset ==
                 0); // We don't support offsets with stbi.
          assert(filePath.uri.isLocalPath()); // We're only capable of loading
                                              // local files.

          // const std::string path(filePath.uri.path().begin(),
          //                        filePath.uri.path().end()); // Thanks C++.


          const std::string path = fpath.parent_path().append(filePath.uri.string());

          AR_CORE_INFO("final path {}", path);
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


void GLTFMetallic_Roughness::build_pipelines(Engine* engine)
{
  VkDevice device = engine->device;

  VkShaderModule meshFragShader;
  AR_LOG_ASSERT(
    vkutil::load_shader_module("shaders/meshes/std_material.frag.spv", device, &meshFragShader),
    "Error when building the triangle fragment shader module"
  );

  VkShaderModule meshVertShader;
  AR_LOG_ASSERT(
    vkutil::load_shader_module("shaders/meshes/std_material.vert.spv", device, &meshVertShader),
    "Error when building the triangle vertex shader module"
  );

  VkPushConstantRange matrixRange{};
  matrixRange.offset = 0;
  matrixRange.size = sizeof(std_material_pcs);
  matrixRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  
  DescriptorLayoutBuilder layoutBuilder;
  layoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  layoutBuilder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  layoutBuilder.add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

  materialLayout = layoutBuilder.build(device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
 
  VkDescriptorSetLayout layouts[] = {
    engine->m_SceneDescriptorLayout,
    materialLayout,
  };

  VkPipelineLayoutCreateInfo mesh_layout_info = vkinit::pipeline_layout_create_info();
  mesh_layout_info.setLayoutCount = 2;
  mesh_layout_info.pSetLayouts = layouts;
  mesh_layout_info.pPushConstantRanges = &matrixRange;
  mesh_layout_info.pushConstantRangeCount = 1;

  VkPipelineLayout newLayout;
  VK_CHECK_RESULT(vkCreatePipelineLayout(device, &mesh_layout_info, nullptr, &newLayout));

  opaquePipeline.layout = newLayout;
  transparentPipeline.layout = newLayout;

  PipelineBuilder pipelineBuilder;
  pipelineBuilder.set_shaders(meshVertShader, meshFragShader);
  pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
  pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
  pipelineBuilder.set_cull_mode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE); // NOTE: backface culling?
  //pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
  pipelineBuilder.set_multisampling_none();
  pipelineBuilder.disable_blending();
  pipelineBuilder.enable_depthtest(true, VK_COMPARE_OP_EQUAL); // NOTE: bigger?
  

  pipelineBuilder.set_color_attachment_format(engine->m_DrawImage.imageFormat);
  pipelineBuilder.set_depth_format(engine->m_DepthImage.imageFormat);

  pipelineBuilder.PipelineLayout = newLayout;

  opaquePipeline.pipeline = pipelineBuilder.build_pipeline(device);

  pipelineBuilder.enable_blending_additive();
  pipelineBuilder.enable_depthtest(false, VK_COMPARE_OP_GREATER_OR_EQUAL);

  transparentPipeline.pipeline = pipelineBuilder.build_pipeline(device);

  vkDestroyShaderModule(device, meshFragShader, nullptr);
  vkDestroyShaderModule(device, meshVertShader, nullptr);
}

void GLTFMetallic_Roughness::clear_resources(VkDevice device)
{
  vkDestroyPipeline(device, transparentPipeline.pipeline, nullptr);
  vkDestroyPipeline(device, opaquePipeline.pipeline, nullptr);

  vkDestroyPipelineLayout(device, opaquePipeline.layout, nullptr);
  if (opaquePipeline.layout != transparentPipeline.layout) 
  {
    vkDestroyPipelineLayout(device, transparentPipeline.layout, nullptr);
  }

  vkDestroyDescriptorSetLayout(device, materialLayout, nullptr);
}

MaterialInstance GLTFMetallic_Roughness::write_material(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator)
{
  MaterialInstance matData;
  matData.passType = pass;
  if (pass == MaterialPass::Transparent)
  {
    matData.pipeline = &transparentPipeline;
  }
  else
  {
    matData.pipeline = &opaquePipeline;
  }

  matData.materialSet = descriptorAllocator.allocate(device, materialLayout);

  writer.clear();
  writer.write_buffer(0, resources.dataBuffer, sizeof(MaterialConstants), resources.dataBufferOffset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  writer.write_image(1, resources.colorImage.imageView, resources.colorSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  writer.write_image(2, resources.metalRoughImage.imageView, resources.metalRoughSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  writer.update_set(device, matData.materialSet);
  return matData;
}

void MeshNode::queue_draw(const glm::mat4& topMatrix, DrawContext& ctx)
{
  glm::mat4 nodeMatrix = topMatrix * worldTransform;

  // AR_CORE_INFO("transform: {}", glm::to_string(glm::mat4(nodeMatrix)));
  int mesh_idx = ctx.freeTransforms.allocate();
  ctx.transforms[mesh_idx] = glm::mat4x3(nodeMatrix);
  

  // this will basically go into DrawData buffer or Indirect Draw cmd buffer
  for (auto& s : mesh->surfaces)
  {
    // RenderObject def;
    // def.indexCount = s.count;
    // def.firstIndex = s.startIndex;
    // // def.indexBuffer = mesh->meshBuffers.indexBuffer.buffer;
    // // def.material = &s.material->data;
    // def.bounds = s.bounds;
    // def.transform = nodeMatrix;
    // // def.vertexBufferAddress = mesh->meshBuffers.vertexBufferAddress;
    // // def.positionBufferAddress = mesh->meshBuffers.positionBufferAddress;
    // // def.albedo_idx = 10000;
    // def.transform_idx = mesh_idx;
    // def.material_idx = s.mat_idx;
    // need to have indices here or in ssbo for indirect - but can figure that out later...
    
    // still need this to place it into the correct draw_set bucket
    // if (s.material->data.passType == MaterialPass::Transparent)
    // {
    //   ctx.TransparentSurfaces.push_back(def);
    // }
    // else
    // {
    //   ctx.OpaqueSurfaces.push_back(def);
    // }
    // ctx.OpaqueSurfaces.push_back(def);

    // AR_CORE_INFO(mesh_idx);

    uint32_t draw_idx = ctx.freeDrawData.allocate();
    ctx.draw_datas[draw_idx] = {
      .material_idx = s.mat_idx,
      .transform_idx = (uint32_t) mesh_idx,
      .indexCount = s.count,
      .firstIndex = s.startIndex,
      // .positionBDA = mesh->meshBuffers.positionBufferAddress,
      // .vertexBDA = mesh->meshBuffers.vertexBufferAddress,
    };

    // AR_CORE_INFO("first index {}", s.startIndex);

    
  }

  Node::queue_draw(topMatrix, ctx);
}

} // namespace aurora
