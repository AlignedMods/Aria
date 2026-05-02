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
        void emit_impl();

        void add_basic_types();
        void add_user_defined_types();
        void emit_declarations();
        void emit_start_end();

        void emit_BooleanConstant_expr(Expr* expr,   ExprValueKind valueKind);
        void emit_CharacterConstant_expr(Expr* expr, ExprValueKind valueKind);
        void emit_IntegerConstant_expr(Expr* expr,   ExprValueKind valueKind);
        void emit_FloatingConstant_expr(Expr* expr,  ExprValueKind valueKind);
        void emit_StringConstant_expr(Expr* expr,    ExprValueKind valueKind);
        void emit_Null_expr(Expr* expr,              ExprValueKind valueKind);
        void emit_DeclRef_expr(Expr* expr,           ExprValueKind valueKind);
        void emit_Member_expr(Expr* expr,            ExprValueKind valueKind);
        void emit_BuiltinMember_expr(Expr* expr,     ExprValueKind valueKind);
        void emit_Self_expr(Expr* expr,              ExprValueKind valueKind);
        void emit_Temporary_expr(Expr* expr,         ExprValueKind valueKind);
        void emit_Copy_expr(Expr* expr,              ExprValueKind valueKind);
        void emit_Call_expr(Expr* expr,              ExprValueKind valueKind);
        void emit_Construct_expr(Expr* expr,         ExprValueKind valueKind);
        void emit_MethodCall_expr(Expr* expr,        ExprValueKind valueKind);
        void emit_ArraySubscript_expr(Expr* expr,    ExprValueKind valueKind);
        void emit_ToSlice_expr(Expr* expr,           ExprValueKind valueKind);
        void emit_New_expr(Expr* expr,               ExprValueKind valueKind);
        void emit_Delete_expr(Expr* expr,            ExprValueKind valueKind);
        void emit_Format_expr(Expr* expr,            ExprValueKind valueKind);
        void emit_Paren_expr(Expr* expr,             ExprValueKind valueKind);
        void emit_ImplicitCast_expr(Expr* expr,      ExprValueKind valueKind);
        void emit_Cast_expr(Expr* expr,              ExprValueKind valueKind);
        void emit_UnaryOperator_expr(Expr* expr,     ExprValueKind valueKind);
        void emit_BinaryOperator_expr(Expr* expr,    ExprValueKind valueKind);
        void emit_CompoundAssign_expr(Expr* expr,    ExprValueKind valueKind);

        void emit_expr(Expr* expr, ExprValueKind valueKind);

        void emit_TranslationUnit_decl(Decl* decl);
        void emit_Module_decl(Decl* decl);
        void emit_Var_decl(Decl* decl);
        void emit_Param_decl(Decl* decl);
        void emit_Function_decl(Decl* decl);
        void emit_OverloadedFunction_decl(Decl* decl);
        void emit_Struct_decl(Decl* decl);
        void emit_Field_decl(Decl* decl);
        void emit_Constructor_decl(Decl* decl);
        void emit_Destructor_decl(Decl* decl);
        void emit_Method_decl(Decl* decl);
        void emit_BuiltinCopyConstructor_decl(Decl* decl);
        void emit_BuiltinDestructor_decl(Decl* decl);

        void emit_decl(Decl* decl);

        void emit_Nop_stmt(Stmt* stmt);
        void emit_Import_stmt(Stmt* stmt);
        void emit_Block_stmt(Stmt* stmt);
        void emit_While_stmt(Stmt* stmt);
        void emit_DoWhile_stmt(Stmt* stmt);
        void emit_For_stmt(Stmt* stmt);
        void emit_If_stmt(Stmt* stmt);
        void emit_Break_stmt(Stmt* stmt);
        void emit_Continue_stmt(Stmt* stmt);
        void emit_Return_stmt(Stmt* stmt);
        void emit_Expr_stmt(Stmt* stmt);
        void emit_Decl_stmt(Stmt* stmt);

        void emit_stmt(Stmt* stmt);

        void emit_destructors(const std::vector<Declaration>& declarations);

        void push_stack_frame(const std::string& name);
        void pop_stack_frame();

        void push_scope();
        void pop_scope();

        void merge_pending_op_codes();

        OpCode type_info_to_vm_type_idx(TypeInfo* t);

        std::string mangle_function(FunctionDecl* fn);

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
