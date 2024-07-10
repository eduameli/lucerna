workspace "aurora"
	configurations {"debug", "release"}

project "aurora"
	kind "WindowedApp"
	language "C++"
	targetdir "build/bin/%{cfg.buildcfg}"
	objdir "build/obj/%{cfg.buildcfg}"
	
	files 
	{
		"**.h",
		"**.cpp",
	}
	
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
	
	
