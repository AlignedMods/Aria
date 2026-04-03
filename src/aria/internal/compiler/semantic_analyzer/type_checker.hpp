#pragma once

#include "aria/internal/compiler/types/type_info.hpp"
#include "aria/internal/compiler/core/string_builder.hpp"
#include "aria/internal/compiler/ast/expr.hpp"
#include "aria/internal/compiler/ast/decl.hpp"
#include "aria/internal/compiler/ast/stmt.hpp"

#include <unordered_map>

namespace Aria::Internal {
    
    enum class ConversionKind {
        None,
        Promotion,
        Narrowing,
        SignChange,
        LValueToRValue
    };

    struct ConversionCost {
        ConversionKind CoKind = ConversionKind::None;
        CastKind CaKind = CastKind::Invalid;

        bool CastNeeded = false;
        bool SignedMismatch = false;
        bool LValueMismatch = false;
        bool ImplicitCastPossible = false;
        bool ExplicitCastPossible = false;
    };

    // NOTE: The type checker serves as a prepass to the main semantic analysis
    // Its job is to resolve all type ambiguities and implicit casts
    class TypeChecker {
    private:
        struct Declaration {
            TypeInfo* ResolvedType = nullptr;
            Decl* SourceDeclaration = nullptr;
            DeclRefKind DeclKind = DeclRefKind::LocalVar;
        };

        struct Scope {
            std::unordered_map<std::string, Declaration> Declarations;
            bool AllowBreakStmt = false;
            bool AllowContinueStmt = false;
        };

    public:
        TypeChecker(CompilationContext* ctx);

    private:
        void CheckImpl();

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

        void HandleInitializer(Expr* initializer, TypeInfo* type, bool temporary);

        void PushScope(bool allowBreak = false, bool allowContinue = false);
        void PopScope();

        ConversionCost GetConversionCost(TypeInfo* dst, TypeInfo* src, ExprValueKind srcKind);
        void InsertImplicitCast(TypeInfo* dstType, TypeInfo* srcType, Expr* srcExpr, CastKind castKind);
        void RequireRValue(Expr* expr);
        void InsertArithmeticPromotion(Expr* lhs, Expr* rhs);

        void ReplaceExpr(Expr* src, Expr* newExpr);

        bool TypeIsEqual(TypeInfo* lhs, TypeInfo* rhs);
        size_t TypeGetSize(TypeInfo* t); // NOTE: works only on trivial types

    private:
        Stmt* m_RootASTNode = nullptr;

        // Type used for expressions when an error has occured
        TypeInfo* m_ErrorType = nullptr;

        std::vector<Scope> m_Scopes;
        TypeInfo* m_ActiveReturnType = nullptr;
        TypeInfo* m_ActiveStruct = nullptr;

        std::unordered_map<std::string, TypeInfo*> m_DeclaredTypes;

        bool m_TemporaryContext = false;

        CompilationContext* m_Context = nullptr;
        friend class SemanticAnalyzer;
    };

} // namespace Aria::Internal
