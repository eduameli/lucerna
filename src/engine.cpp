#include "engine.h"
#include "aurora_pch.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include "vk_initialisers.h"

#include "application.h"

// forward declaration

// NOTE: needs to create instance ... contains device ... surface swapchain logic .. frame drawing

namespace Aurora
{
Engine::Engine()
{
  Window& win = Application::get_main_window();
}

Engine::~Engine()
{}

void Engine::draw()
{}

/*
 
Engine::Engine(Window& window)
  : m_Window{window}, h_Device{m_Window}
{
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

  VmaAllocatorCreateInfo allocatorInfo{};
  allocatorInfo.physicalDevice = h_Device.GetPhysicalDevice();
  allocatorInfo.device = h_Device.GetLogicalDevice();
  allocatorInfo.instance = h_Device.GetInstance();
  allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
  vmaCreateAllocator(&allocatorInfo, &m_Allocator);
  m_DeletionQueue.PushFunction([&]() {
    vmaDestroyAllocator(m_Allocator);
  });


}

*/

/*
Engine::~Engine()
{
  vkDeviceWaitIdle(h_Device.GetLogicalDevice());
  for (int i = 0; i < FRAME_OVERLAP; i++)
  {
    vkDestroyCommandPool(h_Device.GetLogicalDevice(), m_Frames[i].commandPool, nullptr);
    vkDestroyFence(h_Device.GetLogicalDevice(), m_Frames[i].renderFence, nullptr);
    vkDestroySemaphore(h_Device.GetLogicalDevice(), m_Frames[i].renderSemaphore, nullptr);
    vkDestroySemaphore(h_Device.GetLogicalDevice(), m_Frames[i].swapchainSemaphore, nullptr);

  }
  m_DeletionQueue.Flush();
}

void Engine::Draw()
{
  VK_CHECK_RESULT(vkWaitForFences(h_Device.GetLogicalDevice(), 1, &GetCurrentFrame().renderFence, true, 1000000000));
  GetCurrentFrame().deletionQueue.Flush();

  VK_CHECK_RESULT(vkResetFences(h_Device.GetLogicalDevice(), 1, &GetCurrentFrame().renderFence));

  uint32_t swapChainImageIndex;
  VK_CHECK_RESULT(vkAcquireNextImageKHR(h_Device.GetLogicalDevice(), h_Device.GetSwapchain(), 1000000000, GetCurrentFrame().swapchainSemaphore, nullptr, &swapChainImageIndex));
  

  //NOTE: vkhelper class for easy inits
  VkCommandBufferBeginInfo info{};
  info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  info.pNext = nullptr;
  info.pInheritanceInfo = nullptr;
  info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  VkCommandBuffer cmd = GetCurrentFrame().mainCommandBuffer;
  VK_CHECK_RESULT(vkResetCommandBuffer(cmd, 0));
  VK_CHECK_RESULT(vkBeginCommandBuffer(cmd, &info));
 
  TransitionImage(cmd, h_Device.m_SwapchainImages[swapChainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

  VkClearColorValue clearValue;
  float flash = std::abs(std::sin(m_FrameNumber / 120.0f));
  clearValue = {{0.0f, 0.0f, flash, 1.0f}};
  VkImageSubresourceRange clearRange{};
  clearRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  clearRange.baseMipLevel = 0;
  clearRange.levelCount = VK_REMAINING_MIP_LEVELS;
  clearRange.baseArrayLayer = 0;
  clearRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

  vkCmdClearColorImage(cmd, h_Device.m_SwapchainImages[swapChainImageIndex], VK_IMAGE_LAYOUT_GENERAL, &clearValue, 1, &clearRange);

  TransitionImage(cmd, h_Device.m_SwapchainImages[swapChainImageIndex], VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
  VK_CHECK_RESULT(vkEndCommandBuffer(cmd));

  VkCommandBufferSubmitInfo cmdInfo = vkinit::commandbuffer_submit_info(cmd);
  VkSemaphoreSubmitInfo waitInfo= vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, GetCurrentFrame().swapchainSemaphore);
  VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, GetCurrentFrame().renderSemaphore);
  VkSubmitInfo2 submit = vkinit::submit_info(&cmdInfo, &signalInfo, &waitInfo);
  
  VK_CHECK_RESULT(vkQueueSubmit2(h_Device.GetGraphicsQueue(), 1, &submit, GetCurrentFrame().renderFence))

  VkPresentInfoKHR present{};
  present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present.pNext = nullptr;
  present.pSwapchains = 
  &h_Device.GetSwapchain();
  present.swapchainCount = 1;
  present.pWaitSemaphores = &GetCurrentFrame().renderSemaphore;
  present.waitSemaphoreCount = 1;
  present.pImageIndices = &swapChainImageIndex;
  VK_CHECK_RESULT(vkQueuePresentKHR(h_Device.GetGraphicsQueue(), &present));

  m_FrameNumber++;


}

void Engine::TransitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout)
{

  VkImageMemoryBarrier2 imageBarrier{};
  imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  imageBarrier.pNext = nullptr;
  imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
  imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;
  imageBarrier.oldLayout = currentLayout;
  imageBarrier.newLayout = newLayout;
  
  VkImageAspectFlags aspectMask = (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
  
  VkImageSubresourceRange sub{};
  sub.aspectMask = aspectMask;
  sub.baseMipLevel = 0;
  sub.levelCount = VK_REMAINING_MIP_LEVELS;
  sub.baseArrayLayer = 0;
  sub.layerCount = VK_REMAINING_ARRAY_LAYERS;

  imageBarrier.subresourceRange = sub;
  imageBarrier.image = image;

  VkDependencyInfo depInfo{};
  depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  depInfo.pNext = nullptr;
  depInfo.imageMemoryBarrierCount = 1;
  depInfo.pImageMemoryBarriers = &imageBarrier;
  
  vkCmdPipelineBarrier2(cmd, &depInfo);
 }
*/
}
