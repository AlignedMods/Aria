#pragma once

#include "aria/internal/compiler/lexer/tokens.hpp"
#include "aria/internal/compiler/compilation_context.hpp"

namespace Aria::Internal {

    class Lexer {
    public:
        Lexer(CompilationContext* ctx);

    private:
        void LexImpl();

        char Peek(size_t count = 0);
        void Backtrack(size_t count = 1);
        void Consume(size_t count = 1);
        bool TryConsume(char c);

        void ParseCharLiteral();
        void ParseDecimalLiteral();
        void ParseStringLiteral();

        void ParseIdentifier();

        void ParseSingleLineComment();

        void SkipWhitespace();

        void AddToken(TokenKind kind, const SourceRange& range, StringView string);
        void AddTokenWithInteger(TokenKind kind, const SourceRange& range, u64 integer);
        void AddTokenWithNumber(TokenKind kind, const SourceRange& range, f64 number);

        size_t GetColumn(size_t index);

    private:
        Tokens m_Tokens;

        size_t m_Index = 0;
        StringView m_Source;

        size_t m_CurrentLine = 1;
        size_t m_CurrentLineStart = 0; // The number of characters it takes to get to this line (from the start of the file)
    
        CompilationContext* m_Context = nullptr;
    };

} // namespace Aria::Internal
