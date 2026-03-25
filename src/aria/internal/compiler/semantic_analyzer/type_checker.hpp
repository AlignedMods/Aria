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

    public:
        TypeChecker(CompilationContext* ctx);

    private:
        void CheckImpl();

        Expr* HandleBooleanConstantExpr(Expr* expr);
        Expr* HandleCharacterConstantExpr(Expr* expr);
        Expr* HandleIntegerConstantExpr(Expr* expr);
        Expr* HandleFloatingConstantExpr(Expr* expr);
        Expr* HandleStringConstantExpr(Expr* expr);
        Expr* HandleDeclRefExpr(Expr* expr);
        Expr* HandleMemberExpr(Expr* expr);
        Expr* HandleCallExpr(Expr* expr);
        Expr* HandleMethodCallExpr(Expr* expr);
        Expr* HandleParenExpr(Expr* expr);
        Expr* HandleCastExpr(Expr* expr);
        Expr* HandleUnaryOperatorExpr(Expr* expr);
        Expr* HandleBinaryOperatorExpr(Expr* expr);
        Expr* HandleCompoundAssignExpr(Expr* expr);

        Expr* HandleExpr(Expr* expr);

        void HandleTranslationUnitDecl(Decl* decl);
        void HandleVarDecl(Decl* decl);
        void HandleParamDecl(Decl* decl);
        void HandleFunctionDecl(Decl* decl);
        void HandleStructDecl(Decl* decl);

        void HandleDecl(Decl* decl);

        void HandleCompoundStmt(Stmt* stmt);
        void HandleWhileStmt(Stmt* stmt);
        void HandleDoWhileStmt(Stmt* stmt);
        void HandleForStmt(Stmt* stmt);
        void HandleIfStmt(Stmt* stmt);
        void HandleReturnStmt(Stmt* stmt);

        void HandleStmt(Stmt* stmt);

        Expr* HandleInitializer(Expr* initializer, TypeInfo* type);

        TypeInfo* GetTypeInfoFromString(StringView str);

        // type1 is the destination type and type2 is the source type
        ConversionCost GetConversionCost(TypeInfo* dst, TypeInfo* src, ExprValueKind srcKind);
        Expr* InsertImplicitCast(TypeInfo* dstType, TypeInfo* srcType, Expr* srcExpr, CastKind castKind); // Returns the new ImplicitCastExpr

        bool TypeIsEqual(TypeInfo* lhs, TypeInfo* rhs);
        size_t TypeGetSize(TypeInfo* t); // NOTE: works only on trivial types

    private:
        Stmt* m_RootASTNode = nullptr;

        std::vector<std::unordered_map<std::string, Declaration>> m_Declarations;
        TypeInfo* m_ActiveReturnType = nullptr;
        TypeInfo* m_ActiveStruct = nullptr;

        std::unordered_map<std::string, TypeInfo*> m_DeclaredTypes;

        CompilationContext* m_Context = nullptr;
        friend class SemanticAnalyzer;
    };

} // namespace Aria::Internal
