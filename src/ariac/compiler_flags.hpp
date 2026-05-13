#pragma once

#include <string>

namespace Aria::Internal {
    
    struct CompilerFlags {
        bool dump_ast = false;
        bool dump_bytecode = false;
        bool no_stdlib = false;

        std::string ast_dump_output;
        std::string bytecode_dump_output;
    };

}