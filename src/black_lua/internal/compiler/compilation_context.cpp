#include "black_lua/internal/compiler/compilation_context.hpp"
#include "black_lua/internal/compiler/lexer/lexer.hpp"

namespace BlackLua::Internal {

    void CompilationContext::Compile() {
        Lex();
        Parse();
        Analyze();
        Emit();
    }

    void CompilationContext::Lex() {
        Lexer l(this);
    }

} // namespace BlackLua::Internal
