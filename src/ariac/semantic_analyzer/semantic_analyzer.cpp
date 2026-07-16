#include "ariac/semantic_analyzer/semantic_analyzer.hpp"

#include <fstream>

namespace ariac {

    SemanticAnalyzer::SemanticAnalyzer() {
        sema_impl();
    }

    void SemanticAnalyzer::sema_impl() {
        pass_module_heirarchy();
        pass_imports();
        pass_decls();
        pass_code();
        pass_generics();
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

    std::string_view SemanticAnalyzer::get_parent_path(std::string_view path) {
        size_t i = path.rfind("::");
        if (i == std::string_view::npos) { return {}; }

        return path.substr(0, i);
    }

    Module* SemanticAnalyzer::module_get_child(Module* mod, std::string_view child) {
        if (mod->children.empty()) { return nullptr; }

        for (Module* c : mod->children) {
            if (compare_module_names(child, c->name)) {
                return c;
            }

            if (Module* cc = module_get_child(c, child)) {
                return cc;
            }
        }

        return nullptr;
    }

} // namespace ariac
