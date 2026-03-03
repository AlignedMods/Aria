#pragma once

#include "black_lua/internal/compiler/lexer/tokens.hpp"
#include "black_lua/internal/compiler/compilation_context.hpp"
#include "black_lua/internal/compiler/core/string_builder.hpp"
#include "black_lua/internal/compiler/ast/expr.hpp"
#include "black_lua/internal/compiler/ast/stmt.hpp"
#include "black_lua/internal/compiler/ast/decl.hpp"

#include <unordered_map>

namespace BlackLua::Internal {

    class Parser {
    public:
        Parser(CompilationContext* ctx);

    private:
        void ParseImpl();

        Token* Peek(size_t count = 0);
        Token& Consume();
        Token* TryConsume(TokenType type, StringView error);

        // Checks if the current token matches with the requested type
        // This function cannot fail
        bool Match(TokenType type);

        StringBuilder ParseVariableType();
        TinyVector<ParamDecl*> ParseFunctionParameters();
        bool IsPrimitiveType();
        bool IsVariableType();

        BinaryOperatorType ParseOperator();
        size_t GetBinaryPrecedence(BinaryOperatorType type);
        Expr* ParseValue();
        Expr* ParseExpression(size_t minbp = 0);

        Stmt* ParseCompound();
        Stmt* ParseCompoundInline();

        Stmt* ParseType(bool external = false);

        Stmt* ParseVariableDecl(StringBuilder type, SourceRange start);
        Stmt* ParseFunctionDecl(StringBuilder returnType, SourceRange start, bool external = false);
        Stmt* ParseExtern();

        Stmt* ParseStructDecl();

        Stmt* ParseWhile();
        Stmt* ParseDoWhile();
        Stmt* ParseFor();

        Stmt* ParseIf();

        Stmt* ParseBreak();
        Stmt* ParseContinue();
        Stmt* ParseReturn();

        Stmt* ParseStatement();

        Stmt* ParseToken();

        void ErrorExpected(const StringView msg);
        void ErrorTooLarge(const StringView value);

    private:
        size_t m_Index = 0;
        Tokens m_Tokens;

        bool m_NeedsSemi = true; // A flag to see if the current statement needs to finish with a semicolon

        std::unordered_map<std::string, bool> m_DeclaredTypes;

        CompilationContext* m_Context = nullptr;
    };

} // namespace BlackLua::Internal
