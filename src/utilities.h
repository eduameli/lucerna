#pragma once
#include "aurora_pch.h"
#include "log.h" 
#include <vulkan/vulkan.h>
namespace Aurora
{
  struct Timer
  {
    std::chrono::high_resolution_clock::time_point start, end;
    std::chrono::duration<float> duration;
    std::string label{};

    Timer()
    {
      start = std::chrono::high_resolution_clock::now();
    }
    Timer(const std::string& input)
    {
      start = std::chrono::high_resolution_clock::now();
      label = std::move(input);
    }
    ~Timer()
    {
      end = std::chrono::high_resolution_clock::now();
      duration = end - start;
      AR_INFO("[{}]'s timer took {}ms [{} microsecond], ", label, std::chrono::duration_cast<std::chrono::milliseconds>(duration).count(), std::chrono::duration_cast<std::chrono::microseconds>(duration).count());
    }
  };


  #if AR_ENABLE_ASSERTS == 1
  #include <signal.h>
  #define AR_ASSERT(condition, ...) \
      do { \
          if (!(condition)) { \
            AR_CORE_FATAL("ASSERT FAILED [{}, {}, {}]", __FILE__, __FUNCTION__, __LINE__); \
            AR_CORE_FATAL("{}", fmt::format(__VA_ARGS__)); \
            raise(SIGTRAP); \
          } \
      } while (false)



  #else
  #define AR_ASSERT(condition, ...)
  #endif

  
  inline std::string errorString(VkResult errorCode)
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
		  AR_CORE_ERROR("Error: VkResult is [{}] in {} at line {}", errorString(res), __FILE__, __LINE__); \
		  assert(res == VK_SUCCESS);																		\
	  }																									\
  }
  
}
