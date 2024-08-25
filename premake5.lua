--TODO: REFACTOR BUILD SYSTEM, BUILD GLFW SYSTEM INDEPENDENT

workspace "aurora"
    configurations { "debug", "release" }
    architecture "x86_64"
    linkoptions {"-fuse-ld=mold"}
    
project "aurora"
    kind "WindowedApp"
    language "C++"
    cppdialect "c++20"
    targetdir ("build/bin/%{cfg.buildcfg}")
    objdir ("build/obj/%{cfg.buildcfg}")
    
    defines 
    {
    	"SPDLOG_COMPILED_LIB",
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
        "vendor/glfw/include", -- if they are needed here are they also needed when compiling the static lib?
        "vendor/KHR",
        "vendor/vulkan/include",
        "vendor/VulkanMemoryAllocator/include",
        "vendor/imgui",
        "vendor/glm/",
        "vendor/stb_image/",
        "vendor/fastgltf/include",
    }
  
    libdirs {"build/lib/bin/%{cfg.buildcfg}"}

    links 
    {
        "spdlog",
        "glfw",
        "vulkan",
        "imgui",
        "fastgltf",
    }
    
    filter {}

    postbuildcommands {
      "premake export-compile-commands",
    }
  
    filter "configurations:debug"
        defines { "DEBUG", "AR_ENABLE_ASSERTS=1"}
        symbols "On"
        optimize "Off"

    filter "configurations:release"
        defines { "NDEBUG" }
        optimize "On"




project "spdlog"
    location "vendor/spdlog"
    kind "StaticLib"
    language "C++"
    cppdialect "c++20"
    targetdir ("build/lib/bin/%{cfg.buildcfg}")
    objdir ("build/lib/obj/%{cfg.buildcfg}")
    --staticruntime "On"	

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
 
    
    filter "configurations:Debug"
        symbols "On"

    filter "configurations:Release"
        optimize "On"
