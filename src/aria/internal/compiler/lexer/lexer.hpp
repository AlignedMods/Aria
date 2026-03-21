#pragma once

#include "aria/internal/compiler/lexer/tokens.hpp"
#include "aria/internal/compiler/compilation_context.hpp"

namespace Aria::Internal {

    class Lexer {
    public:
        Lexer(CompilationContext* ctx);

    private:
        void LexImpl();

        const char* Peek(size_t count = 0);
        char Consume();

        void AddToken(TokenType type, const SourceRange& loc, const StringView string = {}, u64 integer = 0, f64 number = 0.0);

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
