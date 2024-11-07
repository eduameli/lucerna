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
    VkDeviceAddress vertexBufferAddress, positionBufferAddress;
  };

  struct DrawContext {
    std::vector<RenderObject> OpaqueSurfaces;
    std::vector<RenderObject> TransparentSurfaces;
    std::vector<uint32_t> opaque_draws;
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
      GPUMeshBuffers upload_mesh(std::span<glm::vec3> positions, std::span<Vertex> vertices, std::span<uint32_t> indices);
      AllocatedImage create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
      AllocatedImage create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
      void destroy_image(const AllocatedImage& img) const;
      void update_scene();
      static Engine* get();

      FrameData& get_current_frame() { return m_Frames[frameNumber % FRAME_OVERLAP]; }
      void resize_swapchain(int width, int height);
      void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);
     
      // debug lines functions
      void queue_debug_line(glm::vec3 p1, glm::vec3 p2);
      void queue_debug_frustum(glm::mat4 frustum);    
      void queue_debug_obb(glm::vec3 origin, glm::vec3 extents); 

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
      VkPhysicalDevice physicalDevice;
      VkQueue graphicsQueue;
      VkQueue presentQueue;
      uint32_t graphicsIndex;
      uint32_t presentIndex;
      QueueFamilyIndices indices;

      Camera mainCamera;
      DrawContext mainDrawContext;
      std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> loadedScenes; 
      EngineStats stats{};
      
      size_t frameNumber{ 0 };
      bool stopRendering{ false };

      bool valid_swapchain{ true };

      AllocatedImage m_DrawImage;
      AllocatedImage m_DepthImage;
      AllocatedImage m_WhiteImage;
      AllocatedImage m_BlackImage;
      AllocatedImage m_GreyImage;
      AllocatedImage m_ErrorCheckerboardImage;
      AllocatedImage m_ShadowDepthImage; // used for shadow mapping!
      VkSampler m_DefaultSamplerNearest;
      VkSampler m_DefaultSamplerLinear;
      VkSampler m_ShadowSampler;

      VkDescriptorSetLayout m_SceneDescriptorLayout;
      VkDescriptorSetLayout m_SingleImageDescriptorLayout;

      MaterialInstance defaultData;
      GLTFMetallic_Roughness metalRoughMaterial;
      
      // NOTE move to pcss_settings & add hard shadow setting
      glm::mat4 lView{ 1.0f };
      glm::mat4 lightProj{ 1.0f };
      glm::mat4 lightViewProj{ 1.0f };
      DeletionQueue m_DeletionQueue;
      FrameData m_Frames[FRAME_OVERLAP];
      DescriptorAllocatorGrowable globalDescriptorAllocator;
      VkExtent3D internalExtent{};
      VmaAllocator m_Allocator{};
      VkExtent2D m_DrawExtent{};
  
    private:
      void init_vulkan();
      void init_swapchain();
      void init_commands();
      void init_sync_structures();
      void init_descriptors();
      void init_pipelines();
      void init_depth_prepass_pipeline();
      void init_shadow_map_pipeline();
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
      void draw_depth_prepass(VkCommandBuffer cmd);
      void draw_geometry(VkCommandBuffer cmd);
      void draw_shadow_pass(VkCommandBuffer cmd);
      void draw_debug_lines(VkCommandBuffer cmd);
      void draw_imgui(VkCommandBuffer cmd, VkImageView target);
      bool is_visible(const RenderObject& obj, const glm::mat4& viewproj);
    private:
      VkInstance m_Instance;
      VkDebugUtilsMessengerEXT m_DebugMessenger; //NOTE move to Logger?
      VkSurfaceKHR m_Surface;
      int m_BackgroundEffectIndex{ 0 };
      std::vector<ComputeEffect> m_BackgroundEffects;
      std::vector<const char*> m_InstanceExtensions = {};
      std::vector<const char*> m_DeviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
      };
      std::vector<const char*> m_ValidationLayers = {
        "VK_LAYER_KHRONOS_validation",
      };
      VkDescriptorSet m_DrawDescriptors{};
      VkDescriptorSetLayout m_DrawDescriptorLayout{};

      VkExtent2D m_WindowExtent{};
      float m_RenderScale = 1.0f;

      #ifdef AR_DEBUG 
      constexpr static bool m_UseValidationLayers = true;
      #else
      constexpr static bool m_UseValidationLayers = false;
      #endif
      
      VkFence m_ImmFence;
      VkCommandBuffer m_ImmCommandBuffer;
      VkCommandPool m_ImmCommandPool;
      public:
      GPUSceneData sceneData;
      private:
      VkPipeline m_DepthPrepassPipeline;
      VkPipeline m_ShadowPipeline;
      VkPipelineLayout m_ShadowPipelineLayout;
      VkPipelineLayout zpassLayout;
      VkDescriptorSetLayout m_ShadowSetLayout;
      VkDescriptorSetLayout zpassDescriptorLayout;
      
      // draw debug lines structures
      VkPipelineLayout debugLinePipelineLayout;
      VkPipeline debugLinePipeline;
      std::vector<glm::vec3> debugLines;
      AllocatedBuffer debugLinesBuffer;
      

      VkExtent3D m_ShadowExtent{ 1024, 1024, 1 };

      struct ShadowMappingSettings
      {
        glm::mat4 lightViewProj;
        bool rotate{ false };
        float light_size_uv{/* 0.25 */ 0.0};
        float ortho_size{ 5.0 };
        float distance{ 5.0 };
        float near{ 0.1 };
        float far{ 15.0 };
        uint32_t shadowNumber{ 0 };
      } pcss_settings;

       struct ShadowPassSettings
      {
        AllocatedBuffer buffer;
        glm::mat4 lightView; 
      } shadowPass;
  };
} // namespace aurora
