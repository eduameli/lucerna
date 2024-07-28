#pragma once
// NOTE: move device into engine + vkguide stuff!
#include <vulkan/vulkan.h>
#include "window.h"
#include "device.h"
namespace Aurora
{

  struct FrameData
  {
    VkCommandPool commandPool;
    VkCommandBuffer mainCommandBuffer;
    VkSemaphore swapchainSemaphore, renderSemaphore;
    VkFence renderFence;
  };
  constexpr uint32_t FRAME_OVERLAP = 2;

  class Engine
  {
    public:
      Engine(Window& window);
      ~Engine();
      void Draw();
      inline FrameData& GetCurrentFrame() { return m_Frames[m_FrameNumber % FRAME_OVERLAP]; }
    public:
      static constexpr int WIDTH = 640;
      static constexpr int HEIGHT = 480;
    private:
      FrameData m_Frames[FRAME_OVERLAP];
      size_t m_FrameNumber{};

      Window& m_Window;
      Device h_Device;
      
      VkQueue h_GraphicsQueue;
      uint32_t m_GraphicsQueueIndex;
  };
}
