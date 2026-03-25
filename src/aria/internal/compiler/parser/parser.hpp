#pragma once

#include "aria/internal/compiler/lexer/tokens.hpp"
#include "aria/internal/compiler/compilation_context.hpp"
#include "aria/internal/compiler/core/string_builder.hpp"
#include "aria/internal/compiler/ast/expr.hpp"
#include "aria/internal/compiler/ast/stmt.hpp"
#include "aria/internal/compiler/ast/decl.hpp"

#include <unordered_map>

namespace Aria::Internal {

    class Parser {
    public:
        Parser(CompilationContext* ctx);

    private:
        void ParseImpl();

        Token* Peek(size_t count = 0);
        Token& Consume();
        Token* TryConsume(TokenType type, const std::string& expect);

        // Checks if the current token matches with the requested type
        // This function cannot fail
        bool Match(TokenType type);

        StringBuilder ParseVariableType();
        TinyVector<ParamDecl*> ParseFunctionParameters();
        bool IsPrimitiveType();
        bool IsVariableType();

        BinaryOperatorType ParseOperator();
        size_t GetBinaryPrecedence(BinaryOperatorType type);
        size_t GetNextPrecedence(BinaryOperatorType binop);
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

        // Consumes tokens until it finds the first semi colon, closing curly or EOF
        void StabilizeParser();

        void ErrorExpected(const std::string& expect, SourceLocation loc, SourceRange range);
        void ErrorTooLarge(const StringView value);

    private:
        size_t m_Index = 0;
        Tokens m_Tokens;

        bool m_NeedsSemi = true; // A flag to see if the current statement needs to finish with a semicolon

        std::unordered_map<std::string, bool> m_DeclaredTypes;

        CompilationContext* m_Context = nullptr;
    };

} // namespace Aria::Internal
