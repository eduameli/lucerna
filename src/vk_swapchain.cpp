#include "vk_swapchain.h"
#include "ar_asserts.h"
#include "vk_images.h"
#include "window.h"
#include <GLFW/glfw3.h>
namespace Aurora {

SwapchainContextBuilder::SwapchainContextBuilder(DeviceContext device, VkSurfaceKHR surface)
  : m_Device{device}, m_Surface{surface}
{}

SwapchainContextBuilder& SwapchainContextBuilder::set_preferred_format(VkFormat format)
{
  m_Format = format;
  return *this;
}

SwapchainContextBuilder& SwapchainContextBuilder::set_preferred_colorspace(VkColorSpaceKHR space)
{
  m_ColorSpace = space;
  return *this;
}

SwapchainContextBuilder& SwapchainContextBuilder::set_preferred_present(VkPresentModeKHR mode)
{
  m_PresentMode = mode;
  return *this;
}

SwapchainContext SwapchainContextBuilder::build()
{
  SwapchainSupportDetails details = query_swapchain_support(m_Device.physical);
  VkPresentModeKHR presentMode = choose_present_mode(details.presentModes);
  VkSurfaceFormatKHR surfaceFormat = choose_surface_format(details.formats);
  VkExtent2D extent = choose_extent(details.capabilities);

  uint32_t imageCount = details.capabilities.minImageCount + 1;
  if (details.capabilities.maxImageCount > 0 && imageCount > details.capabilities.maxImageCount)
    imageCount = details.capabilities.maxImageCount;
  
  VkSwapchainCreateInfoKHR createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  createInfo.surface = m_Surface;
  createInfo.minImageCount = imageCount;
  createInfo.imageFormat = surfaceFormat.format;
  createInfo.imageColorSpace = surfaceFormat.colorSpace;
  createInfo.imageExtent = extent;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  
  uint32_t familyIndices[] = {m_Device.graphicsIndex, m_Device.presentIndex};

  if (m_Device.graphicsIndex != m_Device.presentIndex)
  {
    createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices = familyIndices;
  }
  else 
  {
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;
    createInfo.pQueueFamilyIndices = nullptr;
  }

  createInfo.preTransform = details.capabilities.currentTransform;
  createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  createInfo.presentMode = presentMode;
  createInfo.clipped = VK_TRUE;
  createInfo.oldSwapchain = VK_NULL_HANDLE;
  
  VkSwapchainKHR swapchain;
  VK_CHECK_RESULT(vkCreateSwapchainKHR(m_Device.logical, &createInfo, nullptr, &swapchain))
  
  uint32_t count = 0;
  vkGetSwapchainImagesKHR(m_Device.logical, swapchain, &count, nullptr);
  std::vector<VkImage> swapchainImages(count);
  vkGetSwapchainImagesKHR(m_Device.logical, swapchain, &count, swapchainImages.data());
  std::vector<VkImageView> swapchainViews = vkutil::get_image_views(m_Device.logical, surfaceFormat.format, swapchainImages);


  return {
    .handle = swapchain,
    .images = std::move(swapchainImages),
    .views = std::move(swapchainViews),
    .format = surfaceFormat.format,
    .colorSpace = surfaceFormat.colorSpace,
    .extent2d = extent,
    .presentMode = presentMode

  };
}

SwapchainSupportDetails SwapchainContextBuilder::query_swapchain_support(VkPhysicalDevice device)
{
  SwapchainSupportDetails details;

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_Surface, &details.capabilities);
  uint32_t formatCount;
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount, nullptr);
  if (formatCount != 0)
  {
    details.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount, details.formats.data());
  }

  uint32_t presentModeCount;
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &presentModeCount, nullptr);
  if (presentModeCount != 0)
  {
    details.presentModes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &presentModeCount, details.presentModes.data());
  }

  return details;
}

VkSurfaceFormatKHR SwapchainContextBuilder::choose_surface_format(std::span<VkSurfaceFormatKHR> supportedFormats)
{
  for (const auto& format : supportedFormats)
  {
    if (format.format == m_Format && format.colorSpace == m_ColorSpace)
    {
      return format;
    }
  }
  return supportedFormats[0];
}

VkPresentModeKHR SwapchainContextBuilder::choose_present_mode(std::span<VkPresentModeKHR> supportedModes)
{
  for (const auto& mode : supportedModes)
    {
      if (mode == m_PresentMode)
      {
        return mode;
      }
  }
  return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D SwapchainContextBuilder::choose_extent(VkSurfaceCapabilitiesKHR capabilities)
{
  if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
  {
    return capabilities.currentExtent;
  }
  else
  {
    int width, height;
    glfwGetFramebufferSize(Aurora::Window::get(), &width, &height);
    VkExtent2D actualExtent = Window::get_extent(); 
    actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.width, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    
    return actualExtent;
  }
}

} // namespace aurora

std::string vkutil::stringify_present_mode(VkPresentModeKHR presentMode)
{
  switch (presentMode) {
    case VK_PRESENT_MODE_IMMEDIATE_KHR:
        return "VK_PRESENT_MODE_IMMEDIATE_KHR";
    case VK_PRESENT_MODE_MAILBOX_KHR:
        return "VK_PRESENT_MODE_MAILBOX_KHR";
    case VK_PRESENT_MODE_FIFO_KHR:
        return "VK_PRESENT_MODE_FIFO_KHR";
    case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
        return "VK_PRESENT_MODE_FIFO_RELAXED_KHR";
    case VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR:
        return "VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR";
    case VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR:
        return "VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR";
    case VK_PRESENT_MODE_MAX_ENUM_KHR:
        return "VK_PRESENT_MODE_MAX_ENUM_KHR";
    default:
        return "UNKNOWN_PRESENT_MODE";
  }
}
