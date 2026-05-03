#include "aria/internal/compiler/semantic_analyzer/semantic_analyzer.hpp"

namespace Aria::Internal {

    SemanticAnalyzer::SemanticAnalyzer(CompilationContext* ctx) {
        m_context = ctx;

        m_builtin_string_destructor = Decl::Create(m_context, {}, {}, DeclKind::BuiltinDestructor, BuiltinDestructorDecl(BuiltinKind::String));
        m_builtin_string_copy_constructor = Decl::Create(m_context, {}, {}, DeclKind::BuiltinCopyConstructor, BuiltinCopyConstructorDecl(BuiltinKind::String));

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

} // namespace Aria::Internal
