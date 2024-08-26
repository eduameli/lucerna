#pragma once

#include <memory>
#include "spdlog/logger.h"

// NOTE: only one logger?, should it be shared ptr or explicit shutdown method?
namespace Aurora {
  class Logger
  {
  public:
    static void init();
    static std::shared_ptr<spdlog::logger> s_Logger;
  };
}

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
