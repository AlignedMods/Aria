#pragma once

#include "aria/internal/compiler/lexer/tokens.hpp"
#include "aria/internal/compiler/compilation_context.hpp"
#include "aria/internal/compiler/ast/expr.hpp"
#include "aria/internal/compiler/ast/stmt.hpp"
#include "aria/internal/compiler/ast/decl.hpp"
#include "aria/internal/compiler/ast/specifier.hpp"

#include <unordered_map>
#include <functional>

namespace Aria::Internal {

    class Parser {
    public:
        Parser(CompilationContext* ctx);

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
        UnaryOperatorKind get_unary_operator_from_token(Token* token);
        BinaryOperatorKind get_binary_operator_from_token(Token* token);
        Expr* parse_unary(Expr* left);
        Expr* parse_binary(Expr* left);
        Expr* parse_array_subscript(Expr* left);
        Expr* parse_compound_assignment(Expr* left);
        Expr* parse_member(Expr* left);
        Expr* parse_primary(Expr* left);
        Expr* parse_identifier(Token t);
        Expr* parse_new(Expr* left);
        Expr* parse_delete(Expr* left);
        Expr* parse_format(Expr* left);

        Expr* parse_precedence_with_left(Expr* left, size_t precedence);
        Expr* parse_precedence(size_t precedence);
        Expr* parse_expression();
        bool is_expression();

        bool is_primitive_type();
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

        Stmt* parse_expression_statement();
        Stmt* parse_declaration_statement(bool global);

        Stmt* parse_statement();

        // Declarations
        Decl* parse_module_decl();
        Stmt* parse_import_decl();
        Decl* parse_variable_decl(bool global);
        Decl* parse_function_decl();
        std::pair<TinyVector<Decl*>, TinyVector<TypeInfo*>> parse_function_params();
        Decl* parse_struct_decl();

        TinyVector<FunctionDecl::Attribute> parse_function_attrs();

        Stmt* parse_global();

        void sync_global(); // Syncs the parser to a common sync point in a global context
        void sync_local(); // Syncs the parser to a common sync point in a local (block) context

        void error_expected(const std::string& expect, SourceLocation loc, SourceRange range);

        bool stmt_ok(Stmt* stmt);
        bool expr_ok(Expr* expr);
        bool decl_ok(Decl* decl);

    private:
        size_t m_index = 0;
        Tokens m_tokens;

        bool m_declared_module = false;

        using ParseExprFn = std::function<Expr*(Expr*)>;
        struct ParseExprRule {
            ParseExprFn prefix = nullptr;
            ParseExprFn infix = nullptr;
            size_t precedence = 0;
        };
        std::unordered_map<TokenKind, ParseExprRule> m_expr_rules;

        CompilationContext* m_context = nullptr;
    };

} // namespace Aria::Internal
