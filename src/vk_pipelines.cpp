#include "vk_pipelines.h"
#include <fstream>
#include "aurora_pch.h"
#include "vk_initialisers.h"

// FIX: return optional & use std::path?
bool vkutil::load_shader_module(const char *filepath, VkDevice device, VkShaderModule* outShaderModule)
{
  std::ifstream file(filepath, std::ios::ate | std::ios::binary);
  
  if (!file.is_open())
  {
    AR_CORE_ERROR("FAILED TO OPEN FILE!");
    return false;
  }

  size_t fileSize = (size_t) file.tellg();

  std::vector<uint32_t> buffer (fileSize / sizeof(uint32_t));

  file.seekg(0);

  file.read((char*) buffer.data(), fileSize);

  file.close();

  VkShaderModuleCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  info.pNext = nullptr;
  info.codeSize = buffer.size() * sizeof(uint32_t);
  info.pCode = buffer.data();

  VkShaderModule shaderModule;
  if (vkCreateShaderModule(device, &info, nullptr, &shaderModule) != VK_SUCCESS)
  {
    return false;
  }

  *outShaderModule = shaderModule;
  return true;
}
