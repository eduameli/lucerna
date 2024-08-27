#pragma once

#include <memory>
#include "spdlog/logger.h"
#include <vulkan/vulkan.h>

namespace Aurora {
  class Logger
  {
    public:
      static void init();
    public:
      static std::shared_ptr<spdlog::logger> s_Logger;
    public:
      static VkBool32 validation_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);
      static void setup_validation_layer_callback(
        VkInstance instance,
        VkDebugUtilsMessengerEXT& messenger,
        PFN_vkDebugUtilsMessengerCallbackEXT callback);
      static VkResult create_debug_messenger(
        VkInstance instance,
        const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
        const VkAllocationCallbacks* pAllocator,
        VkDebugUtilsMessengerEXT* pDebugMessenger
      );
      static void destroy_debug_messenger(
        VkInstance instance,
        VkDebugUtilsMessengerEXT debugMessenger,
        const VkAllocationCallbacks* pAllocator
      );
  };
} // namespace aurora

#ifdef DEBUG
  #define AR_CORE_FATAL(...)    ::Aurora::Logger::s_Logger->critical(__VA_ARGS__)
  #define AR_CORE_ERROR(...)    ::Aurora::Logger::s_Logger->error(__VA_ARGS__)
  #define AR_CORE_WARN(...)     ::Aurora::Logger::s_Logger->warn(__VA_ARGS__)
  #define AR_CORE_INFO(...)     ::Aurora::Logger::s_Logger->info(__VA_ARGS__)
  #define AR_CORE_TRACE(...)    ::Aurora::Logger::s_Logger->trace(__VA_ARGS__)
#else
  #define LOG_FATAL(...)
  #define LOG_ERROR(...)
  #define LOG_WARN(...)
  #define LOG_INFO(...)
  #define LOG_TRACE(...)
#endif
