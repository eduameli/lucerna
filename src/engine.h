#pragma once

#include "aurora_pch.h"
#include <vulkan/vulkan.h>
#include "vk_descriptors.h"
#include "vk_types.h"
#include "vk_loader.h"
#include "window.h"


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
      
      VkPipelineLayout meshPipelineLayout{};
      VkPipeline meshPipeline;

      VkSurfaceFormatKHR m_SwapchainFormat;
      VkExtent2D m_SwapchainExtent;
      
      bool stop_rendering = false;
      bool resize_requested = false;

      void resize_swapchain();

      std::vector<std::shared_ptr<MeshAsset>> testMeshes;


    public:
      void init();
      void shutdown();

      void run();
      void draw();
      // FIXME: move to private!
      void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);
      void draw_geometry(VkCommandBuffer cmd);


      static Engine& get() { return *s_Instance; };
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

      VkInstance m_Instance;
      VkSurfaceKHR m_Surface;
      VkDebugUtilsMessengerEXT m_DebugMessenger;
      DeletionQueue m_DeletionQueue;
      
      VkDevice m_Device;
      VkPhysicalDevice m_PhysicalDevice;
      VkQueue m_GraphicsQueue;
      VkQueue m_PresentQueue;
      QueueFamilyIndices m_Indices;
      VkSwapchainKHR m_Swapchain;
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
    
      Window m_Window;

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
      void create_device();
      FrameData& get_current_frame() { return m_Frames[m_FrameNumber % FRAME_OVERLAP]; }
      void draw_background(VkCommandBuffer cmd);
      
      AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
      void destroy_buffer(const AllocatedBuffer& buffer);
      
      static Engine* s_Instance;

    public:
      // FIXME: deleting/reusing staging buffers & run on background thread who's sole purpose is executing commands like this
      GPUMeshBuffers upload_mesh(std::span<Vertex> vertices, std::span<uint32_t> indices);
  };

}
