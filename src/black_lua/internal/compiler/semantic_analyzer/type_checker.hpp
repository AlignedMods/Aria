#pragma once

#include "black_lua/internal/compiler/types/type_info.hpp"
#include "black_lua/internal/compiler/core/string_builder.hpp"
#include "black_lua/internal/compiler/ast/expr.hpp"
#include "black_lua/internal/compiler/ast/decl.hpp"
#include "black_lua/internal/compiler/ast/stmt.hpp"

#include <unordered_map>

namespace BlackLua::Internal {
    
    enum class ConversionType {
        None,
        Promotion,
        Narrowing,
        SignChange,
        LValueToRValue
    };

    struct ConversionCost {
        ConversionType ConversionType = ConversionType::None;
        CastType CastType = CastType::Integral;

        bool CastNeeded = false;
        bool SignedMismatch = false;
        bool LValueMismatch = false;
        bool ImplicitCastPossible = false;
        bool ExplicitCastPossible = false;
    };

    // NOTE: The type checker serves as a prepass to the main semantic analysis
    // Its job is to resolve all type ambiguities and implicit casts
    class TypeChecker {
    public:
        TypeChecker(CompilationContext* ctx);

    private:
        void CheckImpl();

        TypeInfo* HandleBooleanConstantExpr(Expr* expr);
        TypeInfo* HandleCharacterConstantExpr(Expr* expr);
        TypeInfo* HandleIntegerConstantExpr(Expr* expr);
        TypeInfo* HandleFloatingConstantExpr(Expr* expr);
        TypeInfo* HandleStringConstantExpr(Expr* expr);
        TypeInfo* HandleVarRefExpr(Expr* expr);
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

        TypeInfo* GetTypeInfoFromString(StringView str);

        // type1 is the destination type and type2 is the source type
        ConversionCost GetConversionCost(TypeInfo* dst, TypeInfo* src, bool srcLValue);
        Expr* InsertImplicitCast(TypeInfo* dstType, TypeInfo* srcType, Expr* srcExpr, CastType castType); // Returns the new ImplicitCastExpr

    private:
        Stmt* m_RootASTNode = nullptr;

        std::vector<std::unordered_map<std::string, TypeInfo*>> m_Declarations;
        TypeInfo* m_ActiveReturnType = nullptr;

        CompilationContext* m_Context = nullptr;
        friend class SemanticAnalyzer;
    };

} // namespace BlackLua::Internal
