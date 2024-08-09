#pragma once
#include <vulkan/vulkan.h>

namespace vkstartup
{

  class PhysicalDeviceSelector 
  {
    public:
      PhysicalDeviceSelector(VkInstance instance);
    public:
      PhysicalDeviceSelector& set_minimum_version(int major, int minor);
      PhysicalDeviceSelector& set_required_features_13(VkPhysicalDeviceVulkan13Features features);
      PhysicalDeviceSelector& set_required_features_12(VkPhysicalDeviceVulkan12Features features);
      //PhysicalDeviceSelector& set_surface??
    private:
      VkInstance h_Instance;
    private:
  };

  class LogicalDeviceBuilder
  {
    public:
    public:
    private:
    private:
  };

} // namespace vkstartup 


// move debug utils fn here ? use volk
