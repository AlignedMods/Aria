#include "ariac/lexer/lexer.hpp"
#include "ariac/core/scratch_buffer.hpp"

#include <charconv>
#include <limits>

namespace ariac {

    Lexer::Lexer() {
        m_source = context.active_comp_unit->source;

        lex_impl();
    }

    void Lexer::lex_impl() {
        while (true) {
            skip_whitespace();

            char c = peek();

            if (c == '\0') {
                context.active_comp_unit->tokens = m_tokens;
                return;
            }

            consume();

            switch (c) {
                case '\n': { m_current_line++; m_current_line_start = m_index - 1; break; }

                // Punctuation
                case ';': add_token(TokenKind::Semi, SourceLoc(m_current_line, get_column(m_index - 1), m_index - 1, 1), ";"); break;
                case '(': add_token(TokenKind::LeftParen, SourceLoc(m_current_line, get_column(m_index - 1), m_index - 1, 1), "("); break;
                case ')': add_token(TokenKind::RightParen, SourceLoc(m_current_line, get_column(m_index - 1), m_index - 1, 1), ")"); break;
                case '[': add_token(TokenKind::LeftBracket, SourceLoc(m_current_line, get_column(m_index - 1), m_index - 1, 1), "["); break;
                case ']': add_token(TokenKind::RightBracket, SourceLoc(m_current_line, get_column(m_index - 1), m_index - 1, 1), "]"); break;
                case '{': add_token(TokenKind::LeftCurly, SourceLoc(m_current_line, get_column(m_index - 1), m_index - 1, 1), "{"); break;
                case '}': add_token(TokenKind::RightCurly, SourceLoc(m_current_line, get_column(m_index - 1), m_index - 1, 1), "}"); break;
                case '~': add_token(TokenKind::Squigly, SourceLoc(m_current_line, get_column(m_index - 1), m_index - 1, 1), "~"); break;
                case ',': add_token(TokenKind::Comma, SourceLoc(m_current_line, get_column(m_index - 1), m_index - 1, 1), ","); break;
                case ':': {
                    if (try_consume(':')) { add_token(TokenKind::ColonColon, SourceLoc(m_current_line, get_column(m_index - 2), m_index - 2, 2), "::"); break; }
                    else { add_token(TokenKind::Colon, SourceLoc(m_current_line, get_column(m_index - 1), m_index - 1, 1), ":"); break; }
                }

                case '.': {
                    if (try_consume('.')) {
                        if (try_consume('.')) { add_token(TokenKind::TripleDot, SourceLoc(m_current_line, get_column(m_index - 3), m_index - 3, 3), "..."); break; }
                        else { add_token(TokenKind::DotDot, SourceLoc(m_current_line, get_column(m_index - 2), m_index - 2, 2), ".."); break; }
                    }

                    add_token(TokenKind::Dot, SourceLoc(m_current_line, get_column(m_index - 1), m_index - 1, 1), ".");
                    break;
                }

                // Operators
                case '+': {
                    if (try_consume('=')) { add_token(TokenKind::PlusEq, SourceLoc(m_current_line, get_column(m_index - 1), m_index - 1, 2), "+="); break; }
                    else if (try_consume('+')) { add_token(TokenKind::PlusPlus, SourceLoc(m_current_line, get_column(m_index - 1), m_index - 1, 2), "++"); break; }
                    else { add_token(TokenKind::Plus, SourceLoc(m_current_line, get_column(m_index - 1), m_index - 1, 1), "+"); break; }
                }

                case '-': {
                    if (try_consume('=')) { add_token(TokenKind::MinusEq, SourceLoc(m_current_line, get_column(m_index - 1), m_index - 1, 2), "-="); break; }
                    else if (try_consume('-')) { add_token(TokenKind::MinusMinus, SourceLoc(m_current_line, get_column(m_index - 1), m_index - 1, 2), "--"); break; }
                    else if (try_consume('>')) { add_token(TokenKind::Arrow, SourceLoc(m_current_line, get_column(m_index - 1), m_index - 1, 2), "->"); break; }
                    else { add_token(TokenKind::Minus, SourceLoc(m_current_line, get_column(m_index - 1), m_index - 1, 1), "-"); break; }
                }

                case '*': {
                    if (try_consume('=')) { add_token(TokenKind::StarEq, SourceLoc(m_current_line, get_column(m_index - 1), m_index - 1, 2), "*="); break; }
                    else { add_token(TokenKind::Star, SourceLoc(m_current_line, get_column(m_index - 1), m_index - 1, 1), "*"); break; }
                }

                case '/': {
                    if (try_consume('=')) { add_token(TokenKind::SlashEq, SourceLoc(m_current_line, get_column(m_index), m_index, 2), "/="); break; }
                    else { add_token(TokenKind::Slash, SourceLoc(m_current_line, get_column(m_index), m_index - 1, 1), "/"); break; }
                }

                case '%': {
                    if (try_consume('=')) { add_token(TokenKind::PercentEq, SourceLoc(m_current_line, get_column(m_index - 1), m_index - 1, 2), "%="); break; }
                    else { add_token(TokenKind::Percent, SourceLoc(m_current_line, get_column(m_index - 1), m_index - 1, 1), "%"); break; }
                }

                case '&': {
                    if (try_consume('=')) { add_token(TokenKind::AmpersandEq, SourceLoc(m_current_line, get_column(m_index - 2), m_index - 2, 2), "&="); break; }
                    else if (try_consume('&')) { add_token(TokenKind::DoubleAmpersand, SourceLoc(m_current_line, get_column(m_index - 2), m_index - 2, 2), "&&"); break; }
                    else { add_token(TokenKind::Ampersand, SourceLoc(m_current_line, get_column(m_index - 1), m_index - 1, 1), "&"); break; }
                }

                case '|': {
                    if (try_consume('=')) { add_token(TokenKind::PipeEq, SourceLoc(m_current_line, get_column(m_index - 1), m_index - 1, 2), "|="); break; }
                    else if (try_consume('|')) { add_token(TokenKind::DoublePipe, SourceLoc(m_current_line, get_column(m_index - 1), m_index - 1, 2), "||"); break; }
                    else { add_token(TokenKind::Pipe, SourceLoc(m_current_line, get_column(m_index - 1), m_index - 1, 1), "|"); break; }
                }

                case '^': {
                    if (try_consume('=')) { add_token(TokenKind::UpArrowEq, SourceLoc(m_current_line, get_column(m_index - 1), m_index - 1, 2), "^="); break; }
                    else { add_token(TokenKind::UpArrow, SourceLoc(m_current_line, get_column(m_index - 1), m_index - 1, 1), "^"); break; }
                }

                case '=': {
                    if (try_consume('=')) { add_token(TokenKind::EqEq, SourceLoc(m_current_line, get_column(m_index - 1), m_index - 1, 2), "=="); break; }
                    else { add_token(TokenKind::Eq, SourceLoc(m_current_line, get_column(m_index - 1), m_index - 1, 1), "="); break; }
                }

                case '!': {
                    if (try_consume('=')) { add_token(TokenKind::BangEq, SourceLoc(m_current_line, get_column(m_index - 1), m_index - 1, 2), "!="); break; }
                    else { add_token(TokenKind::Bang, SourceLoc(m_current_line, get_column(m_index - 1), m_index - 1, 1), "!"); break; }
                }

                case '<': {
                    if (try_consume('<')) {
                        if (try_consume('=')) { add_token(TokenKind::LessLessEq, SourceLoc(m_current_line, get_column(m_index - 1), m_index - 1, 3), "<<="); break; }
                        else { add_token(TokenKind::LessLess, SourceLoc(m_current_line, get_column(m_index - 1), m_index - 1, 2), "<<"); break; }
                    }

                    if (try_consume('=')) { add_token(TokenKind::LessEq, SourceLoc(m_current_line, get_column(m_index - 1), m_index - 1, 2), "<="); break; }
                    else { add_token(TokenKind::Less, SourceLoc(m_current_line, get_column(m_index - 1), m_index - 1, 1), "<"); break; }
                }

                case '>': {
                    if (try_consume('>')) {
                        if (try_consume('=')) { add_token(TokenKind::GreaterGreaterEq, SourceLoc(m_current_line, get_column(m_index - 1), m_index - 1, 3), ">>="); break; }
                        else { add_token(TokenKind::GreaterGreater, SourceLoc(m_current_line, get_column(m_index - 1), m_index - 1, 2), ">>"); break; }
                    }

                    if (try_consume('=')) { add_token(TokenKind::GreaterEq, SourceLoc(m_current_line, get_column(m_index - 1), m_index - 1, 2), ">="); break; }
                    else { add_token(TokenKind::Greater, SourceLoc(m_current_line, get_column(m_index - 1), m_index - 1, 1), ">"); break; }
                }

                case '#': {
                    backtrack();
                    parse_hash_symbol();
                    break;
                }

                case '@': {
                    backtrack();
                    parse_at_symbol();
                    break;
                }

                // Literals and constants
                case '\'': {
                    parse_char_literal();
                    break;
                }

                case '"': {
                    parse_string_literal();
                    break;
                }

                default: {
                    if (std::isdigit(c)) {
                        backtrack();
                        parse_decimal_literal();
                        break;
                    }

                    if (std::isalpha(c) || c == '_') {
                        backtrack();
                        parse_identifier();
                        break;
                    }

                    backtrack();
                    context.report_compiler_diagnostic(SourceLoc(m_current_line, get_column(m_index), m_index, 1), fmt::format("Unknown character '{}' ({:x})", c, c));
                    consume();
                    break;
                }
            }
        }
    }

    char Lexer::peek(size_t count) {
        if (m_index + count < m_source.length()) {
            return m_source.at(m_index + count);
        } else {
            return '\0';
        }
    }

    void Lexer::backtrack(size_t count) {
        ARIA_ASSERT(static_cast<ptrdiff_t>(m_index - count) >= 0, "Lexer::backtrack() out of bounds!");
        m_index -= count;
    }

    void Lexer::consume(size_t count) {
        ARIA_ASSERT(m_index + count <= m_source.length(), "Lexer::consume() out of bounds!");
        m_index += count;
    }

    bool Lexer::try_consume(char c) {
        if (peek() == c) {
            consume();
            return true;
        }

        return false;
    }

    void Lexer::parse_char_literal() {
        SourceLoc loc = SourceLoc(m_current_line, get_column(m_index), m_index);
        size_t start_index = m_index;

        if (try_consume('\'')) {
            loc.len = m_index - start_index;
            context.report_compiler_diagnostic(loc, "Empty character literals are not allowed");
            return;
        }

        char c = parse_char();
        loc.len = m_index - start_index;

        if (!try_consume('\'')) {
            context.report_compiler_diagnostic(loc, "Unterminated character literal");
            return;
        }

        add_token_with_integer(TokenKind::CharLit, loc, static_cast<u64>(c));
    }

    void Lexer::parse_decimal_literal() {
        bool encounteredPeriod = false;
                
        bool isHex = false;
        bool isBinary = false;
        bool isOctal = false;
        bool isDecimal = true;
        bool isUnsigned = false;

        size_t startIndex = m_index;

        if (peek(1)) {
            if (peek() == '0' && std::tolower(peek(1)) == 'x') {
                consume(2);
                isHex = true;
                isUnsigned = true;
            } else if (peek() == '0' && std::tolower(peek(1)) == 'b') {
                consume(2);
                isBinary = true;
                isUnsigned = true;
            } else if (peek() == '0' && std::tolower(peek(1)) == 'o') {
                consume(2);
                isOctal = true;
                isUnsigned = true;
            }
        }

        size_t numberStartIndex = m_index;
        isDecimal = !(isHex || isOctal || isBinary);

        bool errored = false;

        // Parse the actual integer
        while (peek()) {
            if (isDecimal) {
                if (std::isdigit(peek())) {
                    consume();
                } else if (peek() == '.' && !encounteredPeriod) {
                    consume();
                    encounteredPeriod = true;
                } else if (std::tolower(peek()) == 'a' || std::tolower(peek()) == 'b'
                        || std::tolower(peek()) == 'c' || std::tolower(peek()) == 'd'
                        || std::tolower(peek()) == 'e' || std::tolower(peek()) == 'f') {
                    context.report_compiler_diagnostic(SourceLoc(m_current_line, get_column(m_index), m_index, 1), fmt::format("Invalid hexadecimal digit '{}' in decimal literal", peek()));
                    consume();
                    errored = true;
                } else {
                    break;
                }
            } else if (isHex) {
                if (std::isdigit(peek()) || std::tolower(peek()) == 'a' || std::tolower(peek()) == 'b'
                                         || std::tolower(peek()) == 'c' || std::tolower(peek()) == 'd'
                                         || std::tolower(peek()) == 'e' || std::tolower(peek()) == 'f') {
                    consume();
                } else {
                    break;
                }
            } else if (isOctal) {
                if (peek() >= '0' && peek() <= '7') {
                    consume();
                } else if (peek() == '8' || peek() == '9') {
                    context.report_compiler_diagnostic(SourceLoc(m_current_line, get_column(m_index), m_index, 1), fmt::format("Invalid digit '{}' in octal literal", peek()));
                    consume();
                    errored = true;
                } else if (std::tolower(peek()) == 'a' || std::tolower(peek()) == 'b'
                        || std::tolower(peek()) == 'c' || std::tolower(peek()) == 'd'
                        || std::tolower(peek()) == 'e' || std::tolower(peek()) == 'f') {
                    context.report_compiler_diagnostic(SourceLoc(m_current_line, get_column(m_index), m_index, 1), fmt::format("Invalid hexadecimal digit '{}' in octal literal", peek()));
                    consume();
                    errored = true;
                } else {
                    break;
                }
            } else if (isBinary) {
                if (peek() == '0' || peek() == '1') {
                    consume();
                } else if (std::isdigit(peek())) {
                    context.report_compiler_diagnostic(SourceLoc(m_current_line, get_column(m_index), m_index, 1), fmt::format("Invalid digit '{}' in binary literal", peek()));
                    consume();
                    errored = true;
                } else if (std::tolower(peek()) == 'a' || std::tolower(peek()) == 'b'
                        || std::tolower(peek()) == 'c' || std::tolower(peek()) == 'd'
                        || std::tolower(peek()) == 'e' || std::tolower(peek()) == 'f') {
                    context.report_compiler_diagnostic(SourceLoc(m_current_line, get_column(m_index), m_index, 1), fmt::format("Invalid hexadecimal digit '{}' in binary literal", peek()));
                    consume();
                    errored = true;
                } else {
                    break;
                }
            }
        }

        std::string_view buf(m_source.data() + numberStartIndex, m_index - numberStartIndex);
        
        // Check for suffix
        if (peek()) {
            char suffix = std::tolower(peek());
            if (suffix == 'u' || suffix == 'i') {
                if (encounteredPeriod) {
                    context.report_compiler_diagnostic(SourceLoc(m_current_line, get_column(m_index), 1), fmt::format("Cannot use '{}' suffix in a floating-point literal", suffix));
                    consume();
                    errored = true;
                } else {
                    consume();

                    isUnsigned = (suffix == 'u') ? true : false;
                }
            }
        }

        if (encounteredPeriod) {
            double number = 0.0;
            if (!errored) {
                auto [ptr, ec] = std::from_chars(buf.data(), buf.data() + buf.length(), number); 

                if (ec == std::errc::result_out_of_range) {
                    context.report_compiler_diagnostic(SourceLoc(m_current_line, get_column(startIndex), startIndex, buf.length()), "Magnitude of floating-point literal is too large, maximum is 1.7976931348623157E+308");
                    number = 0.0;
                }
            }

            add_token_with_number(TokenKind::NumLit,
                SourceLoc(m_current_line, get_column(startIndex), startIndex, buf.length()),
                number);
        } else {
            u64 integer = 0;

            if (!errored) {
                int base = 10;
                if (isHex) { base = 16; }
                else if (isBinary) { base = 2; }
                else if (isOctal) { base = 8; }

                auto [ptr, ec] = std::from_chars(buf.data(), buf.data() + buf.length(), integer, base); 

                if (ec == std::errc::result_out_of_range) {
                    SourceLoc loc;
                    context.report_compiler_diagnostic(SourceLoc(m_current_line, get_column(startIndex), startIndex, buf.length()), "Integer literal is too large to fit into any integer type");
                    integer = 0;
                }
            }

            if (isUnsigned) {
                if (integer <= UINT32_MAX) {
                    add_token_with_integer(TokenKind::UIntLit,
                        SourceLoc(m_current_line, get_column(startIndex), startIndex, buf.length()),
                        integer);
                } else {
                    add_token_with_integer(TokenKind::ULongLit,
                        SourceLoc(m_current_line, get_column(startIndex), startIndex, buf.length()),
                        integer);
                }
            } else {
                if (integer <= INT32_MAX) {
                    add_token_with_integer(TokenKind::IntLit,
                        SourceLoc(m_current_line, get_column(startIndex), startIndex, buf.length()),
                        integer);
                } else {
                    add_token_with_integer(TokenKind::LongLit,
                        SourceLoc(m_current_line, get_column(startIndex), startIndex, buf.length()),
                        integer);
                }
            }
        }
    }

    void Lexer::parse_string_literal() {
        size_t start_index = m_index - 1;
        SourceLoc loc = SourceLoc(m_current_line, get_column(start_index), start_index, 1);

        scratch_buffer_clear();

        bool loop = true;
        while (loop) {
            switch (peek()) {
                case '"': {
                    loc += SourceLoc(m_current_line, get_column(m_index), m_index, 1);
                    loop = false;
                    consume();
                    break;
                }

                case '\n':
                case '\0': {
                    loop = false;
                    context.report_compiler_diagnostic(loc, "Unterminated string literal");
                    return;
                }

                default: {
                    scratch_buffer_append(parse_char());
                    break;
                }
            }
        }

        add_token(TokenKind::StrLit, loc, scratch_buffer_to_str());
    }

    void Lexer::parse_hash_symbol() {
        SourceLoc loc = SourceLoc(m_current_line, get_column(m_index), m_index, 1);
        scratch_buffer_clear();

        scratch_buffer_append('#');
        consume();

        while (true) {
            if (std::isalpha(peek())) {
                loc.len++;
                scratch_buffer_append(peek());
                consume();
            } else {
                break;
            }
        }

        if (scratch_buffer_cmp("#private")) { add_token(TokenKind::HashPrivate, loc, "#private"); return; }

        context.report_compiler_diagnostic(loc, "Unknown identifier following '#'");
    }

    void Lexer::parse_at_symbol() {
        SourceLoc loc = SourceLoc(m_current_line, get_column(m_index), m_index, 1);
        scratch_buffer_clear();

        scratch_buffer_append('@');
        consume();

        while (true) {
            if (std::isalpha(peek())) {
                loc.len++;
                scratch_buffer_append(peek());
                consume();
            } else {
                break;
            }
        }

        static std::unordered_map<std::string_view, TokenKind> kws = {
            { "@if", TokenKind::AtIf },
            { "@builtin", TokenKind::AtBuiltin },
            { "@sizeof", TokenKind::AtSizeof },
            { "@typeid", TokenKind::AtTypeid },
            { "@typeof", TokenKind::AtTypeof },
            { "@memcpy", TokenKind::AtMemcpy },
            { "@memset", TokenKind::AtMemset }
        };

        std::string_view str = scratch_buffer_to_str();
        if (kws.contains(str)) {
            add_token(kws.at(str), loc, str);
            return;
        }
        
        context.report_compiler_diagnostic(loc, "Unknown identifier following '@'");
    }

    void Lexer::parse_identifier() {
        SourceLoc loc = SourceLoc(m_current_line, get_column(m_index), m_index, 0);
        scratch_buffer_clear();

        while (true) {
            if (std::isalnum(peek()) || peek() == '_') {
                loc.len++;
                scratch_buffer_append(peek());
                consume();
            } else {
                break;
            }
        }

        static std::unordered_map<std::string_view, TokenKind> kws = {
            { "true", TokenKind::True },
            { "false", TokenKind::False },
            { "null", TokenKind::Null },

            { "module", TokenKind::Module },
            { "import", TokenKind::Import },
            { "env", TokenKind::Env },
            { "let", TokenKind::Let },
            { "if", TokenKind::If },
            { "else", TokenKind::Else },
            { "while", TokenKind::While },
            { "do", TokenKind::Do },
            { "for", TokenKind::For },
            { "break", TokenKind::Break },
            { "continue", TokenKind::Continue },
            { "return", TokenKind::Return },
            { "fn", TokenKind::Fn },
            { "struct", TokenKind::Struct },
            { "impl", TokenKind::Impl },
            { "defer", TokenKind::Defer },
            { "extern", TokenKind::Extern },
            { "static", TokenKind::Static },
            { "typedef", TokenKind::Typedef },
            { "enum", TokenKind::Enum },
            { "as", TokenKind::As },
            { "const", TokenKind::Const },
            { "cast", TokenKind::Cast },
            { "self", TokenKind::Self },

            { "void", TokenKind::Void },
            { "bool", TokenKind::Bool },
            { "char", TokenKind::Char },
            { "ichar", TokenKind::IChar },
            { "short", TokenKind::Short },
            { "ushort", TokenKind::UShort },
            { "int", TokenKind::Int },
            { "uint", TokenKind::UInt },
            { "long", TokenKind::Long },
            { "ulong", TokenKind::ULong },
            { "sz", TokenKind::Sz },
            { "isz", TokenKind::Isz },
            { "float", TokenKind::Float },
            { "double", TokenKind::Double },
            { "typeinfo", TokenKind::TypeInfo },
            { "any", TokenKind::Any }
        };

        std::string_view str = scratch_buffer_to_str();
        if (kws.contains(str)) {
            add_token(kws.at(str), loc, str);
            return;
        }

        add_token(TokenKind::Identifier, loc, str);
    }

    void Lexer::parse_single_line_comment() {
        consume(2); // consume '//' 

        while (true) {
            switch (peek()) {
                case '\n': {
                    m_current_line++;
                    m_current_line_start = m_index;

                    consume(); 
                    return;
                }
                case '\0': return;

                default: consume(); break;
            }
        }
    }

    void Lexer::skip_whitespace() {
        while (true) {
            switch (peek()) {
                case '/': { // Potentially a comment
                    if (peek(1) == '/') {
                        parse_single_line_comment();
                        break;
                    }

                    return;
                }

                case '\n': {
                    m_current_line++;
                    m_current_line_start = m_index;

                    consume();
                    break;
                }

                case '\r':
                case ' ':
                case '\t': {
                    consume();
                    break;
                }

                default: return;
            }
        }
    }

    char Lexer::parse_char() {
        if (peek() == '\\') {
            consume();
            switch (peek()) {
                case '\\': consume(); return '\\';
                case '0': consume(); return '\0';
                case 'n': consume(); return '\n';
                case 't': consume(); return '\t';
                case 's': consume(); return ' ';
                case 'r': consume(); return '\r';
                case '"': consume(); return '"';
                case '\'': consume(); return '\'';

                default: {
                    context.report_compiler_diagnostic(SourceLoc(m_current_line, get_column(m_index), m_index, 2), "Unknown escape sequence");
                    return '\\';
                }
            }
        } else {
            consume();
            return peek(-1);
        }

        ARIA_UNREACHABLE("Should never be reached");
    }

    void Lexer::add_token(TokenKind kind, const SourceLoc& loc, std::string_view string) {
        Token token;
        token.kind = kind;
        token.loc = loc;
        token.string = string;

        m_tokens.push_back(token);
    }

    void Lexer::add_token_with_integer(TokenKind kind, const SourceLoc& loc, u64 integer) {
        Token token;
        token.kind = kind;
        token.loc = loc;
        token.integer = integer;

        m_tokens.push_back(token);
    }

    void Lexer::add_token_with_number(TokenKind kind, const SourceLoc& loc, double number) {
        Token token;
        token.kind = kind;
        token.loc = loc;
        token.number = number;

        m_tokens.push_back(token);
    }

    size_t Lexer::get_column(size_t index) {
        return (m_current_line_start == 0) ? (index - m_current_line_start) + 1 : index - m_current_line_start;
    }

} // namespace ariac
