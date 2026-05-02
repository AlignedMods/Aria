#include "aria/internal/compiler/semantic_analyzer/semantic_analyzer.hpp"

namespace Aria::Internal {

    SemanticAnalyzer::SemanticAnalyzer(CompilationContext* ctx) {
        m_Context = ctx;

        m_BuiltInStringDestructor = Decl::Create(m_Context, {}, {}, DeclKind::BuiltinDestructor, BuiltinDestructorDecl(BuiltinKind::String));
        m_BuiltInStringCopyConstructor = Decl::Create(m_Context, {}, {}, DeclKind::BuiltinCopyConstructor, BuiltinCopyConstructorDecl(BuiltinKind::String));

        sema_impl();
    }

    void SemanticAnalyzer::sema_impl() {
        pass_imports();
        pass_decls();
        pass_code();
    }

    void SemanticAnalyzer::push_scope(bool allowBreak, bool allowContinue) {
        Scope s;

        if (m_Scopes.size() > 0) {
            if (m_Scopes.back().AllowBreakStmt) { s.AllowBreakStmt = true; }
            else { s.AllowBreakStmt = allowBreak; }

            if (m_Scopes.back().AllowContinueStmt) { s.AllowContinueStmt = true; }
            else { s.AllowContinueStmt = allowContinue; }
        } else {
            s.AllowBreakStmt = allowBreak;
            s.AllowContinueStmt = allowContinue;
        }

        m_Scopes.push_back(s);
    }

    void SemanticAnalyzer::pop_scope() {
        m_Scopes.pop_back();
    }

    void SemanticAnalyzer::replace_expr(Expr* src, Expr* newExpr) {
        bool resultDiscarded = src->ResultDiscarded;
        *src = *newExpr;
        src->ResultDiscarded = resultDiscarded;
    }

    void SemanticAnalyzer::replace_decl(Decl* src, Decl* newDecl) {
        *src = *newDecl;
    }

} // namespace Aria::Internal
