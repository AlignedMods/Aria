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
    }

    void SemanticAnalyzer::push_scope() {
        m_functions.back().scopes.emplace_back();
    }

    void SemanticAnalyzer::pop_scope() {
        m_functions.back().scopes.pop_back();
    }

    std::pair<SemanticAnalyzer::JumpTarget, SemanticAnalyzer::JumpTarget> SemanticAnalyzer::set_break_targets(Stmt* b, Stmt* c) {
        JumpTarget prevb = m_break_target;
        JumpTarget prevc = m_continue_target;

        m_break_target = { b, m_functions.back().scopes.rbegin() };
        m_continue_target = { c, m_functions.back().scopes.rbegin() };

        return { prevb, prevc };
    }

    void SemanticAnalyzer::restore_break_targets(std::pair<JumpTarget, JumpTarget> prev) {
        m_break_target = prev.first;
        m_continue_target = prev.second;
    }

    void SemanticAnalyzer::replace_expr(Expr* src, Expr* new_expr) {
        bool resultDiscarded = src->result_discarded;
        *src = *new_expr;
        src->result_discarded = resultDiscarded;
    }

    void SemanticAnalyzer::replace_decl(Decl* src, Decl* new_decl) {
        *src = *new_decl;
    }

    std::string_view SemanticAnalyzer::get_parent_path(std::string_view path) {
        size_t i = path.rfind("::");
        if (i == std::string_view::npos) { return {}; }

        return path.substr(0, i);
    }

    std::string_view SemanticAnalyzer::get_bottom_path(std::string_view path) {
        size_t i = path.rfind("::");
        if (i == std::string_view::npos) { return path; }

        return path.substr(i + 2);
    }

    void SemanticAnalyzer::report_error(SourceLoc loc, const std::string& error) {
        report_diag(loc, error, CompilerDiagKind::Error);
    }

    void SemanticAnalyzer::report_warning(SourceLoc loc, const std::string& error) {
        report_diag(loc, error, CompilerDiagKind::Warning);
    }

    void SemanticAnalyzer::report_note(SourceLoc loc, const std::string& error) {
        report_diag(loc, error, CompilerDiagKind::Note);
    }

    void SemanticAnalyzer::report_diag(SourceLoc loc, const std::string& error, CompilerDiagKind kind) {
        report_diag_with_notes(loc, error, {}, kind);
    }

    void SemanticAnalyzer::report_diag_with_notes(SourceLoc loc, const std::string& error, std::initializer_list<std::string> notes, CompilerDiagKind kind) {
        context.report_compiler_diagnostic_with_notes(loc, error, notes, kind);

        for (auto it = m_generic_instantations.rbegin(); it != m_generic_instantations.rend(); it++) {
            context.report_compiler_diagnostic(it->loc, "Instantiated from here", CompilerDiagKind::Note);
        }
    }

} // namespace ariac
