#include "aria/internal/compiler/compilation_context.hpp"
#include "aria/internal/compiler/lexer/lexer.hpp"
#include "aria/internal/compiler/parser/parser.hpp"
#include "aria/internal/compiler/semantic_analyzer/semantic_analyzer.hpp"
#include "aria/internal/compiler/codegen/emitter.hpp"
#include "aria/internal/compiler/codegen/linker.hpp"

namespace Aria::Internal {

    void CompilationContext::CompileFile(const std::string& source) {
        CompilationUnit* unit = new CompilationUnit(source);

        if (CompilationUnits.size() > 0) { unit->Index = CompilationUnits[CompilationUnits.size() - 1]->Index + 1; }

        CompilationUnits.push_back(unit);
        ActiveCompUnit = unit;

        Lex();
        Parse();
    }

    void CompilationContext::FinishCompilation() {
        Analyze();

        bool hasErrors = false;

        for (Module* module : Modules) {
            for (CompilationUnit* unit : module->Units) {
                if (unit->Errors.size() > 0) { hasErrors = true; break; }
            }
        }

        if (!hasErrors) {
            Emit();
            Link();
        }
    }

    void CompilationContext::Lex() { Lexer l(this); }
    void CompilationContext::Parse() { Parser p(this); }
    void CompilationContext::Analyze() { SemanticAnalyzer s(this); }
    void CompilationContext::Emit() { Emitter e(this); }
    void CompilationContext::Link() { Linker l(this); }

    Module* CompilationContext::FindOrCreateModule(const std::string& name) {
        for (Module* mod : Modules) {
            if (mod->Name == name) { return mod; }
        }

        Module* mod = new Module();
        mod->Name = name;
        Modules.push_back(mod);
        return mod;
    }

} // namespace Aria::Internal
