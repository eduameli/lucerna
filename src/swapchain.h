#pragma once
#include <vulkan/vulkan.h>
#include "device.h"

namespace Aurora {

class Swapchain
{
  public:
    Swapchain(Device& device, VkExtent2D extent);
    // FIX: Faster swapchain recreation from older swapchain!
    //Swapchain(Device& device, VkExtent2D extent, Ref<Swapchain> previous);
    ~Swapchain();
  public:
  private:
    //VkExtent2D
  private:
    //NOTE: dynamic rendering so no frambuffers or renderpass?

};

}
