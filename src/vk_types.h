#pragma once
#include <volk.h>
#include "vk_mem_alloc.h"
#include "aurora_pch.h"

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

namespace Aurora
{
  struct AllocatedImage
  {
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VkExtent3D imageExtent;
    VkFormat imageFormat;
  };
  
  // NOTE: could contain create_buffer and destroy_buffer inside the struct and be mini & self contained
  struct AllocatedBuffer
  {
    VkBuffer buffer{};
    VmaAllocation allocation{};
    VmaAllocationInfo info{};
  };
 
  struct Vertex
  {
    glm::vec3 position;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
    glm::vec4 color;
  };

  struct GPUMeshBuffers
  {
    AllocatedBuffer indexBuffer{};
    AllocatedBuffer vertexBuffer{};
    VkDeviceAddress vertexBufferAddress{};
  };
  
  struct GPUDrawPushConstants
  {
    glm::mat4 worldMatrix;
    VkDeviceAddress vertexBuffer;
  };
  
  struct GPUSceneData
  {
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewproj;
    glm::vec4 ambientColour;
    glm::vec4 sunlightDirection;
    glm::vec4 sunlightColour;
  };


  // FIXME: only needed in egine?? or maybe only types that are referenced multiple times, created many times
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

  struct MaterialPipeline 
  {
    VkPipeline pipeline;
    VkPipelineLayout layout;
  };

  struct MaterialInstance
  {
    MaterialPipeline* pipeline;
    VkDescriptorSet materialSet;
    MaterialPass passType;
  };
  
  struct DrawContext;
  class IRenderable
  {
    virtual void draw(const glm::mat4& topMatrix, DrawContext& ctx) = 0;
  };

  struct Node: public IRenderable
  {
    std::weak_ptr<Node> parent;
    std::vector<std::shared_ptr<Node>> children;
    
    glm::mat4 localTransform;
    glm::mat4 worldTransform;

    void refresh_transform(const glm::mat4& parentMatrix)
    {
      worldTransform = parentMatrix * localTransform;
      for (auto c : children)
      {
        c->refresh_transform(worldTransform);
      }
    }
    
    virtual void draw(const glm::mat4& topMatrix, DrawContext& ctx)
    {
      for (auto& c : children)
      {
        c->draw(topMatrix, ctx);
      }
    }
  };


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
} // namespace aurora

