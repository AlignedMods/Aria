#pragma once

#include "aria/internal/compiler/ast/expr.hpp"
#include "aria/internal/compiler/ast/stmt.hpp"
#include "aria/internal/compiler/ast/decl.hpp"
#include "aria/internal/vm/vm.hpp"
#include "aria/internal/compiler/reflection/compiler_reflection.hpp"

namespace Aria::Internal {

    class Emitter {
    private:
        struct Declaration {
            std::variant<size_t, std::string> Data;
            TypeInfo* Type = nullptr;
        };

        struct RuntimeStructDeclaration {
            std::unordered_map<std::string, size_t> FieldIndices;
        };

        struct Scope {
            std::unordered_map<std::string, size_t> DeclaredSymbolMap;
            std::vector<Declaration> DeclaredSymbols;
        };

        struct StackFrame {
            size_t LocalCount = 0;
            std::vector<Scope> Scopes;
            std::string Name;

            std::unordered_map<std::string, size_t> Parameters;
            size_t ParameterCount = 0;
        };

        struct FutureDeclaration {
            std::string Name;
            Decl* Declaration = nullptr;
        };

    public:
        Emitter(CompilationContext* ctx);

    private:
        void EmitImpl();

        void EmitBooleanConstantExpr(Expr* expr,   ExprValueKind valueKind);
        void EmitCharacterConstantExpr(Expr* expr, ExprValueKind valueKind);
        void EmitIntegerConstantExpr(Expr* expr,   ExprValueKind valueKind);
        void EmitFloatingConstantExpr(Expr* expr,  ExprValueKind valueKind);
        void EmitStringConstantExpr(Expr* expr,    ExprValueKind valueKind);
        void EmitDeclRefExpr(Expr* expr,           ExprValueKind valueKind);
        void EmitMemberExpr(Expr* expr,            ExprValueKind valueKind);
        void EmitSelfExpr(Expr* expr,              ExprValueKind valueKind);
        void EmitTemporaryExpr(Expr* expr,         ExprValueKind valueKind);
        void EmitCopyExpr(Expr* expr,              ExprValueKind valueKind);
        void EmitCallExpr(Expr* expr,              ExprValueKind valueKind);
        void EmitMethodCallExpr(Expr* expr,        ExprValueKind valueKind);
        void EmitParenExpr(Expr* expr,             ExprValueKind valueKind);
        void EmitImplicitCastExpr(Expr* expr,      ExprValueKind valueKind);
        void EmitCastExpr(Expr* expr,              ExprValueKind valueKind);
        void EmitUnaryOperatorExpr(Expr* expr,     ExprValueKind valueKind);
        void EmitBinaryOperatorExpr(Expr* expr,    ExprValueKind valueKind);
        void EmitCompoundAssignExpr(Expr* expr,    ExprValueKind valueKind);

        void EmitExpr(Expr* expr, ExprValueKind valueKind);

        void EmitTranslationUnitDecl(Decl* decl);
        void EmitVarDecl(Decl* decl);
        void EmitParamDecl(Decl* decl);
        void EmitFunctionDecl(Decl* decl);
        void EmitStructDecl(Decl* decl);

        void EmitDecl(Decl* decl);

        void EmitCompoundStmt(Stmt* stmt);
        void EmitWhileStmt(Stmt* stmt);
        void EmitDoWhileStmt(Stmt* stmt);
        void EmitForStmt(Stmt* stmt);
        void EmitIfStmt(Stmt* stmt);
        void EmitReturnStmt(Stmt* stmt);

        void EmitStmt(Stmt* stmt);

        void EmitDeclarations(); // Emits all the user defined functions/structs/...

        bool IsStartStackFrame();
        bool IsGlobalScope();

        void EmitDestructors(const std::vector<Declaration>& declarations);

        void PushStackFrame(const std::string& name);
        void PopStackFrame();

        void PushScope();
        void PopScope();

        VMType TypeInfoToVMType(TypeInfo* t);

    private:
        std::vector<OpCode> m_OpCodes;
        CompilerReflectionData m_ReflectionData;

        Stmt* m_RootASTNode = nullptr;

        StackFrame m_ActiveStackFrame;
        Scope m_GlobalScope;

        std::unordered_map<std::string, RuntimeStructDeclaration> m_Structs;

        std::vector<FutureDeclaration> m_DeclarationsToDeclare; // We do not immediately declare functions/structs/..., we actually do them last

        // Counters
        size_t m_AndCounter = 0;
        size_t m_OrCounter = 0;
        size_t m_WhileCounter = 0;
        size_t m_DoWhileCounter = 0;
        size_t m_ForCounter = 0;
        size_t m_IfCounter = 0;

        std::vector<Declaration> m_Temporaries;
    
        CompilationContext* m_Context = nullptr;
    };

} // namespace Aria::Internal
