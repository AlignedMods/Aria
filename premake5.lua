OutputDir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

workspace "Aria"
    configurations { "Debug", "Release" }
    platforms { "x86", "x86_64" }

    project "arialib"
        language "C++"
        cppdialect "C++20"
        kind "StaticLib"

        targetdir("build/bin/" .. OutputDir)
        objdir("build/obj/" .. OutputDir)

        files {"src/arialib/**.cpp", "src/arialib/**.hpp"}

        includedirs { "src/", "include/", "src/vendor/fmt/include/" }

        filter { "action:vs*" }
            buildoptions { "/utf-8", "/Zc:preprocessor" }

        filter {}

        filter "configurations:Debug"
            symbols "On"

        filter "configurations:Release"
            optimize "On"

    project "aria_test"
        language "C++"
        cppdialect "C++20"
        kind "ConsoleApp"

        targetdir("build/bin/" .. OutputDir)
        objdir("build/obj/" .. OutputDir)

        files { "tests/**.cpp", "tests/**.hpp" }

        includedirs { "tests/", "src/", "src/vendor/catch2/", "src/vendor/fmt/include/" }

        links { "AriaLib", "fmt" }

        filter { "action:vs*" }
            buildoptions { "/utf-8", "/Zc:preprocessor" }

        filter {}

        filter "configurations:Debug"
            symbols "On"

        filter "configurations:Release"
            optimize "On"

    project "ariac"
        language "C++"
        cppdialect "C++20"
        kind "ConsoleApp"

        targetdir("build/bin/" .. OutputDir)
        objdir("build/obj/" .. OutputDir)

        files { "src/ariac/**.cpp", "src/ariac/**.hpp" }

        includedirs { "src/", "src/vendor/fmt/include/" }

        links { "fmt" }

        filter { "action:vs*" }
            buildoptions { "/utf-8", "/Zc:preprocessor" }

        filter {}

        filter "configurations:Debug"
            symbols "On"

        filter "configurations:Release"
            optimize "On"

    project "aria"
        language "C++"
        cppdialect "C++20"
        kind "ConsoleApp"

        targetdir("build/bin/" .. OutputDir)
        objdir("build/obj/" .. OutputDir)

        files { "src/aria/**.cpp", "src/aria/**.hpp" }

        includedirs { "src/", "include/", "src/vendor/fmt/include/" }

        links { "fmt", "arialib" }

        filter { "action:vs*" }
            buildoptions { "/utf-8", "/Zc:preprocessor" }

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
