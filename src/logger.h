#pragma once

#include "aurora_pch.h"
#include <volk.h>
#include <vulkan/vulkan_core.h>

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
      static inline LogLevel logLevel{ LogLevel::VERBOSE };
  };
} // namespace aurora


#define STRINGIFY(x) #x

#if (LA_ENABLE_LOGS == 1)
  #define LA_LOG_VERBOSE(...) ::Aurora::Logger::verbose(std::format(__VA_ARGS__))
  #define LA_LOG_DEBUG(...) ::Aurora::Logger::debug(std::format(__VA_ARGS__))
  #define LA_LOG_INFO(...) ::Aurora::Logger::info(std::format(__VA_ARGS__))
  #define LA_LOG_WARN(...) ::Aurora::Logger::warning(std::format(__VA_ARGS__))
  #define LA_LOG_ERROR(...) ::Aurora::Logger::error(std::format(__VA_ARGS__))
  #define LA_LOG_FATAL(...) ::Aurora::Logger::fatal(std::format(__VA_ARGS__))
#else
  #define LA_LOG_VERBOSE(...)
  #define LA_LOG_DEBUG(...)
  #define LA_LOG_INFO(...)
  #define LA_LOG_WARN(...)
  #define LA_LOG_ERROR(...)
  #define LA_LOG_FATAL(...)
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

