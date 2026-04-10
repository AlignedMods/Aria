OutputDir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

workspace "Aria"
    configurations { "Debug", "Release" }
    platforms { "x86", "x86_64" }

    project "AriaLib"
        language "C++"
        cppdialect "C++20"
        kind "StaticLib"

        targetdir("build/bin/" .. OutputDir)
        objdir("build/obj/" .. OutputDir)

        files {"src/aria/**.cpp", "src/aria/**.hpp"}

        includedirs { "src/", "src/vendor/fmt/include/" }

        filter { "action:vs*" }
            buildoptions { "/utf-8" }

        filter {}

        filter "configurations:Debug"
            symbols "On"

        filter "configurations:Release"
            optimize "On"

    project "AriaTest"
        language "C++"
        cppdialect "C++20"
        kind "ConsoleApp"

        targetdir("build/bin/" .. OutputDir)
        objdir("build/obj/" .. OutputDir)

        files { "tests/**.cpp", "tests/**.hpp" }

        includedirs { "tests/", "src/", "src/vendor/catch2/", "src/vendor/fmt/include/" }

        links { "AriaLib", "fmt" }

        filter { "action:vs*" }
            buildoptions { "/utf-8" }

        filter {}

        filter "configurations:Debug"
            symbols "On"

        filter "configurations:Release"
            optimize "On"

    project "Aria"
        language "C++"
        cppdialect "C++20"
        kind "ConsoleApp"

        targetdir("build/bin/" .. OutputDir)
        objdir("build/obj/" .. OutputDir)

        files { "frontend/aria.cpp" }

        includedirs { "src/", "src/vendor/fmt/include/" }

        links { "AriaLib", "fmt" }

        filter { "action:vs*" }
            buildoptions { "/utf-8" }

        filter {}

        filter "configurations:Debug"
            symbols "On"

        filter "configurations:Release"
            optimize "On"

    project "fmt"
        language "C++"
        cppdialect "C++20"
        kind "StaticLib"

        targetdir("build/bin/" .. OutputDir)
        objdir("build/obj/" .. OutputDir)

        files { "src/vendor/fmt/src/format.cc", "src/vendor/fmt/src/os.cc" }

        includedirs { "src/vendor/fmt/include/" }

        filter { "action:vs*" }
            buildoptions { "/utf-8" }

        filter {}

        filter "configurations:Debug"
            symbols "On"

        filter "configurations:Release"
            optimize "On"
