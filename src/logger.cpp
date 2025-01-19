#include "logger.h"
#include "ar_asserts.h"
#include "vulkan/vulkan_core.h"
#include <ctime>
#include <print>
#include <sstream>

namespace Aurora {

constexpr std::string_view log_level_to_string(LogLevel level)
{
  switch (level)
  {
    case LogLevel::VERBOSE: return "\033[97;1mverbose\033[0m";
    case LogLevel::DEBUG: return "\033[35;1mdebug\033[0m";
    case LogLevel::INFO: return "\033[94;1minfo\033[0m";
    case LogLevel::WARNING: return "\033[93;1mwarn\033[0m";
    case LogLevel::ERROR: return "\033[93;1mwarn\033[0m";
    case LogLevel::FATAL: return "\033[91;1merror\033[0m";
    default: return "unknown";
  }
}

void Logger::set_min_log_level(LogLevel level)
{
  logLevel = level;
}

void Logger::verbose(std::string_view message)
{
  log(LogLevel::VERBOSE, message);
}

void Logger::debug(std::string_view message)
{
  
  log(LogLevel::DEBUG, message);
}

void Logger::info(std::string_view message)
{
  
  log(LogLevel::INFO, message);
}

void Logger::warning(std::string_view message)
{
  
  log(LogLevel::WARNING, message);
}

void Logger::error(std::string_view message)
{
  
  log(LogLevel::ERROR, message);
}

void Logger::fatal(std::string_view message)
{
  
  log(LogLevel::FATAL, message);
}

void Logger::log(LogLevel level, std::string_view message)
{
  std::ostringstream oss;
  
  if (logLevel > level)
  {
    return;
  }

  
  std::time_t time = std::time(nullptr);
  std::tm localTime = *std::localtime(&time);

  oss << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
  std::print("[{0}] [{1}] {2}\n", oss.str(), log_level_to_string(level), message);
}




  
} // namespace aurora

VkBool32 vklog::validation_callback(
  VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
  VkDebugUtilsMessageTypeFlagsEXT messageType,
  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
  void* pUserData)
{
  if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
  {
    Aurora::Logger::error(pCallbackData->pMessage);
  }
  return VK_FALSE;
}

void vklog::setup_validation_layer_callback(VkInstance instance, VkDebugUtilsMessengerEXT& messenger, PFN_vkDebugUtilsMessengerCallbackEXT callback)
{
  VkDebugUtilsMessengerCreateInfoEXT createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  createInfo.pfnUserCallback = callback;
  VK_CHECK_RESULT(vkCreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &messenger));
}
