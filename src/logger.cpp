#include "logger.h"

#include "ar_asserts.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"

namespace Aurora {
  std::shared_ptr<spdlog::logger> Logger::s_Logger;
  void Logger::init()
  {
    spdlog::set_pattern("%^[%T] %v%$");
    spdlog::set_level(spdlog::level::trace);
    s_Logger = spdlog::stdout_color_mt("aurora");
  }

  VkBool32 Logger::validation_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
  {
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
      AR_CORE_ERROR("VALIDATION LAYER ERROR: {}", pCallbackData->pMessage);
    }
    return VK_FALSE;
  }

  void Logger::setup_validation_layer_callback(VkInstance instance, VkDebugUtilsMessengerEXT& messenger, PFN_vkDebugUtilsMessengerCallbackEXT callback)
  {
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = callback;
    VK_CHECK_RESULT(vkCreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &messenger));
  }

  // Visual Benchmark
  static TracyBenchmark* s_Instance = nullptr;  
  TracyBenchmark& TracyBenchmark::get()
  {
    AR_LOG_ASSERT(s_Instance != nullptr, "Tracy Profiling must be initialised!");
    return *s_Instance;
  }

  void TracyBenchmark::init(std::string_view name, std::filesystem::path path)
  {
    if (s_Instance == nullptr)
      s_Instance = this;
  
    if (isActive)
      shutdown();
    
    isActive = true;
    AR_CORE_ERROR(path.c_str());  
    s_Logger = spdlog::basic_logger_mt(name.data(), path.c_str());
    s_Logger->set_pattern("%v"); 
    sessionName = name;
    s_Logger->trace("{\"otherData\": {},\"traceEvents\":[");
  }

  void TracyBenchmark::shutdown()
  {
    if (!isActive)
      return;

    isActive = false;
    s_Logger->trace("]}");
    s_Logger->flush();
    counter = 0;
  }
  
  void TracyBenchmark::write(const ProfileResult& result)
  {
    std::lock_guard<std::mutex> locked(lock);

    if (counter++ > 0)
      s_Logger->trace(",");
    
    std::string name = result.name;
    std::replace(name.begin(), name.end(), '"', '\'');
    
    s_Logger->trace("{");
    s_Logger->trace("\"cat\":\"function\",");
    s_Logger->trace("\"dur\":{},", result.end - result.start);
    s_Logger->trace("\"name\":\"{}\",", name);
    s_Logger->trace("\"ph\":\"X\",");
    s_Logger->trace("\"pid\":0,");
    s_Logger->trace("\"tid\":{},", std::hash<std::thread::id>{}(result.threadID));
    s_Logger->trace("\"ts\":{}", result.start);
    s_Logger->trace("}");
  }
  TracyTimer::TracyTimer(std::string_view name)
  {
    result.name = name;
    start = std::chrono::steady_clock::now();
  }

  TracyTimer::~TracyTimer()
  {
    auto end = std::chrono::steady_clock::now();
    result.start = std::chrono::time_point_cast<std::chrono::microseconds>(start).time_since_epoch().count();
    result.end = std::chrono::time_point_cast<std::chrono::microseconds>(end).time_since_epoch().count();
    result.threadID = std::this_thread::get_id();
    TracyBenchmark::get().write(result);
  }

} // aurora namespace
