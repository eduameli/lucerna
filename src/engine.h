#pragma once
// NOTE: move device into engine + vkguide stuff!
#include "aurora_pch.h"
#include <vulkan/vulkan.h>
#include "window.h"
#include "device.h"
#include <deque>
namespace Aurora
{

  // NOTE: its inefficient to store a function for every object being deleted
  // a better approach would be to store arrays of vulkan handles of various types
  // such as VkImage, VkBuffer and so on, and delete those from a loop.
  // NOTE: this could be vk_core or smth
  struct DeletionQueue
  {
    std::deque<std::function<void()>> deletors;
    void PushFunction(std::function<void()>&& function)
    {
      deletors.push_back(function);
    }
    
    void Flush()
    {
      for (auto it = deletors.rbegin(); it != deletors.rend(); it++)
      {
        (*it)();
      }
      
      deletors.clear();
    }
  };

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
    public:
      Engine();
      ~Engine();
      void draw();
    private:
      #ifdef DEBUG
      const bool m_UseValidationLayers = true;
      #else
      const bool m_UseValidationLayers = false;  
      #endif
      std::vector<const char*> m_InstanceExtensions = {};
      const std::array<const char*, 1> m_ValidationLayers = {
        "VK_LAYER_KHRONOS_validation",
      };
      VkInstance h_Instance;
      VkDebugUtilsMessengerEXT h_DebugMessenger;
      DeletionQueue m_DeletionQueue;
    private:
      void init_vulkan();
      void check_instance_ext_support();
      void check_validation_layer_support();
      void create_instance();
      void setup_validation_layer_callback();
      static VKAPI_ATTR VkBool32 VKAPI_CALL validation_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData
      );
      VkResult create_debug_messenger(
        VkInstance instance,
        const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkDebugUtilsMessengerEXT* pDebugMessenger
      );
      void destroy_debug_messenger(
        VkInstance instance,
        VkDebugUtilsMessengerEXT debugMessenger,
        const VkAllocationCallbacks* pAllocator
      );
      
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
