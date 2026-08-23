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
            std::vector<Stmt*> defers;
            bool reaches_end = true;
        };

        struct JumpTarget {
            Stmt* target = nullptr;
            size_t scope_idx = 0;
        };

        struct FunctionContext {
            TypeInfo* return_type = nullptr;
            TypeInfo* struct_type = nullptr;
            std::vector<Scope> scopes;
        };

        using TemplateContext = std::unordered_map<std::string_view, Decl*>;

        struct TemplateInstantationContext {
            Decl* template_decl = nullptr;
            std::unordered_map<Decl*, TypeInfo*> template_types;
            SourceLoc loc;
        };

        struct ResolvedTemplateArg {
            TypeInfo* type = nullptr;
            bool is_deduced = false;
            SourceLoc loc;
        };
        using ResolvedTemplateMap = std::unordered_map<Decl*, ResolvedTemplateArg>;

        struct CaptureErrorContext {
            bool has_error = false;
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

        void resolve_unit_type_decls(Module* module, CompilationUnit* unit);
        void resolve_unit_decls(Module* module, CompilationUnit* unit);
        void resolve_unit_code(Module* module, CompilationUnit* unit);

        void resolve_boolean_literal_expr(Expr* expr);
        void resolve_character_literal_expr(Expr* expr);
        void resolve_integer_literal_expr(Expr* expr);
        void resolve_floating_literal_expr(Expr* expr);
        void resolve_string_literal_expr(Expr* expr);
        void resolve_null_expr(Expr* expr);
        void resolve_decl_ref_expr(Expr* expr);
        void resolve_typeinfo_expr(Expr* expr);
        void resolve_member_expr(Expr* expr);
        void resolve_type_member_expr(Expr* expr);
        void resolve_builtin_member_expr(Expr* expr);
        void resolve_dependent_member_expr(Expr* expr);
        void resolve_self_expr(Expr* expr);
        void resolve_call_expr(Expr* expr);
        void resolve_template_call_expr(Decl* template_, TinyVector<TypeInfo*> template_args, SourceLoc loc, TinyVector<Expr*> args, Decl** callee, TypeInfo** callee_type);
        bool resolve_call_arity(SourceLoc loc, FunctionType& fn_type, TinyVector<Expr*> args);
        void resolve_call_args(FunctionType& fn_type, TinyVector<Expr*>& args);
        void resolve_builtin_call_expr(Expr* expr);
        void resolve_construct_expr(Expr* expr);
        void resolve_array_literal_expr(Expr* expr);
        void resolve_method_call_expr(Expr* expr);
        void resolve_array_subscript_expr(Expr* expr);
        void resolve_to_slice_expr(Expr* expr);
        void resolve_materialize_temporary_expr(Expr* expr);
        void resolve_paren_expr(Expr* expr);
        void resolve_ternary_expr(Expr* expr);
        void resolve_cast_expr(Expr* expr);
        void resolve_implicit_cast_expr(Expr* expr);
        void resolve_unary_operator_expr(Expr* expr);
        void resolve_binary_operator_expr(Expr* expr);
        void resolve_compound_assign_expr(Expr* expr);

        void resolve_expr(Expr* expr);

        void resolve_name_specifier(Specifier* specifier);

        void resolve_import_decl(Decl* decl);
        void resolve_var_decl(Decl* decl);
        void resolve_param_decl(Decl* decl);
        void resolve_function_decl(Decl* decl);
        void resolve_struct_decl(Decl* decl);
        void resolve_typedef_decl(Decl* decl);
        void resolve_enum_decl(Decl* decl);
        void resolve_template_decl(Decl* decl);

        void resolve_function_body(Decl* decl);
        void resolve_struct_body(Decl* decl);
        void resolve_method_body(Decl* decl);
        void resolve_destructor_body(Decl* decl);

        Decl* specialize_template_func(SourceLoc loc, Decl* t, TinyVector<TypeInfo*> args);

        void resolve_decl_attributes(Decl* decl, TinyVector<DeclAttribute> attrs, bool* erase_decl);

        void resolve_decl(Decl* decl);

        void resolve_compound_stmt(Stmt* stmt);
        void resolve_while_stmt(Stmt* stmt);
        void resolve_do_while_stmt(Stmt* stmt);
        void resolve_for_stmt(Stmt* stmt);
        void resolve_if_stmt(Stmt* stmt);
        void resolve_switch_stmt(Stmt* stmt);
        void resolve_break_stmt(Stmt* stmt);
        void resolve_continue_stmt(Stmt* stmt);
        void resolve_nextcase_stmt(Stmt* stmt);
        void resolve_return_stmt(Stmt* stmt);
        void resolve_defer_stmt(Stmt* stmt);
        void resolve_compile_if_stmt(Stmt* stmt);
        void resolve_expr_stmt(Stmt* stmt);
        void resolve_decl_stmt(Stmt* stmt);

        void resolve_stmt(Stmt* stmt);

        void resolve_type(TypeInfo* type);

        void resolve_var_initializer(Decl* decl);
        void resolve_param_initializer(Decl* decl, Expr* arg);
        void resolve_param_default_arg(Decl* decl);

        bool is_const_expr(Expr* expr);
        Expr* eval_const_expr(Expr* expr);

        bool is_assignable_expr(Expr* expr);

        void push_scope();
        void pop_scope();

        std::pair<JumpTarget, JumpTarget> set_break_targets(Stmt* b, Stmt* c);
        void restore_break_targets(std::pair<JumpTarget, JumpTarget> prev);

        JumpTarget set_nextcase_target(Stmt* s);
        void restore_nextcase_target(JumpTarget prev);

        ConversionCost get_conversion_cost(TypeInfo* dst, TypeInfo* src);
        void insert_implicit_cast(TypeInfo* dst_type, TypeInfo* src_type, Expr* src_expr, CastKind cast_kind);
        void try_insert_implicit_cast(TypeInfo* dst_type, Expr* src_expr, std::string_view kind = "");
        void try_insert_explicit_cast(TypeInfo* dst_type, Expr* src_expr);
        void require_rvalue(Expr* expr);
        void maybe_promote_to_int(Expr* expr);
        void insert_arithmetic_promotion(Expr* lhs, Expr* rhs, BinaryOperatorKind op, Expr* e);
        void insert_materialize_temporary_expr(Expr* expr);
        void insert_temporary_expr(Expr* expr, Decl* dtor);
        void insert_expr_with_cleanups(Expr* expr);
        bool cast_needs_rvalue(CastKind kind);
        TypeInfo* type_from_decl(Decl* decl);
        TypeInfo* type_for_self(Decl* decl);
        Decl* type_get_destructor(TypeInfo* t);

        // Gets the compile time type from an expression
        // eg. e = int, this function would return 'int'
        // eg. e = 5, function returns nullptr
        TypeInfo* get_typeinfo(Expr* e);

        bool deduce_template_type(SourceLoc loc, TypeInfo* param_type, TypeInfo* arg_type, ResolvedTemplateMap& deduced_args);

        void replace_expr(Expr* src, Expr* new_expr);
        void replace_decl(Decl* src, Decl* new_decl);
        void replace_stmt(Stmt* src, Stmt* new_stmt);

        bool type_is_equal(TypeInfo* lhs, TypeInfo* rhs);
        bool type_is_trivial(TypeInfo* t);

        void report_error(SourceLoc loc, const std::string& error);
        void report_warning(SourceLoc loc, const std::string& error);
        void report_note(SourceLoc loc, const std::string& error);
        void report_diag(SourceLoc loc, const std::string& error, CompilerDiagKind kind = CompilerDiagKind::Error);
        void report_diag_with_notes(SourceLoc loc, const std::string& error, std::initializer_list<std::string> notes, CompilerDiagKind kind = CompilerDiagKind::Error);

    private:
        struct {
            bool call : 1 = false;
            bool address_of : 1 = false;
            bool temporary : 1 = false;
            bool needs_cleanup : 1 = false;
            bool struct_specilization : 1 = false;
            bool search_generics : 1 = false;
        } m_sema_context;

        std::vector<FunctionContext> m_functions;
        std::vector<TemplateContext> m_generics;
        std::vector<TemplateInstantationContext> m_generic_instantations;
        std::vector<CaptureErrorContext> m_error_captures;

        JumpTarget m_break_target;
        JumpTarget m_continue_target;
        JumpTarget m_nextcase_target;
    };

} // namespace ariac
