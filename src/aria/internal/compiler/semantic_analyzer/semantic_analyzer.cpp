#include "aria/internal/compiler/semantic_analyzer/semantic_analyzer.hpp"

namespace Aria::Internal {

    SemanticAnalyzer::SemanticAnalyzer(CompilationContext* ctx) {
        m_Context = ctx;

        m_BuiltInStringDestructor = Decl::Create(m_Context, {}, {}, DeclKind::BuiltinDestructor, 0, BuiltinDestructorDecl(BuiltinKind::String));
        m_BuiltInStringCopyConstructor = Decl::Create(m_Context, {}, {}, DeclKind::BuiltinCopyConstructor, 0, BuiltinCopyConstructorDecl(BuiltinKind::String));

        SemaImpl();
    }

    void SemanticAnalyzer::SemaImpl() {
        PassImports();
        PassDecls();
        PassCode();
    }

    void SemanticAnalyzer::PushScope(bool allowBreak, bool allowContinue) {
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

    void SemanticAnalyzer::PopScope() {
        m_Scopes.pop_back();
    }

    void SemanticAnalyzer::ReplaceExpr(Expr* src, Expr* newExpr) {
        *src = *newExpr;
    }

    void SemanticAnalyzer::ReplaceDecl(Decl* src, Decl* newDecl) {
        *src = *newDecl;
    }

} // namespace Aria::Internal
