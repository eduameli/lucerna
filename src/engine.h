#pragma once

#include <volk.h>
#include "aurora_pch.h"
#include "vk_descriptors.h"
#include "vk_types.h"
#include "vk_loader.h"
#include "window.h"
#include "vk_device.h"
#include "vk_swapchain.h"

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
      void validate_instance_supported();
      void create_instance();
      void create_device();
      FrameData& get_current_frame() { return m_Frames[frameNumber % FRAME_OVERLAP]; }
      void draw_background(VkCommandBuffer cmd);
      AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
      void destroy_buffer(const AllocatedBuffer& buffer);
      void destroy_swapchain();
      void create_swapchain();
    private:
      VkInstance m_Instance;
      VkDebugUtilsMessengerEXT m_DebugMessenger; //NOTE move to Logger?
      DeviceContext m_Device;
      VkSurfaceKHR m_Surface;
      SwapchainContext m_Swapchain;
      DeletionQueue m_DeletionQueue;
      FrameData m_Frames[FRAME_OVERLAP];
      DescriptorAllocator m_DescriptorAllocator;
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
      AllocatedImage m_DrawImage{};
      VkDescriptorSet m_DrawDescriptors{};
      VkDescriptorSetLayout m_DrawDescriptorLayout{};
      VkExtent2D m_DrawExtent{};
      AllocatedImage m_DepthImage{}; 
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
  };

}
