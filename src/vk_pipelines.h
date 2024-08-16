#pragma once
#include <vulkan/vulkan.h>
namespace vkutil
{
bool load_shader_module(const char* filepath, VkDevice device, VkShaderModule* outShaderModule);
}
