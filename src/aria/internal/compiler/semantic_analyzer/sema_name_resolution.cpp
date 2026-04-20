#include "aria/internal/compiler/semantic_analyzer/semantic_analyzer.hpp"

namespace Aria::Internal {

    Decl* SemanticAnalyzer::FindSymbolInImports(CompilationUnit* unit, StringView identifier) {
        Decl* symbol = nullptr;
        symbol = FindSymbolInUnit(unit, identifier);

        for (Stmt* import : unit->Imports) {
            ARIA_ASSERT(import->Kind == StmtKind::Import, "Invalid import");
            Decl* sy = FindSymbolInModule(import->Import.ResolvedModule, identifier, false);

            if (symbol && sy) { return nullptr; }

            symbol = sy;
        }

        return symbol;
    }

    Decl* SemanticAnalyzer::FindSymbolInModule(Module* mod, StringView identifier, bool allowPrivate) {
        std::string ident = fmt::format("{}", identifier);
        if (mod->Symbols.contains(ident)) {
            if (mod->Symbols.at(ident)->Flags & DECL_FLAG_PRIVATE && !allowPrivate) { return nullptr; }
            return mod->Symbols.at(ident);
        }

        return nullptr;
    }

    Decl* SemanticAnalyzer::FindSymbolInUnit(CompilationUnit* unit, StringView identifier) {
        std::string ident = fmt::format("{}", identifier);
        if (unit->LocalSymbols.contains(ident)) { return unit->LocalSymbols.at(ident); }

        return FindSymbolInModule(m_Context->ActiveCompUnit->Parent, identifier, true);
    }

} // namespace Aria::Internal