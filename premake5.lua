--TODO: REFACTOR BUILD SYSTEM, BUILD GLFW SYSTEM INDEPENDENT

workspace "aurora"
    configurations { "debug", "release" }
    architecture "x86_64"

project "aurora"
    kind "WindowedApp"
    language "C++"
    cppdialect "C++17"
    targetdir ("build/bin/%{cfg.buildcfg}")
    objdir ("build/obj/%{cfg.buildcfg}")
    
    defines 
    {
    	"SPDLOG_COMPILED_LIB",
    }
    
    pchheader "/home/eduameli/Desktop/vulkan/attempt 1/aurora/src/aurora_pch.h"
    
    files 
    {
        "src/**.h",
        "src/**.cpp",
    }

    includedirs 
    {
        "vendor/spdlog/include",
        "vendor/glfw/include", -- if they are needed here are they also needed when compiling the static lib?
    }
  
    libdirs {"build/lib"}

    links 
    {
        "spdlog",
        "glfw",
        "vulkan",
    }
    
    filter {}

    postbuildcommands {
      "premake export-compile-commands",
    }
  
    filter "configurations:debug"
        defines { "DEBUG" }
        symbols "On"

    filter "configurations:release"
        defines { "NDEBUG" }
        optimize "On"




project "spdlog"
    location "vendor/spdlog"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
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
  cppdialect "C++17"
  targetdir ("build/lib/bin/%{cfg.buildcfg}")
  objdir("build/lib/obj/%{cfg.buildcfg}")

  includedirs
  {
    "vendor/glfw/include"
  }
	defines {
		--"_GLFW_WAYLAND",
		"_GLFW_X11",
		"_CRT_SECURE_NO_WARNINGS",
	}
  files
  {
    "vendor/glfw/include/GLFW/**.h",
    --"vendor/glfw/src/**.c",
    "vendor/glfw/src/**.c", -- is this needed?
  }

  filter "configurations:debug"
    symbols "On"
  
  filter "configurations:release"
    optimize "On"
