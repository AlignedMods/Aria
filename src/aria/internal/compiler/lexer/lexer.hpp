#pragma once

#include "aria/internal/compiler/lexer/tokens.hpp"
#include "aria/internal/compiler/compilation_context.hpp"

#include <string_view>

namespace Aria::Internal {

    class Lexer {
    public:
        Lexer(CompilationContext* ctx);

    private:
        void lex_impl();

        char peek(size_t count = 0);
        void backtrack(size_t count = 1);
        void consume(size_t count = 1);
        bool try_consume(char c);

        void parse_char_literal();
        void parse_decimal_literal();
        void parse_string_literal();

        void parse_at_symbol();
        void parse_dollar_symbol();

        void parse_identifier();

        void parse_single_line_comment();

        void skip_whitespace();

        char parse_char();

        void add_token(TokenKind kind, const SourceRange& range, std::string_view string);
        void add_token_with_integer(TokenKind kind, const SourceRange& range, u64 integer);
        void add_token_with_number(TokenKind kind, const SourceRange& range, f64 number);

        size_t get_column(size_t index);

    private:
        Tokens m_tokens;

        size_t m_index = 0;
        std::string_view m_source;

        size_t m_current_line = 1;
        size_t m_current_line_start = 0; // The number of characters it takes to get to this line (from the start of the file)
    
        CompilationContext* m_context = nullptr;
    };

} // namespace Aria::Internal
