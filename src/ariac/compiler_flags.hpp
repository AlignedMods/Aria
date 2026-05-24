#pragma once

#include <string>

namespace Aria::Internal {
    
    struct CompilerFlags {
        bool dump_ast = false;
        bool dump_ir = false;
        bool no_stdlib = false;

        std::string ast_dump_output;
    };

}