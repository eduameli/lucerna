#pragma once
#include <optional>
#include <vulkan/vulkan.h>
#include "vk_mem_alloc.h"
#include <functional>
#include <deque>
namespace Aurora
{
  struct AllocatedImage
  {
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VkExtent3D imageExtent;
    VkFormat imageFormat;
  };
  
  struct QueueFamilyIndices
  {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    bool is_complete() {
      return graphicsFamily.has_value() && presentFamily.has_value();
    }
  };
  
  struct Features
    {
      VkPhysicalDeviceFeatures2 features{};
      VkPhysicalDeviceFeatures f1{};
      VkPhysicalDeviceVulkan11Features f11{};
      VkPhysicalDeviceVulkan12Features f12{};
      VkPhysicalDeviceVulkan13Features f13{};
      VkPhysicalDeviceFeatures2 get()
      {
        f11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
        f12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        f13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

        f11.pNext = &f12;
        f12.pNext = &f13;

        features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        features.pNext = &f11;
        features.features = f1;

        return features;
      }
  };
  
  struct DeletionQueue
  {
    std::deque<std::function<void()>> deletors;
    void push_function(std::function<void()>&& function)
    {
      deletors.push_back(function);
    }
    
    void flush()
    {
      for (auto it = deletors.rbegin(); it != deletors.rend(); it++)
      {
        (*it)();
      }
      deletors.clear();
    }
  };
}

