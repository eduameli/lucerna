#pragma once
#include "aurora_pch.h"
#include <volk.h>
#include "vk_device.h"

namespace Aurora {
  struct SwapchainContext
  {
    VkSwapchainKHR handle;
    std::vector<VkImage> images;
    std::vector<VkImageView> views;
    VkFormat format;
    VkColorSpaceKHR colorSpace;
    VkExtent2D extent2d;
    VkPresentModeKHR presentMode;
  };
  struct SwapchainSupportDetails
  {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
  };

  //TODO: add option to recreate swapchain from previous one
  class SwapchainContextBuilder
  {
    public:
      SwapchainContextBuilder(DeviceContext device, VkSurfaceKHR surface);
      SwapchainContextBuilder& set_preferred_format(VkFormat format);
      SwapchainContextBuilder& set_preferred_colorspace(VkColorSpaceKHR space);
      SwapchainContextBuilder& set_preferred_present(VkPresentModeKHR mode);
      SwapchainContext build(VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE);
    public:
    private:
      SwapchainSupportDetails query_swapchain_support(VkPhysicalDevice device);
      VkSurfaceFormatKHR choose_surface_format(std::span<VkSurfaceFormatKHR> supportedFormats);
      VkPresentModeKHR choose_present_mode(std::span<VkPresentModeKHR> supportedModes);
      VkExtent2D choose_extent(VkSurfaceCapabilitiesKHR capabilities);
    private:
      DeviceContext m_Device;
      VkSurfaceKHR m_Surface;
      VkFormat m_Format = VK_FORMAT_B8G8R8A8_SRGB;
      VkColorSpaceKHR m_ColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
      VkPresentModeKHR m_PresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
  };
} // namespace aurora

namespace vkutil
{
  std::string stringify_present_mode(VkPresentModeKHR presentMode);
}
