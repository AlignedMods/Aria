#pragma once

#include "aria/internal/compiler/ast/expr.hpp"
#include "aria/internal/compiler/ast/decl.hpp"
#include "aria/internal/compiler/ast/stmt.hpp"
#include "aria/internal/compiler/compilation_context.hpp"

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

    class SemanticAnalyzer {
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
        SemanticAnalyzer(CompilationContext* ctx);

    private:
        void SemaImpl();

        // Passes
        void PassImports();
        void PassDecls();
        void PassCode();

        void AddUnitToModule(Module* module, CompilationUnit* unit);
        void ResolveModuleImports(Module* module);
        void ResolveUnitImports(Module* module, CompilationUnit* unit);

        void ResolveModuleDecls(Module* module);
        void ResolveUnitDecls(Module* module, CompilationUnit* unit);

        void ResolveModuleCode(Module* module);
        void ResolveUnitCode(Module* module, CompilationUnit* unit);

        Decl* FindSymbolInModule(Module* mod, StringView identifier);
        Decl* FindSymbolInUnit(CompilationUnit* unit, StringView identifier);

        void ResolveBooleanConstantExpr(Expr* expr);
        void ResolveCharacterConstantExpr(Expr* expr);
        void ResolveIntegerConstantExpr(Expr* expr);
        void ResolveFloatingConstantExpr(Expr* expr);
        void ResolveStringConstantExpr(Expr* expr);
        void ResolveDeclRefExpr(Expr* expr);
        void ResolveMemberExpr(Expr* expr);
        void ResolveCallExpr(Expr* expr);
        void ResolveMethodCallExpr(Expr* expr);
        void ResolveParenExpr(Expr* expr);
        void ResolveCastExpr(Expr* expr);
        void ResolveUnaryOperatorExpr(Expr* expr);
        void ResolveBinaryOperatorExpr(Expr* expr);
        void ResolveCompoundAssignExpr(Expr* expr);

        void ResolveExpr(Expr* expr);

        void ResolveTranslationUnitDecl(Decl* decl);
        void ResolveVarDecl(Decl* decl);
        void ResolveParamDecl(Decl* decl);
        void ResolveFunctionDecl(Decl* decl);
        void ResolveStructDecl(Decl* decl);

        void ResolveDecl(Decl* decl);

        void ResolveImportStmt(Stmt* stmt);
        void ResolveBlockStmt(Stmt* stmt);
        void ResolveWhileStmt(Stmt* stmt);
        void ResolveDoWhileStmt(Stmt* stmt);
        void ResolveForStmt(Stmt* stmt);
        void ResolveIfStmt(Stmt* stmt);
        void ResolveBreakStmt(Stmt* stmt);
        void ResolveContinueStmt(Stmt* stmt);
        void ResolveReturnStmt(Stmt* stmt);

        void ResolveStmt(Stmt* stmt);

        void ResolveType(SourceLocation loc, SourceRange range, TypeInfo* type);
        void ResolveInitializer(Expr* initializer, TypeInfo* type, bool temporary);

        void PushScope(bool allowBreak = false, bool allowContinue = false);
        void PopScope();

        ConversionCost GetConversionCost(TypeInfo* dst, TypeInfo* src, ExprValueKind srcKind);
        void InsertImplicitCast(TypeInfo* dstType, TypeInfo* srcType, Expr* srcExpr, CastKind castKind);
        void RequireRValue(Expr* expr);
        void InsertArithmeticPromotion(Expr* lhs, Expr* rhs);

        void ReplaceExpr(Expr* src, Expr* newExpr);

        bool TypeIsEqual(TypeInfo* lhs, TypeInfo* rhs);
        size_t TypeGetSize(TypeInfo* t);

    private:
        std::unordered_map<std::string, bool> m_ImportedModules;

        Decl* m_BuiltInStringDestructor = nullptr;
        Decl* m_BuiltInStringCopyConstructor = nullptr;

        std::vector<Scope> m_Scopes;
        TypeInfo* m_ActiveReturnType = nullptr;
        TypeInfo* m_ActiveStruct = nullptr;

        bool m_TemporaryContext = false;

        CompilationContext* m_Context = nullptr;
    };

} // namespace Aria::Internal
