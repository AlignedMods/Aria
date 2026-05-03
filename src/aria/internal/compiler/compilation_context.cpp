#include "aria/internal/compiler/compilation_context.hpp"
#include "aria/internal/compiler/lexer/lexer.hpp"
#include "aria/internal/compiler/parser/parser.hpp"
#include "aria/internal/compiler/semantic_analyzer/semantic_analyzer.hpp"
#include "aria/internal/compiler/codegen/emitter.hpp"

namespace Aria::Internal {

    void CompilationContext::compile_file(const std::string& source) {
        CompilationUnit* unit = new CompilationUnit(source);

        if (compilation_units.size() > 0) { unit->index = compilation_units[compilation_units.size() - 1]->index + 1; }

        compilation_units.push_back(unit);
        active_comp_unit = unit;

        lex();
        parse();
    }

    void CompilationContext::finish_compilation() {
        analyze();
        if (!has_errors) { emit(); }
    }

    void CompilationContext::lex() { Lexer l(this); }
    void CompilationContext::parse() { Parser p(this); }
    void CompilationContext::analyze() { SemanticAnalyzer s(this); }
    void CompilationContext::emit() { Emitter e(this); }

    Module* CompilationContext::find_or_create_module(std::string_view name) {
        for (Module* mod : modules) {
            if (mod->name == name) {
                return mod;
            }
        }

        Module* mod = new Module();
        mod->name = name;
        modules.push_back(mod);
        return mod;
    }

} // namespace Aria::Internal
