#pragma once

#include "ariac/ast/expr.hpp"
#include "ariac/ast/decl.hpp"
#include "ariac/ast/stmt.hpp"
#include "ariac/ast/specifier.hpp"
#include "ariac/compilation_context.hpp"

#include <unordered_map>

namespace Aria::Internal {

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
            std::unordered_map<std::string, Declaration> declarations;
            bool allow_break_stmt = false;
            bool allow_continue_stmt = false;
            bool reaches_end = true;
        };

    public:
        SemanticAnalyzer(CompilationContext* ctx);

    private:
        void sema_impl();

        // Passes
        void pass_imports();
        void pass_decls();
        void pass_code();

        void add_unit_to_module(Module* module, CompilationUnit* unit);
        void resolve_module_imports(Module* module);
        void resolve_unit_imports(Module* module, CompilationUnit* unit);

        void resolve_module_decls(Module* module);
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
        void resolve_call_expr(Expr* expr);
        void resolve_array_subscript_expr(Expr* expr);
        void resolve_to_slice_expr(Expr* expr);
        void resolve_new_expr(Expr* expr);
        void resolve_delete_expr(Expr* expr);
        void resolve_format_expr(Expr* expr);
        void resolve_paren_expr(Expr* expr);
        void resolve_cast_expr(Expr* expr);
        void resolve_unary_operator_expr(Expr* expr);
        void resolve_binary_operator_expr(Expr* expr);
        void resolve_compound_assign_expr(Expr* expr);

        void resolve_expr(Expr* expr);

        void resolve_translation_unit_decl(Decl* decl);
        void resolve_var_decl(Decl* decl);
        void resolve_param_decl(Decl* decl);
        void resolve_function_decl(Decl* decl);
        void resolve_struct_decl(Decl* decl);

        void resolve_decl(Decl* decl);

        void resolve_block_stmt(Stmt* stmt);
        void resolve_while_stmt(Stmt* stmt);
        void resolve_do_while_stmt(Stmt* stmt);
        void resolve_for_stmt(Stmt* stmt);
        void resolve_if_stmt(Stmt* stmt);
        void resolve_break_stmt(Stmt* stmt);
        void resolve_continue_stmt(Stmt* stmt);
        void resolve_return_stmt(Stmt* stmt);
        void resolve_expr_stmt(Stmt* stmt);
        void resolve_decl_stmt(Stmt* stmt);

        void resolve_stmt(Stmt* stmt);

        void resolve_type(SourceLocation loc, SourceRange range, TypeInfo* type);

        void resolve_var_initializer(Decl* decl);
        void resolve_param_initializer(TypeInfo* param_type, Expr* arg);
        void create_default_initializer(Expr** expr, TypeInfo* type, SourceLocation loc, SourceRange range);

        bool is_const_expr(Expr* expr);
        bool eval_expr_bool(Expr* expr);

        void push_scope(bool allow_break = false, bool allow_continue = false);
        void pop_scope();

        ConversionCost get_conversion_cost(TypeInfo* dst, TypeInfo* src);
        void insert_implicit_cast(TypeInfo* dst_type, TypeInfo* src_type, Expr* src_expr, CastKind cast_kind);
        void require_rvalue(Expr* expr);
        void insert_arithmetic_promotion(Expr* lhs, Expr* rhs);

        void replace_expr(Expr* src, Expr* new_expr);
        void replace_decl(Decl* src, Decl* new_decl);

        bool compare_module_names(std::string_view specifier, std::string_view module_name);

        bool type_is_equal(TypeInfo* lhs, TypeInfo* rhs);
        size_t type_get_size(TypeInfo* t);
        bool type_is_trivial(TypeInfo* t);

        std::string mangle_function(FunctionDecl* fn);

    private:
        bool m_temporary_context = false;
        bool m_unsafe_context = false;

        Decl* m_builtin_string_destructor = nullptr;
        Decl* m_builtin_string_copy_constructor = nullptr;

        std::vector<Scope> m_scopes;
        TypeInfo* m_active_return_type = nullptr;
        TypeInfo* m_active_struct = nullptr;

        CompilationContext* m_context = nullptr;
    };

} // namespace Aria::Internal
