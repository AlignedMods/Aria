#pragma once

#pragma warning(push, 0) // Disable warnings from LLVM headers
#include "llvm/Target/TargetMachine.h"
#pragma warning(pop)

#include <string>
#include <filesystem>

namespace ariac {
    
    struct BuildOptions {
        llvm::Triple triple;

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