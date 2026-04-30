#pragma once

#include "aria/internal/compiler/ast/expr.hpp"
#include "aria/internal/compiler/ast/stmt.hpp"
#include "aria/internal/compiler/ast/decl.hpp"
#include "aria/internal/compiler/ast/specifier.hpp"
#include "aria/internal/vm/vm.hpp"
#include "aria/internal/compiler/reflection/compiler_reflection.hpp"

namespace Aria::Internal {

    class Emitter {
    private:
        struct Declaration {
            std::variant<size_t, std::string> Data;
            TypeInfo* Type = nullptr;
            Decl* Destructor = nullptr;
        };

        struct Scope {
            std::unordered_map<std::string, size_t> DeclaredSymbolMap;
            std::vector<Declaration> DeclaredSymbols;
        };

        struct RuntimeStructDeclaration {
            std::unordered_map<std::string_view, size_t> FieldIndices;
            u16 Index = 0;
        };

        struct StackFrame {
            size_t LocalCount = 0;
            std::vector<Scope> Scopes;
            std::string Name;

            std::unordered_map<std::string, size_t> Parameters;
        };

        struct FutureDeclaration {
            std::string Name;
            Decl* Declaration = nullptr;
        };

    public:
        Emitter(CompilationContext* ctx);

    private:
        void EmitImpl();

        void AddBasicTypes();
        void AddUserDefinedTypes();
        void EmitDeclarations();
        void EmitStartEnd();

        void EmitBooleanConstantExpr(Expr* expr,   ExprValueKind valueKind);
        void EmitCharacterConstantExpr(Expr* expr, ExprValueKind valueKind);
        void EmitIntegerConstantExpr(Expr* expr,   ExprValueKind valueKind);
        void EmitFloatingConstantExpr(Expr* expr,  ExprValueKind valueKind);
        void EmitStringConstantExpr(Expr* expr,    ExprValueKind valueKind);
        void EmitNullExpr(Expr* expr,              ExprValueKind valueKind);
        void EmitDeclRefExpr(Expr* expr,           ExprValueKind valueKind);
        void EmitMemberExpr(Expr* expr,            ExprValueKind valueKind);
        void EmitBuiltinMemberExpr(Expr* expr,     ExprValueKind valueKind);
        void EmitSelfExpr(Expr* expr,              ExprValueKind valueKind);
        void EmitTemporaryExpr(Expr* expr,         ExprValueKind valueKind);
        void EmitCopyExpr(Expr* expr,              ExprValueKind valueKind);
        void EmitCallExpr(Expr* expr,              ExprValueKind valueKind);
        void EmitConstructExpr(Expr* expr,         ExprValueKind valueKind);
        void EmitMethodCallExpr(Expr* expr,        ExprValueKind valueKind);
        void EmitArraySubscriptExpr(Expr* expr,    ExprValueKind valueKind);
        void EmitToSliceExpr(Expr* expr,           ExprValueKind valueKind);
        void EmitNewExpr(Expr* expr,               ExprValueKind valueKind);
        void EmitDeleteExpr(Expr* expr,            ExprValueKind valueKind);
        void EmitFormatExpr(Expr* expr,            ExprValueKind valueKind);
        void EmitParenExpr(Expr* expr,             ExprValueKind valueKind);
        void EmitImplicitCastExpr(Expr* expr,      ExprValueKind valueKind);
        void EmitCastExpr(Expr* expr,              ExprValueKind valueKind);
        void EmitUnaryOperatorExpr(Expr* expr,     ExprValueKind valueKind);
        void EmitBinaryOperatorExpr(Expr* expr,    ExprValueKind valueKind);
        void EmitCompoundAssignExpr(Expr* expr,    ExprValueKind valueKind);

        void EmitExpr(Expr* expr, ExprValueKind valueKind);

        void EmitTranslationUnitDecl(Decl* decl);
        void EmitModuleDecl(Decl* decl);
        void EmitVarDecl(Decl* decl);
        void EmitParamDecl(Decl* decl);
        void EmitFunctionDecl(Decl* decl);
        void EmitOverloadedFunctionDecl(Decl* decl);
        void EmitStructDecl(Decl* decl);
        void EmitFieldDecl(Decl* decl);
        void EmitConstructorDecl(Decl* decl);
        void EmitDestructorDecl(Decl* decl);
        void EmitMethodDecl(Decl* decl);
        void EmitBuiltinCopyConstructorDecl(Decl* decl);
        void EmitBuiltinDestructorDecl(Decl* decl);

        void EmitDecl(Decl* decl);

        void EmitNopStmt(Stmt* stmt);
        void EmitImportStmt(Stmt* stmt);
        void EmitBlockStmt(Stmt* stmt);
        void EmitWhileStmt(Stmt* stmt);
        void EmitDoWhileStmt(Stmt* stmt);
        void EmitForStmt(Stmt* stmt);
        void EmitIfStmt(Stmt* stmt);
        void EmitBreakStmt(Stmt* stmt);
        void EmitContinueStmt(Stmt* stmt);
        void EmitReturnStmt(Stmt* stmt);
        void EmitExprStmt(Stmt* stmt);
        void EmitDeclStmt(Stmt* stmt);

        void EmitStmt(Stmt* stmt);

        void EmitDestructors(const std::vector<Declaration>& declarations);

        void PushStackFrame(const std::string& name);
        void PopStackFrame();

        void PushScope();
        void PopScope();

        void MergePendingOpCodes();

        OpCode TypeInfoToVMTypeIdx(TypeInfo* t);

        std::string MangleFunction(FunctionDecl* fn);

    private:
        OpCodes m_OpCodes;
        std::vector<OpCode> m_PendingOpCodes;
        CompilerReflectionData m_ReflectionData;

        std::string m_Namespace;
        std::string m_ActiveNamespace;

        Stmt* m_RootASTNode = nullptr;

        Scope m_GlobalScope;

        StackFrame m_ActiveStackFrame;

        std::unordered_map<Decl*, RuntimeStructDeclaration> m_Structs;
        std::unordered_map<TypeKind, u16> m_BasicTypes;
        u16 m_StructIndex = 0;

        // Counters
        size_t m_AndCounter = 0;
        size_t m_OrCounter = 0;
        size_t m_LoopCounter = 0;
        size_t m_IfCounter = 0;

        std::vector<Declaration> m_Temporaries;
    
        CompilationContext* m_Context = nullptr;
    };

} // namespace Aria::Internal
