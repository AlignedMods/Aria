#include "aria/internal/compiler/semantic_analyzer/semantic_analyzer.hpp"

namespace Aria::Internal {

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