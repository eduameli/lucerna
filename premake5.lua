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
    }

    links 
    {
        "spdlog",
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
        "vendor/spdlog/include/spdlog/**.h",
        "vendor/spdlog/src/**.cpp"
    }

    filter "configurations:debug"
        symbols "On"

    filter "configurations:release"
        optimize "On"

