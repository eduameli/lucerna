#pragma once

#include "aurora_pch.h"
//#include "spdlog/spdlog.h"
//#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/logger.h"
namespace Aurora {
  class Log
  {
  public:
    static void Init();
    inline static Aurora::Ref<spdlog::logger>& GetCoreLogger() { return s_CoreLogger; }    
    inline static Aurora::Ref<spdlog::logger>& GetClientLogger() { return s_ClientLogger; }
  private:
    static Aurora::Ref<spdlog::logger> s_CoreLogger;
    static Aurora::Ref<spdlog::logger> s_ClientLogger;
  };
}

// logging macros

#ifdef DEBUG
  #define AR_CORE_FATAL(...)    ::Aurora::Log::GetCoreLogger()->critical(__VA_ARGS__)
  #define AR_CORE_ERROR(...)    ::Aurora::Log::GetCoreLogger()->error(__VA_ARGS__)
  #define AR_CORE_WARN(...)     ::Aurora::Log::GetCoreLogger()->warn(__VA_ARGS__)
  #define AR_CORE_INFO(...)     ::Aurora::Log::GetCoreLogger()->info(__VA_ARGS__)
  #define AR_CORE_TRACE(...)    ::Aurora::Log::GetCoreLogger()->trace(__VA_ARGS__)

  #define AR_FATAL(...)         ::Aurora::Log::GetClientLogger()->critical(__VA_ARGS__)
  #define AR_ERROR(...)         ::Aurora::Log::GetClientLogger()->error(__VA_ARGS__)
  #define AR_WARN(...)          ::Aurora::Log::GetClientLogger()->warn(__VA_ARGS__)
  #define AR_INFO(...)          ::Aurora::Log::GetClientLogger()->info(__VA_ARGS__)
  #define AR_TRACE(...)         ::Aurora::Log::GetClientLogger()->trace(__VA_ARGS__)
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

#endif
