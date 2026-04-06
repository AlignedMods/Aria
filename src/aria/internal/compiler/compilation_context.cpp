#include "aria/internal/compiler/compilation_context.hpp"
#include "aria/internal/compiler/lexer/lexer.hpp"
#include "aria/internal/compiler/parser/parser.hpp"
#include "aria/internal/compiler/semantic_analyzer/semantic_analyzer.hpp"
#include "aria/internal/compiler/codegen/emitter.hpp"
#include "aria/internal/compiler/codegen/linker.hpp"

namespace Aria::Internal {

    void CompilationContext::Compile(const std::string& source) {
        m_CompilationUnits.emplace_back(source);
        m_ActiveCompUnit = &m_CompilationUnits.back();

        if (m_CompilationUnits.size() > 1) { m_ActiveCompUnit->Index = m_CompilationUnits[m_CompilationUnits.size() - 2].Index + 1; }

        Lex();
        Parse();
        Analyze();

        if (m_ActiveCompUnit->Errors.empty()) { Emit(); }
        if (m_ActiveCompUnit->Errors.empty()) { Link(); }
    }

    void CompilationContext::Lex() { Lexer l(this); }
    void CompilationContext::Parse() { Parser p(this); }
    void CompilationContext::Analyze() { SemanticAnalyzer s(this); }
    void CompilationContext::Emit() { Emitter e(this); }
    void CompilationContext::Link() { Linker l(this); }

} // namespace Aria::Internal
