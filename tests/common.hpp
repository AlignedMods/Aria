#pragma once

#include <fmt/format.h>
#include <fmt/color.h>

#include <string>

#define REQUIRE(expr) do { \
    case_counter++; \
    fmt::print("Test case '{}' ({}:{}): ", #expr, __FILE__, __LINE__); \
    if (!(expr)) { \
        fmt::print(fg(fmt::color::pale_violet_red), "FAILED\n"); \
        fail_counter++; \
    } else { \
        fmt::print(fg(fmt::color::light_green), "PASSED\n"); \
        pass_counter++; \
    } \
} while(0)

std::string read_file(const std::string& path);

extern size_t fail_counter;
extern size_t pass_counter;
extern size_t case_counter;