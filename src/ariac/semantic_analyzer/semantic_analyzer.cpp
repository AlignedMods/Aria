#include "ariac/semantic_analyzer/semantic_analyzer.hpp"

#include <fstream>

namespace ariac {

    SemanticAnalyzer::SemanticAnalyzer(CompilationContext* ctx) {
        m_context = ctx;

        sema_impl();
    }

    void SemanticAnalyzer::sema_impl() {
        pass_imports();
        pass_decls();
        pass_code();
    }

    void SemanticAnalyzer::push_scope(bool allow_break, bool allow_continue) {
        Scope s;

        if (m_scopes.size() > 0) {
            if (m_scopes.back().allow_break_stmt) { s.allow_break_stmt = true; }
            else { s.allow_break_stmt = allow_break; }

            if (m_scopes.back().allow_continue_stmt) { s.allow_continue_stmt = true; }
            else { s.allow_continue_stmt = allow_continue; }
        } else {
            s.allow_break_stmt = allow_break;
            s.allow_continue_stmt = allow_continue;
        }

        m_scopes.push_back(s);
    }

    void SemanticAnalyzer::pop_scope() {
        m_scopes.pop_back();
    }

    void SemanticAnalyzer::replace_expr(Expr* src, Expr* new_expr) {
        bool resultDiscarded = src->result_discarded;
        *src = *new_expr;
        src->result_discarded = resultDiscarded;
    }

    void SemanticAnalyzer::replace_decl(Decl* src, Decl* new_decl) {
        *src = *new_decl;
    }

    bool SemanticAnalyzer::compare_module_names(std::string_view specifier, std::string_view module_name) {
        if (specifier.length() > module_name.length()) { return false; }
        if (specifier == module_name) { return true; }
        if (module_name.length() != specifier.length() && module_name[module_name.length() - specifier.length() - 1] != ':') { return false; }

        return specifier == module_name.substr(module_name.length() - specifier.length());
    }

} // namespace ariac
