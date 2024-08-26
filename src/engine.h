#pragma once

#include "aurora_pch.h"
#include <vulkan/vulkan.h>
#include "vk_descriptors.h"
#include "vk_types.h"
#include "vk_loader.h"

namespace Aurora {

  struct FrameData
  {
    VkCommandPool commandPool{};
    VkCommandBuffer mainCommandBuffer{};
    VkSemaphore swapchainSemaphore{}, renderSemaphore{};
    VkFence renderFence{};
    DeletionQueue deletionQueue;
  };
  constexpr uint32_t FRAME_OVERLAP = 2;

  class Engine
  {
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

      DescriptorAllocator g_DescriptorAllocator{};
      VkDescriptorSet drawImageDescriptors{};
      VkDescriptorSetLayout drawImageDescriptorLayout{};
      
      // descriptor set for shaders and push constants...
      VkPipelineLayout gradientPipelineLayout{};
      VkPipelineLayout trigPipelineLayout{};
      VkPipeline trianglePipeline{};
    
      VkPipelineLayout meshPipelineLayout{};
      VkPipeline meshPipeline;
      GPUMeshBuffers rectangle{};

      VkSurfaceFormatKHR m_SwapchainFormat;
      VkExtent2D m_SwapchainExtent;
      
      bool stop_rendering = false;
      bool resize_requested = false;
      void resize_swapchain();

      struct EngineStats
      {
        float frametime;
        uint32_t triangle_count;
        uint32_t drawcall_count;
        float mesh_draw_time;
      } stats;
    
      std::vector<std::shared_ptr<MeshAsset>> testMeshes;
    public:
      void init();
      void shutdown();

      void run();
      void draw();
      // FIXME: move to private!
      void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);
      void draw_geometry(VkCommandBuffer cmd);
      static Engine& get();
    private:
      #ifdef DEBUG
      constexpr static bool m_UseValidationLayers = true;
      #else
      constexpr static bool m_UseValidationLayers = false;  
      #endif
      std::vector<const char*> m_InstanceExtensions = {};
      const std::array<const char*, 1> m_ValidationLayers = {
        "VK_LAYER_KHRONOS_validation",
      };
      VkInstance h_Instance;
      VkSurfaceKHR h_Surface;
      VkDebugUtilsMessengerEXT h_DebugMessenger;
      DeletionQueue m_DeletionQueue;
      
      VkDevice h_Device;
      VkPhysicalDevice h_PhysicalDevice;
      VkQueue h_GraphicsQueue;
      VkQueue h_PresentQueue;
      QueueFamilyIndices m_Indices;
      VkSwapchainKHR h_Swapchain;
      std::vector<VkImage> m_SwapchainImages;
      std::vector<VkImageView> m_SwapchainImageViews;
      
      std::vector<const char*> m_DeviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
      };
      FrameData m_Frames[FRAME_OVERLAP];
      uint32_t m_FrameNumber{0};
      VmaAllocator m_Allocator{};
      
      AllocatedImage m_DrawImage{};
      AllocatedImage m_DepthImage{};
      VkExtent2D m_DrawExtent{};
      
      std::vector<ComputeEffect> backgroundEffects;
      int currentBackgroundEffect{0};
  
      //NOTE: one way to improve this would be to run it on a different queue than the graphics queue (transfer?) to overlap
      // with the main render loop.
      VkFence m_ImmediateFence;
      VkCommandBuffer m_ImmediateCommandBuffer;
      VkCommandPool m_ImmediateCommandPool;
      void immediate_submit(std::function<void(VkCommandBuffer cmd)> && function);
      
    private:
      void init_vulkan();
      void init_swapchain();
      void init_commands();
      void init_sync_structures();
      void init_descriptors();
      void init_pipelines();
      void init_background_pipelines();
      void init_triangle_pipeline();
      void init_mesh_pipeline();
      void init_imgui();

      void check_instance_ext_support();
      void check_validation_layer_support();
      void create_instance();
      static VKAPI_ATTR VkBool32 VKAPI_CALL validation_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData
      );
      void create_device();
      FrameData& get_current_frame() { return m_Frames[m_FrameNumber % FRAME_OVERLAP]; }
      void draw_background(VkCommandBuffer cmd);
      
      AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
      void destroy_buffer(const AllocatedBuffer& buffer);
  
    public:
      // FIXME: deleting/reusing staging buffers & run on background thread who's sole purpose is executing commands like this
      GPUMeshBuffers upload_mesh(std::span<Vertex> vertices, std::span<uint32_t> indices);
  };
/*
  class Engine
  {
    public:
      Engine();
      ~Engine();
      void init_vulkan();
      void draw();
      
      inline FrameData& GetCurrentFrame() { return m_Frames[m_FrameNumber % FRAME_OVERLAP]; }
    public:
      static constexpr int WIDTH = 640;
      static constexpr int HEIGHT = 480;
    private:
      // vkhelper or vkimage?
      void TransitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);
      // vkhelper init?
    private:
      FrameData m_Frames[FRAME_OVERLAP];
      uint32_t m_FrameNumber = 0;

      //Window& m_Window;
      //Device h_Device;
      
      VkQueue h_GraphicsQueue{};
      uint32_t m_GraphicsQueueIndex{};

      DeletionQueue m_DeletionQueue;
      VmaAllocator m_Allocator;

      AllocatedImage m_DrawImage;
      VkExtent2D m_DrawExtent;
  };
*/
}
