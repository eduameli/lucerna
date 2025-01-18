#pragma once

#include "aurora_pch.h"
#include <volk.h>
#include <vulkan/vulkan_core.h>
#include <string_view>

namespace Aurora {

  enum class LogLevel
  {
    VERBOSE,
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    FATAL,
  };

  
  class Logger
  {
    public:
      static void set_min_log_level(LogLevel level);
      static void verbose(std::string_view message);
      static void debug(std::string_view message);      
      static void info(std::string_view message);
      static void warning(std::string_view message);
      static void error(std::string_view message);
      static void fatal(std::string_view message);
    private:
      static void log(LogLevel, std::string_view message);
    private:
      static inline LogLevel logLevel{ LogLevel::INFO };
  };
} // namespace aurora


#define STRINGIFY(x) #x

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

  VkBool32 validation_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData);
  void setup_validation_layer_callback(
    VkInstance instance,
    VkDebugUtilsMessengerEXT& messenger,
    PFN_vkDebugUtilsMessengerCallbackEXT callback);
}

