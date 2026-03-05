#include "black_lua/internal/compiler/compilation_context.hpp"
#include "black_lua/internal/compiler/lexer/lexer.hpp"
#include "black_lua/internal/compiler/parser/parser.hpp"
#include "black_lua/internal/compiler/semantic_analyzer/semantic_analyzer.hpp"
#include "black_lua/internal/compiler/codegen/emitter.hpp"

namespace BlackLua::Internal {

    void CompilationContext::Compile() {
        Lex();
        Parse();
        Analyze();
        Emit();
    }

    void CompilationContext::Lex() { Lexer l(this); }
    void CompilationContext::Parse() { Parser p(this); }
    void CompilationContext::Analyze() { SemanticAnalyzer s(this); }
    void CompilationContext::Emit() { Emitter e(this); }

} // namespace BlackLua::Internal
