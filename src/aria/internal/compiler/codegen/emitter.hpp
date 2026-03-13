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

        struct Scope {
            std::unordered_map<std::string, size_t> DeclaredSymbolMap;
            std::vector<Declaration> DeclaredSymbols;
        };

        struct StackFrame {
            size_t SlotCount = 0;
            size_t LocalCount = 0;
            std::vector<Scope> Scopes;
            std::string Name;
        };

    public:
        Emitter(CompilationContext* ctx);

    private:
        void EmitImpl();

        void EmitBooleanConstantExpr(Expr* expr);
        void EmitCharacterConstantExpr(Expr* expr);
        void EmitIntegerConstantExpr(Expr* expr);
        void EmitFloatingConstantExpr(Expr* expr);
        void EmitStringConstantExpr(Expr* expr);
        void EmitDeclRefExpr(Expr* expr, bool lvalue);
        void EmitCallExpr(Expr* expr);
        void EmitParenExpr(Expr* expr);
        void EmitImplicitCastExpr(Expr* expr);
        void EmitCastExpr(Expr* expr);
        void EmitUnaryOperatorExpr(Expr* expr);
        void EmitBinaryOperatorExpr(Expr* expr);

        void EmitExpr(Expr* expr);

        void EmitTranslationUnitDecl(Decl* decl);
        void EmitVarDecl(Decl* decl);
        void EmitParamDecl(Decl* decl, i32 slot);
        void EmitFunctionDecl(Decl* decl);

        void EmitDecl(Decl* decl);

        void EmitCompoundStmt(Stmt* stmt);
        void EmitWhileStmt(Stmt* stmt);
        void EmitDoWhileStmt(Stmt* stmt);
        void EmitForStmt(Stmt* stmt);
        void EmitIfStmt(Stmt* stmt);
        void EmitReturnStmt(Stmt* stmt);

        void EmitStmt(Stmt* stmt);

        void EmitFunctions(); // Emits all the defined functions

        bool IsStartStackFrame();
        bool IsGlobalScope();
        void IncrementStackSlotCount();

        void EmitDestructors(const std::vector<Declaration>& declarations);

        void PushStackFrame(const std::string& name);
        void PopStackFrame();

        void PushScope();
        void PopScope();

    private:
        std::vector<OpCode> m_OpCodes;
        CompilerReflectionData m_ReflectionData;

        Stmt* m_RootASTNode = nullptr;

        StackFrame m_ActiveStackFrame;
        Scope m_GlobalScope;

        std::unordered_map<std::string, Decl*> m_FunctionsToDeclare; // We do not immediately declare functions, we actually do them last

        // Counters
        size_t m_IfCounter = 0;
    
        CompilationContext* m_Context = nullptr;
    };

} // namespace Aria::Internal
