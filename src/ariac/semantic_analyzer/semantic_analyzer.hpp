#pragma once

#include "ariac/ast/expr.hpp"
#include "ariac/ast/decl.hpp"
#include "ariac/ast/stmt.hpp"
#include "ariac/ast/specifier.hpp"
#include "ariac/compilation_context.hpp"

#include <unordered_map>
#include <any>

namespace ariac {

    struct ConversionCost {
        CastKind kind = CastKind::Invalid;

        bool cast_needed = false;
        bool implicit_cast_possible = false;
        bool explicit_cast_possible = false;
    };

    class SemanticAnalyzer {
    private:
        struct Declaration {
            TypeInfo* resolved_type = nullptr;
            Decl* source_decl = nullptr;
            DeclKind kind = DeclKind::Var;
        };

        struct Scope {
            std::unordered_map<std::string_view, Declaration> declarations;
            bool allow_break_stmt = false;
            bool allow_continue_stmt = false;
            bool reaches_end = true;
        };

    public:
        SemanticAnalyzer();

    private:
        void sema_impl();

        // Passes
        void pass_imports();
        void pass_decls();
        void pass_code();

        void add_unit_to_module(Module* module, CompilationUnit* unit);
        void resolve_unit_imports(Module* module, CompilationUnit* unit);

        void resolve_module_type_decls(Module* module);
        void resolve_module_decls(Module* module);
        void resolve_unit_type_decls(Module* module, CompilationUnit* unit);
        void resolve_unit_decls(Module* module, CompilationUnit* unit);

        void resolve_module_code(Module* module);
        void resolve_unit_code(Module* module, CompilationUnit* unit);

        void resolve_boolean_literal_expr(Expr* expr);
        void resolve_character_literal_expr(Expr* expr);
        void resolve_integer_literal_expr(Expr* expr);
        void resolve_floating_literal_expr(Expr* expr);
        void resolve_string_literal_expr(Expr* expr);
        void resolve_null_expr(Expr* expr);
        void resolve_decl_ref_expr(Expr* expr);
        void resolve_member_expr(Expr* expr);
        void resolve_self_expr(Expr* expr);
        void resolve_call_expr(Expr* expr);
        void resolve_construct_expr(Expr* expr);
        void resolve_method_call_expr(Expr* expr);
        void resolve_array_subscript_expr(Expr* expr);
        void resolve_to_slice_expr(Expr* expr);
        void resolve_new_expr(Expr* expr);
        void resolve_delete_expr(Expr* expr);
        void resolve_sizeof_expr(Expr* expr);
        void resolve_paren_expr(Expr* expr);
        void resolve_cast_expr(Expr* expr);
        void resolve_implicit_cast_expr(Expr* expr);
        void resolve_unary_operator_expr(Expr* expr);
        void resolve_binary_operator_expr(Expr* expr);
        void resolve_compound_assign_expr(Expr* expr);

        void resolve_expr(Expr* expr);

        void resolve_name_specifier(Specifier* specifier);

        void resolve_var_decl(Decl* decl);
        void resolve_param_decl(Decl* decl);
        void resolve_function_decl(Decl* decl);
        void resolve_struct_decl(Decl* decl);
        void resolve_impl_decl(Decl* decl);
        void resolve_typedef_decl(Decl* decl);
        void resolve_enum_decl(Decl* decl);
        void resolve_generic_decl(Decl* decl);

        void resolve_decl_attributes(Decl* decl, TinyVector<DeclAttribute> attrs, bool* erase_decl);

        void resolve_decl(Decl* decl);

        void resolve_block_stmt(Stmt* stmt);
        void resolve_while_stmt(Stmt* stmt);
        void resolve_do_while_stmt(Stmt* stmt);
        void resolve_for_stmt(Stmt* stmt);
        void resolve_if_stmt(Stmt* stmt);
        void resolve_break_stmt(Stmt* stmt);
        void resolve_continue_stmt(Stmt* stmt);
        void resolve_return_stmt(Stmt* stmt);
        void resolve_defer_stmt(Stmt* stmt);
        void resolve_expr_stmt(Stmt* stmt);
        void resolve_decl_stmt(Stmt* stmt);

        void resolve_stmt(Stmt* stmt);

        void resolve_type(TypeInfo* type);

        void resolve_var_initializer(Decl* decl);
        void resolve_param_initializer(TypeInfo* param_type, Expr* arg);

        bool is_const_expr(Expr* expr);
        Expr* eval_const_expr(Expr* expr);

        void push_scope(bool allow_break = false, bool allow_continue = false);
        void pop_scope();

        ConversionCost get_conversion_cost(TypeInfo* dst, TypeInfo* src);
        void insert_implicit_cast(TypeInfo* dst_type, TypeInfo* src_type, Expr* src_expr, CastKind cast_kind);
        void require_rvalue(Expr* expr);
        void maybe_promote_to_int(Expr* expr);
        void insert_arithmetic_promotion(Expr* lhs, Expr* rhs);
        bool cast_needs_rvalue(CastKind kind);

        void replace_expr(Expr* src, Expr* new_expr);
        void replace_decl(Decl* src, Decl* new_decl);

        bool compare_module_names(std::string_view specifier, std::string_view module_name);

        bool type_is_equal(TypeInfo* lhs, TypeInfo* rhs);
        bool type_is_trivial(TypeInfo* t);

    private:
        bool m_temporary_context = false;

        std::vector<Decl*> m_generic_types;
        std::unordered_map<std::string_view, TypeInfo*> m_specialized_generic_types;
        bool m_replace_generic_types = false;
        bool m_search_generics = false;

        std::vector<Scope> m_scopes;
        TypeInfo* m_active_return_type = nullptr;
        TypeInfo* m_active_struct = nullptr;
    };

} // namespace ariac
