#include "aria/internal/compiler/compilation_context.hpp"
#include "aria/internal/compiler/lexer/lexer.hpp"
#include "aria/internal/compiler/parser/parser.hpp"
#include "aria/internal/compiler/semantic_analyzer/semantic_analyzer.hpp"
#include "aria/internal/compiler/codegen/emitter.hpp"

namespace Aria::Internal {

    void CompilationContext::Compile() {
        Lex();
        Parse();
        Analyze();

        if (m_CompilerErrors.empty()) { Emit(); }
    }

    void CompilationContext::Lex() { Lexer l(this); }
    void CompilationContext::Parse() { Parser p(this); }
    void CompilationContext::Analyze() { SemanticAnalyzer s(this); }
    void CompilationContext::Emit() { Emitter e(this); }

} // namespace Aria::Internal
