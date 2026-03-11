#pragma once

#include "aria/internal/compiler/ast/expr.hpp"
#include "aria/internal/compiler/ast/decl.hpp"
#include "aria/internal/compiler/ast/stmt.hpp"
#include "aria/internal/compiler/semantic_analyzer/type_checker.hpp"
#include "aria/internal/compiler/compilation_context.hpp"

#include <unordered_map>

namespace Aria::Internal {

    class SemanticAnalyzer {
    private:
        struct VariableDeclaration {
            TypeInfo* Type = nullptr;
            VarDecl* SourceDecl = nullptr;
        };

        struct ParameterDeclaration {
            TypeInfo* Type = nullptr;
            ParamDecl* SourceDecl = nullptr;
        };

        struct FunctionDeclaration {
            TypeInfo* ReturnType = nullptr;
            FunctionDecl* SourceDecl = nullptr;
        };

        using Declaration = std::variant<VariableDeclaration, ParameterDeclaration, FunctionDeclaration>;

        struct Scope {
            std::unordered_map<std::string, Declaration> DeclaredSymbols;
        };

    public:
        SemanticAnalyzer(CompilationContext* ctx);

    private:
        void AnalyzeImpl();

        TypeInfo* HandleBooleanConstantExpr(Expr* expr);
        TypeInfo* HandleCharacterConstantExpr(Expr* expr);
        TypeInfo* HandleIntegerConstantExpr(Expr* expr);
        TypeInfo* HandleFloatingConstantExpr(Expr* expr);
        TypeInfo* HandleStringConstantExpr(Expr* expr);
        TypeInfo* HandleDeclRefExpr(Expr* expr);
        TypeInfo* HandleCallExpr(Expr* expr);
        TypeInfo* HandleParenExpr(Expr* expr);
        TypeInfo* HandleCastExpr(Expr* expr);
        TypeInfo* HandleUnaryOperatorExpr(Expr* expr);
        TypeInfo* HandleBinaryOperatorExpr(Expr* expr);

        TypeInfo* HandleExpr(Expr* expr);

        void HandleTranslationUnitDecl(Decl* decl);
        void HandleVarDecl(Decl* decl);
        void HandleParamDecl(Decl* decl);
        void HandleFunctionDecl(Decl* decl);

        void HandleDecl(Decl* decl);

        void HandleCompoundStmt(Stmt* stmt);
        void HandleWhileStmt(Stmt* stmt);
        void HandleDoWhileStmt(Stmt* stmt);
        void HandleForStmt(Stmt* stmt);
        void HandleIfStmt(Stmt* stmt);
        void HandleReturnStmt(Stmt* stmt);

        void HandleStmt(Stmt* stmt);

        void PushScope();
        void PopScope();

        std::unordered_map<std::string, Declaration>& GetActiveDeclMap();

    private:
        Stmt* m_RootASTNode = nullptr;

        std::unordered_map<std::string, Declaration> m_GlobalDeclaredSymbols;
        std::vector<Scope> m_Scopes;

        bool m_IsFunctionScope = false;

        TypeChecker m_TypeChecker;
        CompilationContext* m_Context = nullptr;
    };

} // namespace Aria::Internal
