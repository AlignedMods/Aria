#pragma once

#include "aria/internal/compiler/lexer/tokens.hpp"
#include "aria/internal/compiler/compilation_context.hpp"
#include "aria/internal/compiler/core/string_builder.hpp"
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
        void AddExprRules();
        void ParseImpl();

        Token* Peek(size_t count = 0);
        Token& Consume();
        Token* TryConsume(TokenKind kind, const std::string& expect);
        bool Match(TokenKind kind);

        // Expressions
        Expr* ParseGrouping(Expr* left);
        Expr* ParseCast(Expr* left);
        Expr* ParseCall(Expr* left);
        UnaryOperatorKind GetUnaryOperatorFromToken(Token* token);
        BinaryOperatorKind GetBinaryOperatorFromToken(Token* token);
        Expr* ParseUnary(Expr* left);
        Expr* ParseBinary(Expr* left);
        Expr* ParseCompoundAssignment(Expr* left);
        Expr* ParseMember(Expr* left);
        Expr* ParsePrimary(Expr* left);
        Expr* ParseIdentifier(Token t);

        Expr* ParsePrecedenceWithLeft(Expr* left, size_t precedence);
        Expr* ParsePrecedence(size_t precedence);
        Expr* ParseExpression();

        bool IsPrimitiveType();
        TypeInfo* ParseType();

        Stmt* ParseBlock();
        Stmt* ParseBlockInline();
        Stmt* ParseWhile();
        Stmt* ParseDoWhile();
        Stmt* ParseFor();
        Stmt* ParseIf();

        Stmt* ParseBreak();
        Stmt* ParseContinue();
        Stmt* ParseReturn();

        Stmt* ParseExpressionStatement();
        Stmt* ParseDeclarationStatement(bool global);

        Stmt* ParseStatement();

        // Declarations
        Decl* ParseModuleDecl();
        Stmt* ParseImportStmt();
        Decl* ParseVariableDecl(bool global);
        Decl* ParseFunctionDecl();
        std::pair<TinyVector<Decl*>, TinyVector<TypeInfo*>> ParseFunctionParams();
        Decl* ParseStructDecl();

        int ParseDeclarationFlags();

        Stmt* ParseGlobal();

        void SyncGlobal(); // Syncs the parser to a common sync point in a global context
        void SyncLocal(); // Syncs the parser to a common sync point in a local (block) context

        void ErrorExpected(const std::string& expect, SourceLocation loc, SourceRange range);

        bool ExprOk(Expr* expr);
        bool DeclOk(Decl* decl);

    private:
        size_t m_Index = 0;
        Tokens m_Tokens;

        bool m_DeclaredModule = false;

        using ParseExprFn = std::function<Expr*(Expr*)>;
        struct ParseExprRule {
            ParseExprFn Prefix = nullptr;
            ParseExprFn Infix = nullptr;
            size_t Precedence = 0;
        };
        std::unordered_map<TokenKind, ParseExprRule> m_ExprRules;

        CompilationContext* m_Context = nullptr;
    };

} // namespace Aria::Internal
