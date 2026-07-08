#pragma once

#include "ariac/lexer/tokens.hpp"
#include "ariac/compilation_context.hpp"
#include "ariac/ast/expr.hpp"
#include "ariac/ast/stmt.hpp"
#include "ariac/ast/decl.hpp"
#include "ariac/ast/specifier.hpp"

#include <unordered_map>
#include <functional>

namespace ariac {

    class Parser {
    public:
        Parser();

    private:
        void add_expr_rules();
        void parse_impl();

        Token* peek(size_t count = 0);
        Token& consume();
        Token* try_consume(TokenKind kind, const std::string& expect);
        bool match(TokenKind kind);

        // Expressions
        Expr* parse_grouping(Expr* left);
        Expr* parse_cast(Expr* left);
        Expr* parse_call(Expr* left);
        BuiltinCallKind get_builtin_call_from_token(Token* token);
        UnaryOperatorKind get_unary_operator_from_token(Token* token);
        BinaryOperatorKind get_binary_operator_from_token(Token* token);
        Expr* parse_unary(Expr* left);
        Expr* parse_infix_unary(Expr* left);
        Expr* parse_binary(Expr* left);
        Expr* parse_array_subscript(Expr* left);
        Expr* parse_compound_assignment(Expr* left);
        Expr* parse_member(Expr* left);
        Expr* parse_primary(Expr* left);
        Expr* parse_identifier(Token t);
        Expr* parse_env(Expr* left);
        Expr* parse_new(Expr* left);
        Expr* parse_delete(Expr* left);
        Expr* parse_builtin_call(Expr* left);

        Expr* parse_precedence_with_left(Expr* left, size_t precedence);
        Expr* parse_precedence(size_t precedence);
        Expr* parse_expression();
        Expr* parse_term();
        bool is_expression();

        bool is_primitive_type();
        bool is_type();
        TypeInfo* parse_type();

        Stmt* parse_block(bool unsafe = false);
        Stmt* parse_block_inline(bool unsafe = false);
        Stmt* parse_while();
        Stmt* parse_do_while();
        Stmt* parse_for();
        Stmt* parse_if();

        Stmt* parse_break();
        Stmt* parse_continue();
        Stmt* parse_return();
        Stmt* parse_defer();

        Stmt* parse_expression_statement();

        Stmt* parse_statement();

        // Declarations
        Decl* parse_module_decl();
        Decl* parse_import_decl();
        Decl* parse_let_decl();
        Decl* parse_const_decl(bool global);
        Decl* parse_variable_decl(bool global, bool const_, LinkageKind linkage = LinkageKind::None);
        Decl* parse_function_decl(LinkageKind linkage);
        std::pair<TinyVector<Decl*>, TinyVector<TypeInfo*>> parse_function_params(bool* var_arg);
        TinyVector<Decl*> parse_generic_params();
        Decl* parse_struct_decl();
        Decl* parse_impl_decl();
        Decl* parse_extern_decl();
        Decl* parse_static_decl();
        Decl* parse_typedef_decl();
        Decl* parse_enum_decl();

        TinyVector<DeclAttribute> parse_decl_attributes();

        std::string_view parse_module_path();

        Decl* parse_global();

        void split_token();

        void sync_global(); // Syncs the parser to a common sync point in a global context
        void sync_local(); // Syncs the parser to a common sync point in a local (block) context
        void sync_params(); // Syncs the parser to a common sync point in a function parameter context

        void error_expected(const std::string& expect, SourceLoc loc);

        bool stmt_ok(Stmt* stmt);
        bool expr_ok(Expr* expr);
        bool decl_ok(Decl* decl);

    private:
        size_t m_index = 0;
        Tokens m_tokens;

        bool m_declared_module = false;

        DeclVisibility m_current_visibility = DeclVisibility::Public;

        using ParseExprFn = std::function<Expr*(Expr*)>;
        struct ParseExprRule {
            ParseExprFn prefix = nullptr;
            ParseExprFn infix = nullptr;
            size_t precedence = 0;
        };
        std::unordered_map<TokenKind, ParseExprRule> m_expr_rules;
    };

} // namespace ariac
