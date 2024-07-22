#pragma once

#include "aurora_pch.h"
#include "spdlog/logger.h"
#include "spdlog/fmt/fmt.h"

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
// TODO: instead of ifdef should be if DEBUG = 1


#ifdef DEBUG
#include <signal.h>

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

// FIXME: move to an utilities class! + add VK_CHECK_RESULT
#if AR_ENABLE_ASSERTS == 1
#include <signal.h>
#define AR_ASSERT(condition, ...) \
    do { \
        if (!(condition)) { \
            AR_CORE_FATAL("ASSERT FAILED [{}, {}, {}]", __FILE__, __FUNCTION__, __LINE__); \
            AR_CORE_FATAL("{}", fmt::format(__VA_ARGS__)); \
            AR_STOP; \
        } \
    } while (false)



#else
#define AR_ASSERT(condition)
#endif


/* EXAMPLE
#define VK_CHECK(x)                                                 \
	do                                                              \
	{                                                               \
		VkResult err = x;                                           \
		if (err)                                                    \
		{                                                           \
			LOGE("Detected Vulkan error: {}", vkb::to_string(err)); \
			abort();                                                \
		}                                                           \
	} while (0)

#define ASSERT_VK_HANDLE(handle)        \
	do                                  \
	{                                   \
		if ((handle) == VK_NULL_HANDLE) \
		{                               \
			LOGE("Handle is NULL");     \
			abort();                    \
		}                               \
	} while (0)
*/
