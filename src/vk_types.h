#pragma once
#include <cstdint>
#include <volk.h>
#include "la_asserts.h"
#include "vk_mem_alloc.h"
#include "lucerna_pch.h"
#include "logger.h"



namespace Lucerna {

  struct AllocatedImage
  {
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VkExtent3D imageExtent;
    VkFormat imageFormat;
    uint32_t image_idx{UINT32_MAX};
    uint32_t texture_idx{3};
  };
  
  struct AllocatedBuffer
  {
    VkBuffer buffer{};
    VmaAllocation allocation{};
    VmaAllocationInfo info{};
  };

  struct GPUMeshBuffers
  {
    AllocatedBuffer indexBuffer{};
    AllocatedBuffer vertexBuffer{};
    AllocatedBuffer positionBuffer{};
    VkDeviceAddress vertexBufferAddress{};
    VkDeviceAddress positionBufferAddress{};
  };



  struct GPUSceneBuffers
  {
    AllocatedBuffer indexBuffer{};
    AllocatedBuffer vertexBuffer{};
    AllocatedBuffer positionBuffer{};
    AllocatedBuffer materialBuffer{};
    AllocatedBuffer transformBuffer{};
    AllocatedBuffer drawDataBuffer{};
    AllocatedBuffer indirectCmdBuffer{};
  };


  
  
 
  // FIXME: only needed in engine?? or maybe only types that are referenced multiple times, created many times
  // for now keep here as improved deletion queue could go here! one array per type of deletion to store less data / overhead
  struct DeletionQueue
  {
    std::deque<std::function<void()>> deletors;
    void push_function(std::function<void()>&& function)
    {
      deletors.push_back(function);
    }
    
    void flush()
    {
      for (auto it = deletors.rbegin(); it != deletors.rend(); it++)
      {
        (*it)();
      }
      deletors.clear();
    }
  };

  enum class MaterialPass:uint8_t 
  {
    MainColour,
    Transparent,
    Other,
  }; 

  struct DrawContext;
  class IRenderable
  {
    virtual void queue_draw(const glm::mat4& topMatrix, DrawContext& ctx) = 0;
  };

  struct Node: public IRenderable
  {
    std::weak_ptr<Node> parent;
    std::vector<std::shared_ptr<Node>> children;
    
    glm::mat4 localTransform{};
    glm::mat4 worldTransform{};

    void refresh_transform(const glm::mat4& parentMatrix)
    {
      worldTransform = parentMatrix * localTransform;
      for (auto c : children)
      {
        c->refresh_transform(worldTransform);
      }
    }
    
    virtual void queue_draw(const glm::mat4& topMatrix, DrawContext& ctx)
    {
      for (auto& c : children)
      {
        c->queue_draw(topMatrix, ctx);
      }
    }
  };



// NOTE: cpp/glsl structure definition
#include "input_structures.glsl"
#include "common.h"
#include "zprepass/zprepass.vert"
#include "shadow/shadow_map.vert"
#include "debug_line/debug_line.vert"
#include "ssao/ssao.comp"
#include "ssao/bilateral_filter.comp"
#include "culling/indirect_cull.comp"

  struct GeoSurface
  {
    uint32_t startIndex;
    uint32_t count;
    Bounds bounds;
    uint32_t mat_idx;
  };

  struct MeshAsset
  {
    std::string name;
    std::vector<GeoSurface> surfaces;
  };



  // still using global buffers
  struct DrawSetBuffers
  {
    AllocatedBuffer indices;
    AllocatedBuffer draws;
    AllocatedBuffer indirectDraws;
  };


  struct DrawSet
  {
    std::vector<uint32_t> indices;
    std::vector<DrawData> draw_datas;
    std::vector<IndirectDraw> indirect_draws;
    DrawSetBuffers buffers;

    VkPipeline pipeline;
  };





} // namespace Lucerna
