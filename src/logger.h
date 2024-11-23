#pragma once

#include "aurora_pch.h"
#include "spdlog/logger.h"
#include <volk.h>

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

// Profiling
namespace Aurora {
 struct Timer
  {
    std::chrono::time_point<std::chrono::steady_clock> start, end;
    std::chrono::duration<float> duration;
    std::string_view label;

    Timer()
      : Timer("unnamed")
    {}
    
    Timer(std::string_view string)
    {
      start = std::chrono::steady_clock::now();
      label = string;
    }

    ~Timer()
    {
      end = std::chrono::steady_clock::now();
      duration = end - start;
      
      AR_CORE_TRACE("{}: {}", label, std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());
    }  
  };
  
  // Visual Benchmark
  // FIXME:
  // Benchmark::init(name, filepath)
  // Benchmark::shutdown()
  // fully static - accessible anywhere
  struct ProfileResult
  {
    std::string name;
    int64_t start, end;
    std::thread::id threadID;
  };

  struct TracyBenchmark
  {
    void init(std::string_view name, std::filesystem::path path);
    static TracyBenchmark& get();
    void write(const ProfileResult& result);
    void shutdown();

    std::shared_ptr<spdlog::logger> s_Logger;
    std::string sessionName{ "Unnamed" };
    int counter{ 0 };
    std::mutex lock;
    bool isActive{ false };
    
  };

  struct TracyTimer
  {
    ProfileResult result;
    std::chrono::time_point<std::chrono::steady_clock> start, end;
    
    TracyTimer(std::string_view name);
    ~TracyTimer();
  };

} // aurora namespace

#if (AR_LOG_LEVEL == 1)
 #define AR_PROFILE_SCOPE() Aurora::Timer timer(__FUNCTION__)
 #define AR_BENCHMARK() Aurora::TracyTimer timer##__LINE__(__FUNCTION__)
#else
  #define AR_PROFILE_SCOPE
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
}

