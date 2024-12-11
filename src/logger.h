#pragma once

#include "aurora_pch.h"
#include "spdlog/logger.h"
#include <volk.h>
#include <vulkan/vulkan_core.h>

// Console Logger
namespace Aurora {
  class Logger
  {
    public:
      static void init();
      static VkBool32 validation_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);
      static void setup_validation_layer_callback(
        VkInstance instance,
        VkDebugUtilsMessengerEXT& messenger,
        PFN_vkDebugUtilsMessengerCallbackEXT callback);
    public:
      static std::shared_ptr<spdlog::logger> s_Logger;
  };
} // namespace aurora


#define STRINGIFY(x) #x

#if (AR_LOG_LEVEL == 1)
  #define AR_CORE_FATAL(...)    ::Aurora::Logger::s_Logger->critical(__VA_ARGS__)
  #define AR_CORE_ERROR(...)    ::Aurora::Logger::s_Logger->error(__VA_ARGS__)
  #define AR_CORE_WARN(...)     ::Aurora::Logger::s_Logger->warn(__VA_ARGS__)
  #define AR_CORE_INFO(...)     ::Aurora::Logger::s_Logger->info(__VA_ARGS__)
  #define AR_CORE_TRACE(...)    ::Aurora::Logger::s_Logger->trace(__VA_ARGS__)
#else
  #define AR_CORE_FATAL(...)    
  #define AR_CORE_ERROR(...)    
  #define AR_CORE_WARN(...)     
  #define AR_CORE_INFO(...)     
  #define AR_CORE_TRACE(...)    
#endif


namespace vklog {
  inline void label_image(VkDevice device, VkImage image, const char* name)
  {
#ifdef AR_DEBUG 
    VkDebugUtilsObjectNameInfoEXT nameInfo{ .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT, .pNext = nullptr };
    nameInfo.objectType = VK_OBJECT_TYPE_IMAGE;
    nameInfo.objectHandle = reinterpret_cast<uint64_t> (image);
    nameInfo.pObjectName = name;
    vkSetDebugUtilsObjectNameEXT(device, &nameInfo);
#endif
  }

  inline void label_buffer(VkDevice device, VkBuffer buffer, const char* name)
  {
#ifdef AR_DEBUG 
    VkDebugUtilsObjectNameInfoEXT nameInfo{ .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT, .pNext = nullptr };
    nameInfo.objectType = VK_OBJECT_TYPE_BUFFER;
    nameInfo.objectHandle = reinterpret_cast<uint64_t> (buffer);
    nameInfo.pObjectName = name;
    vkSetDebugUtilsObjectNameEXT(device, &nameInfo);
#endif
  }

  
  inline void label_pipeline(VkDevice device, VkPipeline pipeline, const char* name)
  {
#ifdef AR_DEBUG 
    VkDebugUtilsObjectNameInfoEXT nameInfo{ .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT, .pNext = nullptr };
    nameInfo.objectType = VK_OBJECT_TYPE_PIPELINE;
    nameInfo.objectHandle = reinterpret_cast<uint64_t> (pipeline);
    nameInfo.pObjectName = name;
    vkSetDebugUtilsObjectNameEXT(device, &nameInfo);
#endif
  }
}

