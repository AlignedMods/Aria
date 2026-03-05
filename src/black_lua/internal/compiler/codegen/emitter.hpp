#pragma once

#include "black_lua/internal/compiler/ast/expr.hpp"
#include "black_lua/internal/compiler/ast/stmt.hpp"
#include "black_lua/internal/compiler/ast/decl.hpp"
#include "black_lua/internal/vm/vm.hpp"
#include "black_lua/internal/compiler/reflection/compiler_reflection.hpp"

namespace BlackLua::Internal {

    class Emitter {
    private:
        struct CompileStackSlot {
            StackSlotIndex Slot = 0;
            bool Relative = false;
        };

        struct Declaration {
            CompileStackSlot Slot;
            TypeInfo* Type = nullptr;
        };

        struct StackFrame {
            size_t SlotCount = 0;
            std::unordered_map<std::string, size_t> DeclaredSymbolMap;
            std::vector<Declaration> DeclaredSymbols;
        };

    public:
        Emitter(CompilationContext* ctx);

    private:
        void EmitImpl();

        CompileStackSlot EmitBooleanConstantExpr(Expr* expr);
        CompileStackSlot EmitCharacterConstantExpr(Expr* expr);
        CompileStackSlot EmitIntegerConstantExpr(Expr* expr);
        CompileStackSlot EmitFloatingConstantExpr(Expr* expr);
        CompileStackSlot EmitStringConstantExpr(Expr* expr);
        CompileStackSlot EmitVarRefExpr(Expr* expr);
        CompileStackSlot EmitCallExpr(Expr* expr);
        CompileStackSlot EmitParenExpr(Expr* expr);
        CompileStackSlot EmitCastExpr(Expr* expr);
        CompileStackSlot EmitUnaryOperatorExpr(Expr* expr);
        CompileStackSlot EmitBinaryOperatorExpr(Expr* expr);

        CompileStackSlot EmitExpr(Expr* expr);

        void EmitTranslationUnitDecl(Decl* decl);
        void EmitVarDecl(Decl* decl);
        void EmitParamDecl(Decl* decl);
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

        StackSlotIndex CompileToRuntimeStackSlot(CompileStackSlot slot);

        int32_t CreateLabel(const std::string& debugData = {});
        void PushBytes(size_t bytes, TypeInfo* type, const std::string& debugData = {});

        bool IsStackFrameGlobal();
        void IncrementStackSlotCount();
        CompileStackSlot GetStackTop();

        void EmitDestructors(const std::vector<Declaration>& declarations);

        void PushStackFrame();
        void PushCompilerStackFrame();
        void PopStackFrame();
        void PopCompilerStackFrame();

    private:
        std::vector<OpCode> m_OpCodes;
        CompilerReflectionData m_ReflectionData;

        Stmt* m_RootASTNode = nullptr;

        std::vector<StackFrame> m_StackFrames; // There is always one implicit top stack frame (global space)
    
        size_t m_LabelCount = 0;
        std::unordered_map<int32_t, Decl*> m_FunctionsToDeclare; // We do not immediately declare functions, we actually do them last
    
        CompilationContext* m_Context = nullptr;
    };

} // namespace BlackLua::Internal
