#include "ariac/semantic_analyzer/semantic_analyzer.hpp"

#include <fstream>

namespace ariac {

    SemanticAnalyzer::SemanticAnalyzer() {
        sema_impl();
    }

    void SemanticAnalyzer::sema_impl() {
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

        m_break_target = { b, m_functions.back().scopes.size() - 1 };
        m_continue_target = { c, m_functions.back().scopes.size() - 1 };

        return { prevb, prevc };
    }

    void SemanticAnalyzer::restore_break_targets(std::pair<JumpTarget, JumpTarget> prev) {
        m_break_target = prev.first;
        m_continue_target = prev.second;
    }

    SemanticAnalyzer::JumpTarget SemanticAnalyzer::set_nextcase_target(Stmt* s) {
        JumpTarget prev = m_nextcase_target;
        m_nextcase_target = { s, m_functions.back().scopes.size() - 1 };
        return prev;
    }

    void SemanticAnalyzer::restore_nextcase_target(JumpTarget prev) {
        m_nextcase_target = prev;
    }

    TypeInfo* SemanticAnalyzer::get_typeinfo(Expr* e) {
        if (!e->type->is_typeid()) { return nullptr; }
        if (!is_const_expr(e)) { return nullptr; }

        Expr* c = eval_const_expr(e);
        if (c->const_.kind == ConstExprKind::Error) { return nullptr; }

        ARIA_ASSERT(c->const_.kind == ConstExprKind::Typeid, "Invalid typeid");
        return c->const_.type;
    }

    void SemanticAnalyzer::replace_expr(Expr* src, Expr* new_expr) {
        bool resultDiscarded = src->result_discarded;
        *src = *new_expr;
        src->result_discarded = resultDiscarded;
    }

    void SemanticAnalyzer::replace_decl(Decl* src, Decl* new_decl) {
        *src = *new_decl;
    }

    void SemanticAnalyzer::replace_stmt(Stmt* src, Stmt* new_stmt) {
        *src = *new_stmt;
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
        // If we are capturing errors, don't emit an actual error
        if (!m_error_captures.empty() && kind == CompilerDiagKind::Error) {
            m_error_captures.back().has_error = true;
            return;
        }

        context.report_compiler_diagnostic_with_notes(loc, error, notes, kind);

        for (auto it = m_generic_instantations.rbegin(); it != m_generic_instantations.rend(); it++) {
            context.report_compiler_diagnostic(it->loc, "Instantiated from here", CompilerDiagKind::Note);
        }
    }

} // namespace ariac
