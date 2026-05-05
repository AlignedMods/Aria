#pragma once

#include "ariac/ast/expr.hpp"
#include "ariac/ast/stmt.hpp"
#include "ariac/ast/decl.hpp"
#include "ariac/ast/specifier.hpp"
#include "common/op_codes.hpp"

namespace Aria::Internal {

    class Emitter {
    private:
        struct Declaration {
            std::variant<size_t, std::string> data;
            TypeInfo* type = nullptr;
            Decl* destructor = nullptr;
        };

        struct Scope {
            std::unordered_map<std::string, size_t> declared_symbol_map;
            std::vector<Declaration> declared_symbols;
        };

        struct RuntimeStructDeclaration {
            std::unordered_map<std::string_view, size_t> field_indices;
            u16 index = 0;
        };

        struct StackFrame {
            size_t local_count = 0;
            std::vector<Scope> scopes;
            std::string name;

            std::unordered_map<std::string, size_t> parameters;
        };

        struct FutureDeclaration {
            std::string name;
            Decl* declaration = nullptr;
        };

    public:
        Emitter(CompilationContext* ctx);

    private:
        void emit_impl();

        void add_basic_types();
        void add_user_defined_types();
        void emit_declarations();
        void emit_start_end();

        void emit_boolean_literal_expr(Expr* expr,   ExprValueKind value_kind);
        void emit_character_literal_expr(Expr* expr, ExprValueKind value_kind);
        void emit_integer_literal_expr(Expr* expr,   ExprValueKind value_kind);
        void emit_floating_literal_expr(Expr* expr,  ExprValueKind value_kind);
        void emit_string_literal_expr(Expr* expr,    ExprValueKind value_kind);
        void emit_null_expr(Expr* expr,              ExprValueKind value_kind);
        void emit_decl_ref_expr(Expr* expr,          ExprValueKind value_kind);
        void emit_member_expr(Expr* expr,            ExprValueKind value_kind);
        void emit_builtin_member_expr(Expr* expr,    ExprValueKind value_kind);
        void emit_self_expr(Expr* expr,              ExprValueKind value_kind);
        void emit_temporary_expr(Expr* expr,         ExprValueKind value_kind);
        void emit_copy_expr(Expr* expr,              ExprValueKind value_kind);
        void emit_call_expr(Expr* expr,              ExprValueKind value_kind);
        void emit_construct_expr(Expr* expr,         ExprValueKind value_kind);
        void emit_array_subscript_expr(Expr* expr,   ExprValueKind value_kind);
        void emit_to_slice_expr(Expr* expr,          ExprValueKind value_kind);
        void emit_new_expr(Expr* expr,               ExprValueKind value_kind);
        void emit_delete_expr(Expr* expr,            ExprValueKind value_kind);
        void emit_format_expr(Expr* expr,            ExprValueKind value_kind);
        void emit_paren_expr(Expr* expr,             ExprValueKind value_kind);
        void emit_implicit_cast_expr(Expr* expr,     ExprValueKind value_kind);
        void emit_cast_expr(Expr* expr,              ExprValueKind value_kind);
        void emit_unary_operator_expr(Expr* expr,    ExprValueKind value_kind);
        void emit_binary_operator_expr(Expr* expr,   ExprValueKind value_kind);
        void emit_compound_assign_expr(Expr* expr,   ExprValueKind value_kind);

        void emit_expr(Expr* expr, ExprValueKind value_kind);

        void emit_translation_unit_decl(Decl* decl);
        void emit_var_decl(Decl* decl);
        void emit_param_decl(Decl* decl);
        void emit_function_decl(Decl* decl);
        void emit_struct_decl(Decl* decl);

        void emit_decl(Decl* decl);

        void emit_block_stmt(Stmt* stmt);
        void emit_while_stmt(Stmt* stmt);
        void emit_do_while_stmt(Stmt* stmt);
        void emit_for_stmt(Stmt* stmt);
        void emit_if_stmt(Stmt* stmt);
        void emit_break_stmt(Stmt* stmt);
        void emit_continue_stmt(Stmt* stmt);
        void emit_return_stmt(Stmt* stmt);
        void emit_expr_stmt(Stmt* stmt);
        void emit_decl_stmt(Stmt* stmt);

        void emit_stmt(Stmt* stmt);

        void emit_destructors(const std::vector<Declaration>& declarations);

        void push_stack_frame(const std::string& name);
        void pop_stack_frame();

        void push_scope();
        void pop_scope();

        void merge_pending_op_codes();

        u16 type_info_to_vm_type_idx(TypeInfo* t);

        std::string mangle_function(FunctionDecl* fn);

    private:
        OpCodes m_op_codes;
        std::vector<OpCode> m_pending_op_codes;
        CompilerReflectionData m_reflection_data;

        std::string m_namespace;
        std::string m_active_namespace;

        Stmt* m_root_ast_node = nullptr;

        Scope m_global_scope;

        StackFrame m_active_stack_frame;

        std::unordered_map<Decl*, RuntimeStructDeclaration> m_structs;
        std::unordered_map<TypeKind, u16> m_basic_types;
        u16 m_struct_index = 0;

        // Counters
        size_t m_and_counter = 0;
        size_t m_or_counter = 0;
        size_t m_loop_counter = 0;
        size_t m_if_counter = 0;

        std::vector<Declaration> m_temporaries;
    
        CompilationContext* m_context = nullptr;
    };

} // namespace Aria::Internal
