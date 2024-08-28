#pragma once

#include "logger.h" 
#include <vulkan/vulkan.h>

#if AR_ENABLE_ASSERTS == 1
#include <signal.h>

#define AR_STOP raise(SIGTRAP)

#define AR_ASSERT(condition) \
    do { \
        if (!(condition)) { \
          AR_CORE_FATAL("ASSERT FAILED ({}) \t [{}, {}, {}]", #condition, __FILE__, __FUNCTION__, __LINE__); \
          AR_STOP; \
        } \
    } while (false)


#define AR_LOG_ASSERT(condition, ...) \
      do { \
          if (!(condition)) { \
            AR_CORE_FATAL("ASSERT FAILED ({}) [{}, {}, {}]", #condition, __FILE__, __FUNCTION__, __LINE__); \
            AR_CORE_FATAL("{}", fmt::format(__VA_ARGS__)); \
            AR_STOP; \
          } \
      } while (false)


#else
#define AR_ASSERT(condition, ...)
#define AR_LOG_ASSERT(condition, ...)
#define AR_STOP
#endif


inline std::string error_to_string(VkResult errorCode)
  {
    switch (errorCode)
    {
#define STR(r) case VK_ ##r: return #r
      STR(NOT_READY);
      STR(TIMEOUT);
      STR(EVENT_SET);
      STR(EVENT_RESET);
      STR(INCOMPLETE);
      STR(ERROR_OUT_OF_HOST_MEMORY);
      STR(ERROR_OUT_OF_DEVICE_MEMORY);
      STR(ERROR_INITIALIZATION_FAILED);
      STR(ERROR_DEVICE_LOST);
      STR(ERROR_MEMORY_MAP_FAILED);
      STR(ERROR_LAYER_NOT_PRESENT);
      STR(ERROR_EXTENSION_NOT_PRESENT);
      STR(ERROR_FEATURE_NOT_PRESENT);
      STR(ERROR_INCOMPATIBLE_DRIVER);
      STR(ERROR_TOO_MANY_OBJECTS);
      STR(ERROR_FORMAT_NOT_SUPPORTED);
      STR(ERROR_SURFACE_LOST_KHR);
      STR(ERROR_NATIVE_WINDOW_IN_USE_KHR);
      STR(SUBOPTIMAL_KHR);
      STR(ERROR_OUT_OF_DATE_KHR);
      STR(ERROR_INCOMPATIBLE_DISPLAY_KHR);
      STR(ERROR_VALIDATION_FAILED_EXT);
      STR(ERROR_INVALID_SHADER_NV);
      STR(ERROR_INCOMPATIBLE_SHADER_BINARY_EXT);
#undef STR
    default:
      return "UNKNOWN_ERROR";
    }
  }

#define VK_CHECK_RESULT(f)																				\
{																										\
  VkResult res = (f);																					\
  if (res != VK_SUCCESS)																				\
  {																									\
    AR_CORE_ERROR("Error: VkResult is [{}] in {} at line {}", error_to_string(res), __FILE__, __LINE__); \
    assert(res == VK_SUCCESS);																		\
  }																									\
}
