#include "aria/internal/compiler/compilation_context.hpp"
#include "aria/internal/compiler/lexer/lexer.hpp"
#include "aria/internal/compiler/parser/parser.hpp"
#include "aria/internal/compiler/semantic_analyzer/semantic_analyzer.hpp"
#include "aria/internal/compiler/codegen/emitter.hpp"

namespace Aria::Internal {

    void CompilationContext::compile_file(const std::string& source) {
        CompilationUnit* unit = new CompilationUnit(source);

        if (CompilationUnits.size() > 0) { unit->Index = CompilationUnits[CompilationUnits.size() - 1]->Index + 1; }

        CompilationUnits.push_back(unit);
        ActiveCompUnit = unit;

        lex();
        parse();
    }

    void CompilationContext::finish_compilation() {
        analyze();
        if (!HasErrors) { emit(); }
    }

    void CompilationContext::lex() { Lexer l(this); }
    void CompilationContext::parse() { Parser p(this); }
    void CompilationContext::analyze() { SemanticAnalyzer s(this); }
    void CompilationContext::emit() { Emitter e(this); }

    Module* CompilationContext::find_or_create_module(const std::string& name) {
        for (Module* mod : Modules) {
            if (mod->Name == name) {
                return mod;
            }
        }

        Module* mod = new Module();
        mod->Name = name;
        Modules.push_back(mod);
        return mod;
    }

} // namespace Aria::Internal
