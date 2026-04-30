#pragma once

#include "aria/internal/compiler/ast/expr.hpp"
#include "aria/internal/compiler/ast/decl.hpp"
#include "aria/internal/compiler/ast/stmt.hpp"
#include "aria/internal/compiler/ast/specifier.hpp"
#include "aria/internal/compiler/compilation_context.hpp"

#include <unordered_map>

namespace Aria::Internal {


    struct ConversionCost {
        CastKind Kind = CastKind::Invalid;

        bool CastNeeded = false;
        bool ImplicitCastPossible = false;
        bool ExplicitCastPossible = false;
    };

    class SemanticAnalyzer {
    private:
        struct Declaration {
            TypeInfo* ResolvedType = nullptr;
            Decl* SourceDeclaration = nullptr;
            DeclKind Kind = DeclKind::Var;
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

        void ResolveBooleanConstantExpr(Expr* expr);
        void ResolveCharacterConstantExpr(Expr* expr);
        void ResolveIntegerConstantExpr(Expr* expr);
        void ResolveFloatingConstantExpr(Expr* expr);
        void ResolveStringConstantExpr(Expr* expr);
        void ResolveNullExpr(Expr* expr);
        void ResolveDeclRefExpr(Expr* expr);
        void ResolveMemberExpr(Expr* expr);
        void ResolveBuiltinMemberExpr(Expr* expr);
        void ResolveTemporaryExpr(Expr* expr);
        void ResolveCopyExpr(Expr* expr);
        void ResolveCallExpr(Expr* expr);
        void ResolveConstructExpr(Expr* expr);
        void ResolveMethodCallExpr(Expr* expr);
        void ResolveArraySubscriptExpr(Expr* expr);
        void ResolveToSliceExpr(Expr* expr);
        void ResolveNewExpr(Expr* expr);
        void ResolveDeleteExpr(Expr* expr);
        void ResolveFormatExpr(Expr* expr);
        void ResolveParenExpr(Expr* expr);
        void ResolveCastExpr(Expr* expr);
        void ResolveImplicitCastExpr(Expr* expr);
        void ResolveUnaryOperatorExpr(Expr* expr);
        void ResolveBinaryOperatorExpr(Expr* expr);
        void ResolveCompoundAssignExpr(Expr* expr);

        void ResolveExpr(Expr* expr);

        void ResolveTranslationUnitDecl(Decl* decl);
        void ResolveModuleDecl(Decl* decl);
        void ResolveVarDecl(Decl* decl);
        void ResolveParamDecl(Decl* decl);
        void ResolveFunctionDecl(Decl* decl);
        void ResolveOverloadedFunctionDecl(Decl* decl);
        void ResolveStructDecl(Decl* decl);
        void ResolveFieldDecl(Decl* decl);
        void ResolveConstructorDecl(Decl* decl);
        void ResolveDestructorDecl(Decl* decl);
        void ResolveMethodDecl(Decl* decl);
        void ResolveBuiltinCopyConstructorDecl(Decl* decl);
        void ResolveBuiltinDestructorDecl(Decl* decl);

        void ResolveDecl(Decl* decl);

        void ResolveNopStmt(Stmt* stmt);
        void ResolveImportStmt(Stmt* stmt);
        void ResolveBlockStmt(Stmt* stmt);
        void ResolveWhileStmt(Stmt* stmt);
        void ResolveDoWhileStmt(Stmt* stmt);
        void ResolveForStmt(Stmt* stmt);
        void ResolveIfStmt(Stmt* stmt);
        void ResolveBreakStmt(Stmt* stmt);
        void ResolveContinueStmt(Stmt* stmt);
        void ResolveReturnStmt(Stmt* stmt);
        void ResolveExprStmt(Stmt* stmt);
        void ResolveDeclStmt(Stmt* stmt);

        void ResolveStmt(Stmt* stmt);

        void ResolveType(SourceLocation loc, SourceRange range, TypeInfo* type);

        void ResolveVarInitializer(Decl* decl);
        void ResolveParamInitializer(TypeInfo* paramType, Expr* arg);
        void CreateDefaultInitializer(Expr** expr, TypeInfo* type, SourceLocation loc, SourceRange range);

        void PushScope(bool allowBreak = false, bool allowContinue = false);
        void PopScope();

        ConversionCost GetConversionCost(TypeInfo* dst, TypeInfo* src);
        void InsertImplicitCast(TypeInfo* dstType, TypeInfo* srcType, Expr* srcExpr, CastKind castKind);
        void RequireRValue(Expr* expr);
        void InsertArithmeticPromotion(Expr* lhs, Expr* rhs);

        void ReplaceExpr(Expr* src, Expr* newExpr);
        void ReplaceDecl(Decl* src, Decl* newDecl);

        bool TypeIsEqual(TypeInfo* lhs, TypeInfo* rhs);
        size_t TypeGetSize(TypeInfo* t);
        bool TypeIsTrivial(TypeInfo* t);

        std::string MangleFunction(FunctionDecl* fn);

    private:
        std::unordered_map<std::string, bool> m_ImportedModules;

        bool m_TemporaryContext = false;
        bool m_UnsafeContext = false;

        bool m_CanReachEndOfFunction = true;

        Decl* m_BuiltInStringDestructor = nullptr;
        Decl* m_BuiltInStringCopyConstructor = nullptr;

        std::vector<Scope> m_Scopes;
        TypeInfo* m_ActiveReturnType = nullptr;
        TypeInfo* m_ActiveStruct = nullptr;

        CompilationContext* m_Context = nullptr;
    };

} // namespace Aria::Internal
