#pragma once
#include "aurora_pch.h"

#include "spdlog/logger.h"
#include "spdlog/fmt/fmt.h"

namespace Aurora {
  class Logger
  {
  public:
    static void init();
    inline static Aurora::Ref<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }    
    inline static Aurora::Ref<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }
  private:
    static Aurora::Ref<spdlog::logger> s_CoreLogger;
    static Aurora::Ref<spdlog::logger> s_ClientLogger;
  };
}

// logging macros
// TODO: instead of ifdef should be if DEBUG = 1


#ifdef DEBUG
#include <signal.h>

  #define AR_CORE_FATAL(...)    ::Aurora::Logger::GetCoreLogger()->critical(__VA_ARGS__)
  #define AR_CORE_ERROR(...)    ::Aurora::Logger::GetCoreLogger()->error(__VA_ARGS__)
  #define AR_CORE_WARN(...)     ::Aurora::Logger::GetCoreLogger()->warn(__VA_ARGS__)
  #define AR_CORE_INFO(...)     ::Aurora::Logger::GetCoreLogger()->info(__VA_ARGS__)
  #define AR_CORE_TRACE(...)    ::Aurora::Logger::GetCoreLogger()->trace(__VA_ARGS__)

  #define AR_FATAL(...)         ::Aurora::Logger::GetClientLogger()->critical(__VA_ARGS__)
  #define AR_ERROR(...)         ::Aurora::Logger::GetClientLogger()->error(__VA_ARGS__)
  #define AR_WARN(...)          ::Aurora::Logger::GetClientLogger()->warn(__VA_ARGS__)
  #define AR_INFO(...)          ::Aurora::Logger::GetClientLogger()->info(__VA_ARGS__)
  #define AR_TRACE(...)         ::Aurora::Logger::GetClientLogger()->trace(__VA_ARGS__)
  

  #define AR_STOP raise(SIGTRAP) 

#else

  #define AR_CORE_FATAL(...)
  #define AR_CORE_ERROR(...)
  #define AR_CORE_WARN(...)
  #define AR_CORE_INFO(...)
  #define AR_CORE_TRACE(...)

  #define AR_FATAL(...)
  #define AR_ERROR(...)
  #define AR_WARN(...)
  #define AR_INFO(...)
  #define AR_TRACE(...)
  
  #define STOP std::exit();  

#endif
