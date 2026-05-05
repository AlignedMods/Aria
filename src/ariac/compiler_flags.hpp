#pragma once

namespace Aria::Internal {
    
    struct CompilerFlags {
        bool compile_only = false;
        bool dump_ast = false;
        bool dump_bytecode = false;
    };

}