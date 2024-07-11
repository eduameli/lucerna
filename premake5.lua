workspace "aurora"
	configurations {"debug", "release"}

project "aurora"
	kind "WindowedApp"
	language "C++"
	cppdialect "C++17"
	
	targetdir "build/bin/%{cfg.buildcfg}"
	objdir "build/obj/%{cfg.buildcfg}"
	
	files 
	{
		"src/**.h",
		"src/**.cpp",
	}
	
	includedirs 
	{
		"vendor/spdlog/include",
	}
	
	pchheader "src/aurora_pch.h"
	

	filter "configurations:debug"
		defines 
		{
			"DEBUG",
		}
		symbols "On"
	
	filter "configurations:release"
		defines
		{
			"NDEBUG"
		}
		optimize "On"
