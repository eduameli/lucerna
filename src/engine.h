#pragma once

#include "aurora_pch.h"
#include <vulkan/vulkan.h>
#include "vk_descriptors.h"

namespace Aurora
{

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
      DescriptorAllocator g_DescriptorAllocator{};
      VkDescriptorSet drawImageDescriptors{};
      VkDescriptorSetLayout drawImageDescriptorLayout{};
      VkPipeline gradientPipeline{};
      VkPipelineLayout gradientPipelineLayout{};
      VkSurfaceFormatKHR m_SwapchainFormat;
      VkExtent2D m_SwapchainExtent;

    public:
      Engine();
      ~Engine();
      void draw();
      void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);
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
      VkExtent2D m_DrawExtent{};

    private:
      void init_vulkan();
      void init_swapchain();
      void init_commands();
      void init_sync_structures();
      void init_descriptors();
      void init_pipelines();
      void init_background_pipelines();
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
      //void create_image_views();
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
