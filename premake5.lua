workspace "lucerna"
    configurations { "debug", "release" }
    architecture "x86_64"

    language "C++"
    cppdialect "C++23"
    toolset "clang"

    linkoptions {"-fuse-ld=mold"}

    filter "configurations:debug"
        symbols "On"
    filter "configurations:release"
        optimize "On"

project "lucerna"
    kind "WindowedApp"

    targetdir ("build/bin/%{cfg.buildcfg}")
    objdir ("build/obj/%{cfg.buildcfg}")
    

    defines 
    {
    	"GLM_ENABLE_EXPERIMENTAL",
      "GLM_FORCE_RADIANS",
      "GLM_FORCE_DEPTH_ZERO_TO_ONE",
      "GLM_FORCE_RIGHT_HANDED",
      "GLM_FORCE_SWIZZLE",
      "GLM_FORCE_XYZW_ONLY",
    }
    
    pchheader "src/lucerna_pch.h"
    
    buildoptions {"-Wno-nullability-completeness"}

    files 
    {
        "src/**.h",
        "src/**.cpp",
    }

    includedirs 
    {
        "vendor/glfw/include",
        "vendor/KHR",
        "vendor/vulkan-headers/include",
        "vendor/VulkanMemoryAllocator/include",
        "vendor/imgui",
        "vendor/glm",
        "vendor/stb_image",
        "vendor/fastgltf/include",
        "vendor/volk",
        "vendor/tomlplusplus/include",
        "vendor/spirv-headers/include",

        "shaders/include",
        "shaders/",
    }
  
    libdirs {"build/lib/bin/%{cfg.buildcfg}"}

    links 
    {
        "glfw",
        "imgui",
        "fastgltf",
        "volk",
    }
    
    postbuildcommands {
      "premake export-compile-commands",
      "./shader_compilation.sh"
    }
  
    filter "configurations:debug"
        defines {"LA_ENABLE_LOGS", "LA_ENABLE_ASSERTS=1", "TOML_EXCEPTIONS=0", "LA_DEBUG"}

    filter "configurations:release"
        defines {"LA_ENABLE_LOGS", "LA_ENABLE_ASSERTS=1", "TOML_EXCEPTIONS=0"}

project "glfw"
    location "vendor/glfw"
    kind "StaticLib"
    targetdir ("build/lib/bin/%{cfg.buildcfg}")
    objdir("build/lib/obj/%{cfg.buildcfg}")

    includedirs
    {
        "vendor/glfw/include",
        "vendor/KHR/",
    }

    defines {
        "_GLFW_X11",
    }

    files
    {
        "vendor/glfw/src/**.c",
    }


project "imgui"
    location "vendor/imgui"
    kind "StaticLib"
    targetdir ("build/lib/bin/%{cfg.buildcfg}")
    objdir ("build/lib/obj/%{cfg.buildcfg}")

    includedirs
    {
        "vendor/imgui",
        "vendor/imgui/backends",

        "vendor/vulkan-headers/include", -- NOTE: why do i need vulkan headers if im using volk?
        "vendor/volk/",
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


project "fastgltf"
    location "vendor/fastgltf"
    kind "StaticLib"
    targetdir ("build/lib/bin/%{cfg.buildcfg}")
    objdir ("build/lib/obj/%{cfg.buildcfg}")

    includedirs
    {
        "vendor/fastgltf/include",
        "vendor/simdjson/singleheader/"
    }


    files {
      "vendor/simdjson/singleheader/**.cpp",
      "vendor/fastgltf/src/**",
    }
 
    
project "volk"
    location "vendor/volk"
    kind "StaticLib"
    targetdir ("build/lib/bin/%{cfg.buildcfg}")
    objdir ("build/lib/obj/%{cfg.buildcfg}")

    defines 
    {
        "VOLK_STATIC_DEFINES",
        "VK_USE_PLATFORM_XLIB_KHR",
    }

    includedirs
    {
        "vendor/vulkan-headers/include/",
    }
    files
    {
        "vendor/volk/*.c",
        "vendor/volk/*.h",
    }
