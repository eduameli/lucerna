#include "vk_startup.h"
#include "application.h"
#include "ar_asserts.h"
#include "vk_device.h"

namespace vks
{

SwapchainBuilder::SwapchainBuilder(
VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, Aurora::QueueFamilyIndices indices)
  : h_PhysicalDevice{physicalDevice}, h_Device{device}, h_Surface{surface}, m_Indices{indices}
{}


void SwapchainBuilder::build(VkSwapchainKHR& swapchain, std::vector<VkImage>& images, VkSurfaceFormatKHR& surfaceFormat, VkExtent2D& extent)
{
  SwapchainSupportDetails details = query_swapchain_support(h_PhysicalDevice);
  VkPresentModeKHR presentMode = choose_present_mode(details.presentModes);
  surfaceFormat = choose_surface_format(details.formats);
  extent = choose_extent(details.capabilities);

  uint32_t imageCount = details.capabilities.minImageCount + 1;
  if (details.capabilities.maxImageCount > 0 && imageCount > details.capabilities.maxImageCount)
  {
    imageCount = details.capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  createInfo.surface = h_Surface;
  createInfo.minImageCount = imageCount;
  createInfo.imageFormat = surfaceFormat.format;
  createInfo.imageColorSpace = surfaceFormat.colorSpace;
  createInfo.imageExtent = extent;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

  uint32_t queueFamilyIndices[] = {m_Indices.graphics.value(), m_Indices.present.value()};

  if (m_Indices.graphics != m_Indices.present)
  {
    createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices = queueFamilyIndices;
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
  
  VK_CHECK_RESULT(vkCreateSwapchainKHR(h_Device, &createInfo, nullptr, &swapchain));
  
  uint32_t count = 0;
  vkGetSwapchainImagesKHR(h_Device, swapchain, &count, nullptr);
  images.resize(count);
  vkGetSwapchainImagesKHR(h_Device, swapchain, &count, images.data());

}

SwapchainBuilder::SwapchainSupportDetails SwapchainBuilder::query_swapchain_support(VkPhysicalDevice device)
{
  SwapchainSupportDetails details;

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, h_Surface, &details.capabilities);
  uint32_t formatCount;
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, h_Surface, &formatCount, nullptr);
  if (formatCount != 0)
  {
    details.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, h_Surface, &formatCount, details.formats.data());
  }

  uint32_t presentModeCount;
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, h_Surface, &presentModeCount, nullptr);
  if (presentModeCount != 0)
  {
    details.presentModes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, h_Surface, &presentModeCount, details.presentModes.data());
  }

  return details;
}

VkSurfaceFormatKHR SwapchainBuilder::choose_surface_format(const std::vector<VkSurfaceFormatKHR>& supportedFormats)
{
  for (const auto& format : supportedFormats)
  {
    if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
    {
      return format;
    }
  }
  return supportedFormats[0];
}

VkPresentModeKHR SwapchainBuilder::choose_present_mode(const std::vector<VkPresentModeKHR>& supportedModes)
{
  for (const auto& mode : supportedModes)
    {
      if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
      {
        AR_CORE_INFO("Using VK_PRESENT_MODE_MAILBOX_KHR");
        return mode;
      }
  }
  AR_CORE_INFO("Using VK_PRESENT_MODE_FIFO_KHR");
  return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D SwapchainBuilder::choose_extent(const VkSurfaceCapabilitiesKHR& capabilities)
{

  if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
  {
    return capabilities.currentExtent;
  }
  else
  {
    int width, height;
    glfwGetFramebufferSize(Aurora::Window::get_handle(), &width, &height);
    VkExtent2D actualExtent = {
      (uint32_t) (width),
      (uint32_t) (height)
    };

    actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.width, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    
    return actualExtent;
  }
}

} // namespace vkstartup
