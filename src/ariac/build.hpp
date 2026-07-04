#pragma once

#include <string>
#include <filesystem>

namespace ariac {
    
    struct BuildOptions {
        std::filesystem::path output_path;
        std::filesystem::path stdlib_path;
        std::vector<std::string> files;
        std::vector<std::string> args;
        std::vector<std::string> libs;
        std::vector<std::string> libdirs;

        bool run_after_compile = false;
        bool no_codegen = false;
        bool no_stdlib = false;
        bool emit_ast = false;
        bool emit_llvm = false;
    };

}