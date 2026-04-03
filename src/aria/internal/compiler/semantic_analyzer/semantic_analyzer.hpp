#pragma once

#include "aria/internal/compiler/ast/expr.hpp"
#include "aria/internal/compiler/ast/decl.hpp"
#include "aria/internal/compiler/ast/stmt.hpp"
#include "aria/internal/compiler/semantic_analyzer/type_checker.hpp"
#include "aria/internal/compiler/compilation_context.hpp"

#include <unordered_map>

namespace Aria::Internal {

    class SemanticAnalyzer {
    public:
        SemanticAnalyzer(CompilationContext* ctx);

    private:
        void AnalyzeImpl();

        void HandleBooleanConstantExpr(Expr* expr);
        void HandleCharacterConstantExpr(Expr* expr);
        void HandleIntegerConstantExpr(Expr* expr);
        void HandleFloatingConstantExpr(Expr* expr);
        void HandleStringConstantExpr(Expr* expr);
        void HandleDeclRefExpr(Expr* expr);
        void HandleMemberExpr(Expr* expr);
        void HandleCallExpr(Expr* expr);
        void HandleMethodCallExpr(Expr* expr);
        void HandleParenExpr(Expr* expr);
        void HandleCastExpr(Expr* expr);
        void HandleImplicitCastExpr(Expr* expr);
        void HandleUnaryOperatorExpr(Expr* expr);
        void HandleBinaryOperatorExpr(Expr* expr);
        void HandleCompoundAssignExpr(Expr* expr);

        void HandleExpr(Expr* expr);

        void HandleTranslationUnitDecl(Decl* decl);
        void HandleVarDecl(Decl* decl);
        void HandleParamDecl(Decl* decl);
        void HandleFunctionDecl(Decl* decl);
        void HandleStructDecl(Decl* decl);

        void HandleDecl(Decl* decl);

        void HandleBlockStmt(Stmt* stmt);
        void HandleWhileStmt(Stmt* stmt);
        void HandleDoWhileStmt(Stmt* stmt);
        void HandleForStmt(Stmt* stmt);
        void HandleIfStmt(Stmt* stmt);
        void HandleBreakStmt(Stmt* stmt);
        void HandleContinueStmt(Stmt* stmt);
        void HandleReturnStmt(Stmt* stmt);

        void HandleStmt(Stmt* stmt);

    private:
        Stmt* m_RootASTNode = nullptr;

        bool m_AllowBreakStmt = false;
        bool m_AllowContinueStmt = false;

        TypeChecker m_TypeChecker;
        CompilationContext* m_Context = nullptr;
    };

} // namespace Aria::Internal
