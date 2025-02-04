#pragma once
#include "lucerna_pch.h"
#include "input_structures.glsl"
#include "vk_descriptors.h"
#include "vk_types.h"
#include "vk_loader.h"
#include "vk_device.h"
#include "vk_swapchain.h"
#include "camera.h"
#include <vulkan/vulkan_core.h>


namespace Lucerna {
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
    uint32_t mesh_idx;
    uint32_t material_idx;
    
    glm::mat4 transform;
    VkDeviceAddress vertexBufferAddress, positionBufferAddress;
  };

  struct DrawContext {
    std::vector<glm::mat4x3> transforms;
    std::vector<StandardMaterial> standard_materials;
    std::vector<uint32_t> indices;
    std::vector<glm::vec3> positions;
    std::vector<Vertex> vertices;
    std::vector<glm::vec4> sphere_bounds;
    GPUSceneBuffers sceneBuffers;
  };


  struct EngineStats
  {
    float frametime;
    int triangle_count;
    int drawcall_count;
    float scene_update_time;
    float mesh_draw_time;
    
    std::string gpuName{};
    std::string instanceVersion{};
  };

  
  struct FrameGraph
  {
    static void add_sample(float sample);
    static void render_graph();
    static inline std::deque<float> frametimes{};
    static inline float average = 0;
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
      
      GPUSceneBuffers upload_scene(
        std::span<glm::vec3> positions,
        std::span<Vertex> vertices,
        std::span<uint32_t> indices,
        std::span<glm::mat4x3> transforms,
        std::span<glm::vec4> sphere_bounds,
        std::span<StandardMaterial> materials
      );


      void  upload_draw_set(
        DrawSet& set
      );
      
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
      void queue_debug_obb(glm::mat4 transform, glm::vec3 origin, glm::vec3 extents); 

    public:

      // NOTE: unused, just to get an idea of possible architecture
      DrawSet opaque_set{.name = "Opaque Set"};
      DrawSet transparent_set{.name = "Transparent Set"};

      // NOTE: end unused
      // draw set - many diff buckets (alpha cutoff, transparent, ??)

      
    
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
      void init_mesh_pipeline();
      void init_imgui();
      void init_default_data();
      void validate_instance_supported();
      void create_instance();
      void create_device();


      void init_draw_sets();

      public:
      constexpr static uint32_t SAMPLED_IMAGE_BINDING = 0;
      constexpr static uint32_t SAMPLER_BINDING = 1;
      constexpr static uint32_t STORAGE_IMAGE_BINDING = 2;

      constexpr static uint32_t SAMPLED_IMAGE_COUNT = 65536; // 2^24 left if u store combined sampler in uint32_t
      constexpr static uint32_t SAMPLER_COUNT = 65536; // not this many.. store image idx in uint32_t 5 bits for the sampler?  2^8 -> 256               
      constexpr static uint32_t STORAGE_IMAGE_COUNT = 65536;
        
      VkDescriptorPool bindless_descriptor_pool;
      VkDescriptorSetLayout bindless_descriptor_layout;
      
      public:
      VkDescriptorSet bindless_descriptor_set;
      VkPipelineLayout bindless_pipeline_layout;
      VkPipeline std_pipeline;
      VkDescriptorSet global_descriptor_set;

      private:

      // indirect culling - FIXME: should use draw_sets one for the different geometry "buckets"
      // FIXME: scuffed now culls WHOLE DRAWING EVEN FOR SHADOWS
      VkPipeline cullPipeline{};
      VkPipeline compactPipeline{};
      VkPipeline writeIndirectPipeline{};
      
      VkPipelineLayout cullPipelineLayout{};
      VkDescriptorSetLayout compact_descriptor_layout{};
      
      void init_indirect_cull_pipeline();
      void do_culling(VkCommandBuffer cmd, DrawSet& set); // 
      
    private:
      
      std::vector<VkImageView> sampler_desc_updates; // NOTE: pass by value?
      std::vector<VkImageView> img_desc_updates;
      
      std::vector<std::pair<VkImageView, uint32_t>> upload_sampled;
      std::vector<std::pair<VkImageView, uint32_t>> upload_storage;
      
    public:

      // NOTE: use this across the board instead of free list?
      uint32_t sampledCounter{ 0 };
      uint32_t samplerCounter{ 0 };
      uint32_t storageCounter{ 0 };

    private:
      void init_bindless_pipeline_layout();
      void update_descriptors();

      inline bool should_quit();
      void draw();
      void draw_background(VkCommandBuffer cmd);
      void draw_depth_prepass(VkCommandBuffer cmd);
      void draw_geometry(VkCommandBuffer cmd);
      void draw_shadow_pass(VkCommandBuffer cmd);
      void draw_debug_lines(VkCommandBuffer cmd);
      void draw_imgui(VkCommandBuffer cmd, VkImageView target);
      
      void render_draw_set(VkCommandBuffer cmd, DrawSet& draw_set);
      
    private:
      VkInstance m_Instance;
      VkDebugUtilsMessengerEXT m_DebugMessenger; //NOTE move to Logger?
      VkSurfaceKHR m_Surface;
      std::vector<const char*> m_InstanceExtensions = {};
      std::vector<const char*> m_DeviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME,
        VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME, // FIXME not used right now...
      };
      std::vector<const char*> m_ValidationLayers = {
        "VK_LAYER_KHRONOS_validation",
      };
      VkDescriptorSet m_DrawDescriptors{};
      VkDescriptorSetLayout m_DrawDescriptorLayout{};

      VkExtent2D m_WindowExtent{};
      float m_RenderScale = 1.0f;

      #ifdef LA_DEBUG 
      constexpr static bool m_UseValidationLayers = true;
      #else
      constexpr static bool m_UseValidationLayers = false;
      #endif

      glm::mat4 lastDebugFrustum{1.0f};
      
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
} // namespace Lucerna


