#pragma once

#include "black_lua/internal/compiler/lexer/tokens.hpp"
#include "black_lua/internal/compiler/compilation_context.hpp"

namespace BlackLua::Internal {

    class Lexer {
    public:
        Lexer(CompilationContext* ctx);

    private:
        void LexImpl();

        const char* Peek();
        char Consume();

        void AddToken(TokenType type, const SourceRange& loc, const StringView data = {});

        size_t GetColumn(size_t index);

    private:
        Tokens m_Tokens;

        size_t m_Index = 0;
        StringView m_Source;

        size_t m_CurrentLine = 1;
        size_t m_CurrentLineStart = 0; // The number of characters it takes to get to this line (from the start of the file)
    
        CompilationContext* m_Context = nullptr;
    };

} // namespace BlackLua::Internal
