#pragma once

#include "aria/internal/vm/op_codes.hpp"
#include "aria/internal/compiler/compilation_context.hpp"

namespace Aria::Internal {
    
    class Linker {
    public:
        Linker(CompilationContext* ctx);

    private:
        void LinkImpl();

    private:
        CompilationContext* m_Context = nullptr;
    };

} // namespace Aria::Internal