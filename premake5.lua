--TODO: REFACTOR BUILD SYSTEM, BUILD GLFW SYSTEM INDEPENDENT
workspace "aurora"
    configurations { "debug", "release" }
    architecture "x86_64"
    toolset "clang"
    linkoptions {"-fuse-ld=mold"}

project "aurora"
    kind "WindowedApp"
    language "C++"
    cppdialect "c++20"
    targetdir ("build/bin/%{cfg.buildcfg}")
    objdir ("build/obj/%{cfg.buildcfg}")
    
    defines 
    {
    	"GLM_ENABLE_EXPERIMENTAL",
      "GLM_FORCE_RADIANS",
      "GLM_FORCE_DEPTH_ZERO_TO_ONE",
      --"GLM_FORCE_LEFT_HANDED",
    }
    
    pchheader "src/aurora_pch.h"
    
    files 
    {
        "src/**.h",
        "src/**.cpp",
    }

    includedirs 
    {
        "vendor/spdlog/include",
        "vendor/glfw/include",
        "vendor/KHR",
        "vendor/vulkan/include",
        "vendor/VulkanMemoryAllocator/include",
        "vendor/imgui",
        "vendor/glm/",
        "vendor/stb_image/",
        "vendor/fastgltf/include",
        "vendor/volk/"
    }
  
    libdirs {"build/lib/bin/%{cfg.buildcfg}"}

    links 
    {
        "spdlog",
        "glfw",
        "imgui",
        "fastgltf",
        "volk",
    }
    
    filter {}

    postbuildcommands {
      "premake export-compile-commands",
    }
  
    buildoptions {"-Wno-nullability-completeness"}

    filter "configurations:debug"
        defines { "AR_LOG_LEVEL=1", "AR_ENABLE_ASSERTS=1", "DEBUG"}
        symbols "On"
        optimize "Off"

    filter "configurations:release"
        defines { "AR_LOG_LEVEL=1", "AR_ENABLE_ASSERTS=1"}
        optimize "On"

project "spdlog"
    location "vendor/spdlog"
    kind "StaticLib"
    language "C++"
    cppdialect "c++20"
    targetdir ("build/lib/bin/%{cfg.buildcfg}")
    objdir ("build/lib/obj/%{cfg.buildcfg}")

    defines
    {
        "SPDLOG_COMPILED_LIB",
    }

    includedirs
    {
        "vendor/spdlog/include"
    }

    files
    {
        "vendor/spdlog/include/spdlog/**.h", -- maybe this can be removed as its already in included dirs
        "vendor/spdlog/src/**.cpp"
    }

    filter "configurations:debug"
        symbols "On"

    filter "configurations:release"
        optimize "On"

project "glfw"
  location "vendor/glfw"
  kind "StaticLib"
  language "C++"
  cppdialect "c++20"
  targetdir ("build/lib/bin/%{cfg.buildcfg}")
  objdir("build/lib/obj/%{cfg.buildcfg}")

  includedirs
  {
    "vendor/glfw/include",
    "vendor/KHR/",
  }
  
  defines {
    --"_GLFW_WAYLAND",
    "_GLFW_X11",
    --"GLFW_VULKAN_STATIC",
  }
  
  files
  {
    "vendor/glfw/include/GLFW/**.h",
    "vendor/glfw/src/**.c", -- is this needed?
  }

  filter "configurations:debug"
    symbols "On"
  
  filter "configurations:release"
    optimize "On"

project "imgui"
    location "vendor/imgui"
    kind "StaticLib"
    language "C++"
    cppdialect "c++20"
    targetdir ("build/lib/bin/%{cfg.buildcfg}")
    objdir ("build/lib/obj/%{cfg.buildcfg}")

    includedirs
    {
        "vendor/imgui",
        "vendor/imgui/backends",

        "vendor/vulkan/include",
        "vendor/glfw/include",
        "vendor/KHR/",
    }
    
    defines
    {
      "IMGUI_IMPL_VULKAN_USE_VOLK"
    }
    files
    {
        "vendor/imgui/*.cpp",
        "vendor/imgui/*.h",
        "vendor/imgui/backends/imgui_impl_glfw.cpp",
        "vendor/imgui/backends/imgui_impl_glfw.h",
        "vendor/imgui/backends/imgui_impl_vulkan.cpp",
        "vendor/imgui/backends/imgui_impl_vulkan.h"
    }

    filter "configurations:debug"
        symbols "On"

    filter "configurations:release"
        optimize "On"

project "fastgltf"
    location "vendor/fastgltf"
    kind "StaticLib"
    language "C++"
    targetdir ("build/lib/bin/%{cfg.buildcfg}")
    objdir ("build/lib/obj/%{cfg.buildcfg}")
    
    includedirs
    {
        "vendor/fastgltf/include",
        "vendor/simdjson/singleheader/" -- Updated path to the correct include directory
    }


   files {
      "vendor/simdjson/singleheader/**.cpp",  -- Include simdjson source files for compilation
      "vendor/fastgltf/src/**",
   }
 
    
    filter "configurations:debug"
        symbols "On"

    filter "configurations:release"
        optimize "On"

project "volk"
  location "vendor/volk"
  kind "StaticLib"
  language "C++"
  targetdir ("build/lib/bin/%{cfg.buildcfg}")
  objdir ("build/lib/obj/%{cfg.buildcfg}")

  defines 
  {
    "VOLK_STATIC_DEFINES",
    "VK_USE_PLATFORM_XLIB_KHR",
  }

  includedirs
  {
    "vendor/volk/*.h"
  }
  files
  {
    "vendor/volk/*.c"
  }
  filter "configurations:debug"
    symbols "On"

  filter "configurations:release"
    optimize "On"
