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
            size_t Size = 0;
            TypeInfo* Type = nullptr;
            bool Extern = false;
            bool Destruct = false;

            StringView Identifier;
        };

    public:
        Emitter(const ASTNodes* nodes, Context* ctx);

        const CompilerReflectionData& GetReflectionData() const;
        const OpCodes& GetOpCodes() const;
    
    private:
        void EmitImpl();

        Node* Peek();
        Node* Consume();
    
        StackSlotIndex CompileToRuntimeStackSlot(CompileStackSlot slot);
    
        int32_t CreateLabel(const std::string& debugData = {});
        void PushBytes(size_t bytes, TypeInfo* type, const std::string& debugData = {});
        void IncrementStackSlotCount();
        CompileStackSlot GetStackTop();
        void EmitDestructors(const std::vector<Declaration>& declarations);

        void PushStackFrame();
        void PushCompilerStackFrame();
        void PopStackFrame();
        void PopCompilerStackFrame();

        CompileStackSlot EmitNodeExpression(NodeExpr* expr);

        void EmitNodeCompound(NodeStmt* stmt);
    
        void EmitNodeVarDecl(NodeStmt* stmt);
        void EmitNodeParamDecl(NodeStmt* stmt);

        void EmitNodeFunctionDecl(NodeStmt* stmt);

        void EmitNodeStructDecl(NodeStmt* stmt);
    
        void EmitNodeWhile(NodeStmt* stmt);
        void EmitNodeDoWhile(NodeStmt* stmt);
    
        void EmitNodeIf(NodeStmt* stmt);
    
        void EmitNodeReturn(NodeStmt* stmt);

        void EmitNodeStatement(NodeStmt* stmt);

        void EmitFunctions();
    
        void EmitNode(Node* node);
    
    private:
        OpCodes m_OpCodes;
        size_t m_Index = 0;
        const ASTNodes* m_Nodes = nullptr;
    
        size_t m_SlotCount = 0;
        size_t m_LabelCount = 0;
    
        std::unordered_map<std::string, size_t> m_DeclaredSymbolMap;
        std::vector<Declaration> m_DeclaredSymbols;

        std::unordered_map<int32_t, NodeStmt*> m_FunctionsToDeclare; // We do not immediately declare functions, we actually do them last
    
        struct StackFrame {
            StackFrame* Parent = nullptr;
            size_t SlotCount = 0;
            std::unordered_map<std::string, size_t> DeclaredSymbolMap;
            std::vector<Declaration> DeclaredSymbols;
        };
    
        StackFrame* m_CurrentStackFrame = nullptr;

        CompilerReflectionData m_ReflectionData;
        Context* m_Context = nullptr;
    };

} // namespace BlackLua::Internal
