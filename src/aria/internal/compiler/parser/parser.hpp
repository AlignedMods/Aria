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
        Token* TryConsume(TokenKind kind, const std::string& expect);

        // Checks if the current token matches with the requested type
        // This function cannot fail
        bool Match(TokenKind kind);

        BinaryOperatorKind ParseOperator();
        size_t GetBinaryPrecedence(BinaryOperatorKind kind);
        size_t GetNextPrecedence(BinaryOperatorKind binop);
        Expr* ParseValue();
        Expr* ParseExpression(size_t minbp = 0);

        bool IsPrimitiveType();
        bool IsType();
        TypeInfo* ParseType();

        Decl* ParseTypeDecl();
        Decl* ParseVariableDecl(TypeInfo* type, SourceLocation start);
        Decl* ParseFunctionDecl(TypeInfo* type, SourceLocation start);
        Decl* ParseStructDecl();

        Stmt* ParseBlock();
        Stmt* ParseBlockInline();
        Stmt* ParseWhile();
        Stmt* ParseDoWhile();
        Stmt* ParseFor();
        Stmt* ParseIf();

        Stmt* ParseBreak();
        Stmt* ParseContinue();
        Stmt* ParseReturn();

        Stmt* ParseStatement();

        Stmt* ParseToken();

        // Consumes tokens until it finds the first semi colon, closing curly, comma or EOF
        void StabilizeParser();

        void ErrorExpected(const std::string& expect, SourceLocation loc, SourceRange range);
        void ErrorTooLarge(const StringView value);

    private:
        size_t m_Index = 0;
        Tokens m_Tokens;

        std::unordered_map<std::string, Decl*> m_DeclaredTypes;
        CompilationContext* m_Context = nullptr;
    };

} // namespace Aria::Internal
