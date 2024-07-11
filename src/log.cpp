#include "aurora_pch.h"
//#include "log.h"

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace Aurora {
  Ref<spdlog::logger> Log::s_CoreLogger;
  Ref<spdlog::logger> Log::s_ClientLogger;
  void Log::Init()
  {
    spdlog::set_pattern("%^[%T] %n: %v%$");
    s_CoreLogger = spdlog::stdout_color_mt("AURORA");
    s_CoreLogger->set_level(spdlog::level::trace);
    s_ClientLogger = spdlog::stdout_color_mt("APP");
    s_ClientLogger->set_level(spdlog::level::trace);
  }
}
