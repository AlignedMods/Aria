#include "fmt/format.h"
#include "fmt/color.h"

#include <fstream>
#include <sstream>

size_t fail_counter = 0;
size_t pass_counter = 0;
size_t case_counter = 0;

void test_ast_basic(std::string_view ariac_path);
void test_ast_var(std::string_view ariac_path);
void test_ast_binary_expr(std::string_view ariac_path);
void test_ast_casts(std::string_view ariac_path);
void test_ast_binary_promotion(std::string_view ariac_path);
void test_ast_functions(std::string_view ariac_path);
void test_ast_overloaded_functions(std::string_view ariac_path);
void test_ast_control_flow(std::string_view ariac_path);

std::string read_file(const std::string& path) {
    std::ifstream f(path);

    if (!f) {
        fmt::print(stderr, "Failed to open file '{}' for reading\n", path);
        return std::string{};
    }

    std::stringstream ss;
    ss << f.rdbuf();

    std::string s = ss.str();
    while (!s.empty() && std::isspace(s.back())) { // Trim whitespace from the end
        s.pop_back();
    }

    return s;
}

void print_help(const char* prog_name) {
    fmt::println("{}: help: ", prog_name);
    fmt::println("  {} <path_to_compiler>", prog_name);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_help(argv[0]);
        return 1;
    }

    std::string_view path = argv[1];

    // Call tests
    test_ast_basic(path);
    test_ast_var(path);
    test_ast_binary_expr(path);
    test_ast_casts(path);
    test_ast_binary_promotion(path);
    test_ast_functions(path);
    test_ast_overloaded_functions(path);
    test_ast_control_flow(path);

    fmt::println("\nTest results:");
    fmt::print(fg(fmt::color::pale_violet_red), "Failed: {}\n", fail_counter);
    fmt::print(fg(fmt::color::light_green), "Passed: {}\n", pass_counter);
    fmt::print(fg(fmt::color::light_gray), "Cases: {}\n", case_counter);

    if (fail_counter == 0) {
        fmt::print(fg(fmt::color::light_green), "\nAll Tests Passed\n");
    } else {
        fmt::print(fg(fmt::color::pale_violet_red), "\n{} Test(s) Failed\n", fail_counter);
    }
}