#pragma once

#include <string>
#include <filesystem>

namespace ariac {
    
    struct CompilerFlags {
        bool no_codegen = false;
        bool dump_ast = false;
        bool dump_ir = false;
        bool no_stdlib = false;

        std::filesystem::path stdlib_path;
        std::string ast_dump_output;

        std::vector<std::string> libs;
        std::vector<std::string> libdirs;
    };

}