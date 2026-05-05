#pragma once

#include "fmt/printf.h"
#include "fmt/color.h"

#ifdef _WIN32
    #define ARIA_DEBUGBREAK() __debugbreak()
#elif __linux__
    #include <signal.h>
    #define ARIA_DEBUGBREAK() raise(SIGTRAP)
#endif

#ifndef ARIA_ASSERT
    #define ARIA_ASSERT(condition, error) do { if (!(condition)) { fmt::print(stderr, "{}:{}, Assertion failed with message:\n{}\n", __FILE__, __LINE__, error); ARIA_DEBUGBREAK(); abort(); } } while(0)
#endif

#ifndef ARIA_TODO
    #define ARIA_TODO(message) do { fmt::print(stderr, "{}:{}, Unimplemented feature caught (TODO): '{}'\n", __FILE__, __LINE__, message); ARIA_DEBUGBREAK(); abort(); } while(0)
#endif

#ifndef ARIA_UNREACHABLE
    #define ARIA_UNREACHABLE() ARIA_ASSERT(false, "Unreachable!")
#endif
