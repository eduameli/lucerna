#pragma once
#include "aurora_pch.h"
#include "vk_descriptors.h"
#include "vk_types.h"
#include "vk_loader.h"
#include "vk_device.h"
#include "vk_swapchain.h"
#include "camera.h"

namespace Aurora {

   struct GLTFMetallic_Roughness
    {
    MaterialPipeline opaquePipeline;
    MaterialPipeline transparentPipeline;
  
    VkDescriptorSetLayout materialLayout;
    
    struct MaterialConstants
    {
      glm::vec4 colorFactors;
      glm::vec4 metal_rough_factors;
      glm::vec4 extra[14]; // uniform buffers need minimum requirement for alignment, 256bytes is a good default alignment that gpus meet 
    };

    struct MaterialResources
    {
      AllocatedImage colorImage;
      VkSampler colorSampler;
      AllocatedImage metalRoughImage;
      VkSampler metalRoughSampler;
      VkBuffer dataBuffer;
      uint32_t dataBufferOffset;
    };

    DescriptorWriter writer;
    
    void build_pipelines(Engine* engine);
    void clear_resources(VkDevice device);

    MaterialInstance write_material(VkDevice device, MaterialPass pass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorAllocator);
  };

  struct FrameData
  {
    VkCommandPool commandPool{};
    VkCommandBuffer mainCommandBuffer{};
    VkSemaphore swapchainSemaphore{}, renderSemaphore{};
    VkFence renderFence{};
    DeletionQueue deletionQueue;
    DescriptorAllocatorGrowable frameDescriptors;
  };
  constexpr uint32_t FRAME_OVERLAP = 2;



struct RenderObject {
	uint32_t indexCount;
	uint32_t firstIndex;
	VkBuffer indexBuffer;

	MaterialInstance* material;

	glm::mat4 transform;
	VkDeviceAddress vertexBufferAddress;
};

struct DrawContext {
	std::vector<RenderObject> OpaqueSurfaces;
  std::vector<RenderObject> TransparentSurfaces;
};
struct MeshNode : public Node {

	std::shared_ptr<MeshAsset> mesh;

	virtual void draw(const glm::mat4& topMatrix, DrawContext& ctx) override;
};

struct EngineStats
{
  float frametime;
  int triangle_count;
  int drawcall_count;
  float scene_update_time;
  float mesh_draw_time;
};

  class Engine
  {
    public:
      void init();
      void shutdown();
      void run();
      static Engine& get();
      GPUMeshBuffers upload_mesh(std::span<Vertex> vertices, std::span<uint32_t> indices);
    public:
      struct ComputePushConstants
      {
        float data1[4];
        float data2[4];
        float data3[4];
        float data4[4];
      };
      struct ComputeEffect
      {
        const char* name;
        VkPipeline pipeline;
        VkPipelineLayout layout;
        ComputePushConstants data;
      };
      bool stopRendering = false;
      bool resizeRequested = false;
      uint32_t frameNumber;
      SwapchainContext m_Swapchain;
      DeviceContext m_Device; 
    private:
      void draw();
      void draw_imgui(VkCommandBuffer cmd, VkImageView target);
      void draw_geometry(VkCommandBuffer cmd);
      void resize_swapchain();
      void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);
      void init_vulkan();
      void init_swapchain();
      void init_commands();
      void init_sync_structures();
      void init_descriptors();
      void init_pipelines(); // NOTE: this will leave engine class at some point, as its per model
      void init_background_pipelines();
      void init_mesh_pipeline();
      void init_imgui();
      void init_default_data();
      void validate_instance_supported();
      void create_instance();
      void create_device();
      FrameData& get_current_frame() { return m_Frames[frameNumber % FRAME_OVERLAP]; }
      void draw_background(VkCommandBuffer cmd);
    public:
      AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
      void destroy_buffer(const AllocatedBuffer& buffer);
      EngineStats stats; 
      AllocatedImage create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
      AllocatedImage create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
      void destroy_image(const AllocatedImage& img);
    private:
      VkInstance m_Instance;
      VkDebugUtilsMessengerEXT m_DebugMessenger; //NOTE move to Logger?
      VkSurfaceKHR m_Surface;
      DeletionQueue m_DeletionQueue;
      FrameData m_Frames[FRAME_OVERLAP];
      DescriptorAllocatorGrowable globalDescriptorAllocator;
      std::vector<std::shared_ptr<MeshAsset>> m_TestMeshes;
      int m_BackgroundEffectIndex = 0;
      std::vector<ComputeEffect> m_BackgroundEffects;
      std::vector<const char*> m_InstanceExtensions = {};
      std::vector<const char*> m_DeviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
      };
      std::vector<const char*> m_ValidationLayers = {
        "VK_LAYER_KHRONOS_validation",
      };
      VmaAllocator m_Allocator{};
    public:
      AllocatedImage m_DrawImage{};
    private:
      VkDescriptorSet m_DrawDescriptors{};
      VkDescriptorSetLayout m_DrawDescriptorLayout{};
      VkExtent2D m_DrawExtent{};
    public:
      AllocatedImage m_DepthImage{};
      MaterialInstance defaultData;
      GLTFMetallic_Roughness metalRoughMaterial;
    private:
      VkExtent2D m_WindowExtent{};
      float m_RenderScale = 1.0f;

      VkPipeline m_MeshPipeline;
      VkPipelineLayout m_MeshPipelineLayout;
      
      #ifdef USE_VALIDATION_LAYERS
      constexpr static bool m_UseValidationLayers = true;
      #else
      constexpr static bool m_UseValidationLayers = false;
      #endif
      
      VkFence m_ImmFence;
      VkCommandBuffer m_ImmCommandBuffer;
      VkCommandPool m_ImmCommandPool;

      GPUSceneData sceneData;
    public:
      VkDescriptorSetLayout m_SceneDescriptorLayout;
    public:
      AllocatedImage m_WhiteImage;
      AllocatedImage m_BlackImage;
      AllocatedImage m_GreyImage;
      AllocatedImage m_ErrorCheckerboardImage;
      VkSampler m_DefaultSamplerLinear;
      VkSampler m_DefaultSamplerNearest;
      VkDescriptorSetLayout m_SingleImageDescriptorLayout;
    public:
      DrawContext mainDrawContext;
      std::unordered_map<std::string, std::shared_ptr<Node>> loadedNodes;
      void update_scene();
      Camera mainCamera;
      std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> loadedScenes;
  };
  
 
} // namespace aurora
