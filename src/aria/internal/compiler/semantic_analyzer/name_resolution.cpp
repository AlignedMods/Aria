#include "aria/internal/compiler/semantic_analyzer/semantic_analyzer.hpp"

namespace Aria::Internal {

    Decl* SemanticAnalyzer::FindSymbolInModule(Module* mod, StringView identifier) {
        std::string ident = fmt::format("{}", identifier);
        if (mod->Symbols.contains(ident)) { return mod->Symbols.at(ident); }

        return nullptr;
    }

    Decl* SemanticAnalyzer::FindSymbolInUnit(CompilationUnit* unit, StringView identifier) {
        std::string ident = fmt::format("{}", identifier);
        if (m_SearchModule) { return FindSymbolInModule(m_SearchModule, identifier); }
        if (unit->LocalSymbols.contains(ident)) { return unit->LocalSymbols.at(ident); }

        return FindSymbolInModule(m_Context->ActiveCompUnit->Parent, identifier);
    }

} // namespace Aria::Internal