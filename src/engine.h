#pragma once
#include "aurora_pch.h"
#include "vk_descriptors.h"
#include "vk_types.h"
#include "vk_loader.h"
#include "vk_device.h"
#include "vk_swapchain.h"
#include "camera.h"

namespace Aurora {
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
    uint32_t indexCount, firstIndex;
    VkBuffer indexBuffer;
    MaterialInstance* material;
    Bounds bounds;
    glm::mat4 transform;
    VkDeviceAddress vertexBufferAddress;
  };

  struct DrawContext {
    std::vector<RenderObject> OpaqueSurfaces;
    std::vector<RenderObject> TransparentSurfaces;
  };

  struct EngineStats
  {
    float frametime;
    int triangle_count;
    int drawcall_count;
    float scene_update_time;
    float mesh_draw_time;
  };

  // FIXME: some stuff is public that could be private!
  class Engine
  {
    public:
      void init();
      void shutdown();
      void run();
      
      AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
      void destroy_buffer(const AllocatedBuffer& buffer);
      GPUMeshBuffers upload_mesh(std::span<Vertex> vertices, std::span<uint32_t> indices);
      AllocatedImage create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
      AllocatedImage create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
      void destroy_image(const AllocatedImage& img);
    
      void update_scene();

      static Engine& get();
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
    
      DeviceContext m_Device;
      SwapchainContext m_Swapchain;
    
      // FIXME: use directily instead of m_Device.<>
      // same with SwapchainContext
      VkDevice device;
      VkPhysicalDevice gpu;
      VkQueue graphicsQueue;
      VkQueue presentQueue;
      std::optional<uint32_t> graphicsIndex;
      std::optional<uint32_t> presentIndex;
      QueueFamilyIndices indices;

      

    

      Camera mainCamera;
      DrawContext mainDrawContext;
      std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> loadedScenes; 
      EngineStats stats{};
      
      size_t frameNumber{ 0 };
      bool stopRendering{ false };
      bool resizeRequested{ false };

      AllocatedImage m_DrawImage;
      AllocatedImage m_DepthImage;
      AllocatedImage m_WhiteImage;
      AllocatedImage m_BlackImage;
      AllocatedImage m_GreyImage;
      AllocatedImage m_ErrorCheckerboardImage;
      AllocatedImage m_ShadowDepthImage; // used for shadow mapping!
      VkSampler m_DefaultSamplerNearest;
      VkSampler m_DefaultSamplerLinear;
      VkDescriptorSetLayout m_SceneDescriptorLayout;
      VkDescriptorSetLayout m_SingleImageDescriptorLayout;

      MaterialInstance defaultData;
      GLTFMetallic_Roughness metalRoughMaterial;
      
      // NOTE move to pcss_settings & add hard shadow setting
      bool lightView{ false };
      float spinSpeed{ 0.0f };
      glm::mat4 lView{ 1.0f };
      glm::mat4 lightProj{ 1.0f };
      glm::mat4 lightViewProj{ 1.0f };
    private:
      void init_vulkan();
      void init_swapchain();
      void init_commands();
      void init_sync_structures();
      void init_descriptors();
      void init_pipelines();
      void init_background_pipelines();
      void init_mesh_pipeline();
      void init_imgui();
      void init_default_data();
      void validate_instance_supported();
      void create_instance();
      void create_device();

      inline bool should_quit();
      void draw();
      void draw_background(VkCommandBuffer cmd);
      void draw_geometry(VkCommandBuffer cmd);
      void draw_shadow_pass(VkCommandBuffer cmd);
      void draw_imgui(VkCommandBuffer cmd, VkImageView target);
    public:
      FrameData& get_current_frame() { return m_Frames[frameNumber % FRAME_OVERLAP]; }
    private:
      void resize_swapchain();
    public:
      void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);
    private:
      bool is_visible(const RenderObject& obj, const glm::mat4& viewproj);
    private:
      VkInstance m_Instance;
      VkDebugUtilsMessengerEXT m_DebugMessenger; //NOTE move to Logger?
      VkSurfaceKHR m_Surface;
    public:
      DeletionQueue m_DeletionQueue;
    private:
      FrameData m_Frames[FRAME_OVERLAP];
    public:
      DescriptorAllocatorGrowable globalDescriptorAllocator;
      VkExtent3D internalExtent{};
    private:
      int m_BackgroundEffectIndex{ 0 };
      std::vector<ComputeEffect> m_BackgroundEffects;
      std::vector<const char*> m_InstanceExtensions = {};
      std::vector<const char*> m_DeviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
      };
      std::vector<const char*> m_ValidationLayers = {
        "VK_LAYER_KHRONOS_validation",
      };
    public:
      VmaAllocator m_Allocator{};
    private:
      VkDescriptorSet m_DrawDescriptors{};
      VkDescriptorSetLayout m_DrawDescriptorLayout{};
    public:
      VkExtent2D m_DrawExtent{};
    private:
      VkExtent2D m_WindowExtent{};
      float m_RenderScale = 1.0f;

      #ifdef DEBUG 
      constexpr static bool m_UseValidationLayers = true;
      #else
      constexpr static bool m_UseValidationLayers = false;
      #endif
      
      VkFence m_ImmFence;
      VkCommandBuffer m_ImmCommandBuffer;
      VkCommandPool m_ImmCommandPool;
      GPUSceneData sceneData;

      VkPipeline m_ShadowPipeline;
      VkPipelineLayout m_ShadowPipelineLayout;
      VkDescriptorSetLayout m_ShadowSetLayout;
      VkExtent3D m_ShadowExtent{ 1024*4, 1024*4, 1 };

      struct ShadowMappingSettings
      {
        glm::mat4 lightViewProj;
        bool rotate{ false };
        float light_size_uv{/* 0.25 */ 0.0};
        float ortho_size{ 10.0 };
        float near{ 0.1 };
        float far{ 20.0 };
      } pcss_settings;
      
      // this instead of pcs / sceneData
      struct ShadowUBO
      {
        glm::mat4 lightView;
        float near{0.1};
        float far{20.0};
        float light_size{0.0};
        bool pcss_enabled{true};
      } shadowUBO;

  };
} // namespace aurora
