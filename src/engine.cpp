#include "engine.h"
#include "aurora_pch.h"
namespace Aurora
{
Engine::Engine(Window& window)
  : m_Window{window}, h_Device{m_Window}
{
  AR_CORE_INFO("Starting Engine!!");
  //FIX: could have a better "API" like vkbootstrap enum=
  h_GraphicsQueue = h_Device.GetGraphicsQueue();
  m_GraphicsQueueIndex = h_Device.indices.graphicsFamily.value();

  // create command pool / buffers
  VkCommandPoolCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  createInfo.pNext = nullptr;
  createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  createInfo.queueFamilyIndex = m_GraphicsQueueIndex;

  for (int i = 0; i < FRAME_OVERLAP; i++)
  {
    VK_CHECK_RESULT(vkCreateCommandPool(h_Device.GetLogicalDevice(), &createInfo, nullptr, &m_Frames[i].commandPool));
    
    VkCommandBufferAllocateInfo cmdInfo{};
    cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdInfo.pNext = nullptr;
    cmdInfo.commandPool = m_Frames[i].commandPool;
    cmdInfo.commandBufferCount = 1;
    cmdInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    VK_CHECK_RESULT(vkAllocateCommandBuffers(h_Device.GetLogicalDevice(), &cmdInfo, &m_Frames[i].mainCommandBuffer));

  }
  
  // init sync structures
  VkFenceCreateInfo fenceInfo{};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.pNext = nullptr;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  VkSemaphoreCreateInfo semaphoreInfo{};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  semaphoreInfo.pNext = nullptr;
  
  for (int i = 0; i < FRAME_OVERLAP; i++)
  {
    VK_CHECK_RESULT(vkCreateFence(h_Device.GetLogicalDevice(), &fenceInfo, nullptr, &m_Frames[i].renderFence));
    
    VK_CHECK_RESULT(vkCreateSemaphore(h_Device.GetLogicalDevice(), &semaphoreInfo, nullptr, &m_Frames[i].swapchainSemaphore));
    VK_CHECK_RESULT(vkCreateSemaphore(h_Device.GetLogicalDevice(), &semaphoreInfo, nullptr, &m_Frames[i].renderSemaphore));
  }

}

Engine::~Engine()
{
  vkDeviceWaitIdle(h_Device.GetLogicalDevice());
  for (int i = 0; i < FRAME_OVERLAP; i++)
  {
    AR_CORE_TRACE("Destroying pool!! T_T");
    vkDestroyCommandPool(h_Device.GetLogicalDevice(), m_Frames[i].commandPool, nullptr);
  }
}

void Engine::Draw()
{
  VK_CHECK_RESULT(vkWaitForFences(h_Device.GetLogicalDevice(), 1, &GetCurrentFrame().renderFence, true, 1000000000));
  VK_CHECK_RESULT(vkResetFences(h_Device.GetLogicalDevice(), 1, &GetCurrentFrame().renderFence));
}
}
