#include "logger.h"

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace Aurora {
  std::shared_ptr<spdlog::logger> Logger::s_Logger;
  void Logger::init()
  {
    spdlog::set_pattern("%^[%T] [%n] %v%$");
    spdlog::set_level(spdlog::level::trace);
    s_Logger = spdlog::stdout_color_mt("aurora");
  }
}
