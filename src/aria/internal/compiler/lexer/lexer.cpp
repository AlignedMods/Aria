#include "aria/internal/compiler/lexer/lexer.hpp"
#include "aria/internal/compiler/core/scratch_buffer.hpp"

#include <charconv>
#include <limits>

namespace Aria::Internal {

    Lexer::Lexer(CompilationContext* ctx) {
        m_context = ctx;
        m_source = ctx->active_comp_unit->source;

        lex_impl();
    }

    void Lexer::lex_impl() {
        while (true) {
            skip_whitespace();

            SourceLocation start = SourceLocation(m_current_line, get_column(m_index));
            char c = peek();

            if (c == '\0') {
                m_context->active_comp_unit->tokens = m_tokens;
                return;
            }

            consume();

            switch (c) {
                case '\n': { m_current_line++; m_current_line_start = m_index - 1; break; }

                // Punctuation
                case ';': add_token(TokenKind::Semi, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), ";");
                          break;
                case '(': add_token(TokenKind::LeftParen, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "(");
                          break;
                case ')': add_token(TokenKind::RightParen, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), ")");
                          break;
                case '[': add_token(TokenKind::LeftBracket, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "[");
                          break;
                case ']': add_token(TokenKind::RightBracket, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "]");
                          break;
                case '{': add_token(TokenKind::LeftCurly, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "{");
                          break;
                case '}': add_token(TokenKind::RightCurly, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "}");
                          break;
                case '~': add_token(TokenKind::Squigly, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "~");
                          break;
                case ',': add_token(TokenKind::Comma, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), ",");
                          break;
                case ':': {
                    if (try_consume(':')) { add_token(TokenKind::ColonColon, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "::"); break; }
                    else { add_token(TokenKind::Colon, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), ":"); break; }
                }
                case '.': add_token(TokenKind::Dot, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), ".");
                          break;

                // Operators
                case '+': {
                    if (try_consume('=')) { add_token(TokenKind::PlusEq, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "+="); break; }
                    else { add_token(TokenKind::Plus, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "+"); break; }
                }
                case '-': {
                    if (try_consume('=')) { add_token(TokenKind::MinusEq, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "-="); break; }
                    else if (try_consume('>')) { add_token(TokenKind::Arrow, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "->"); break; }
                    else { add_token(TokenKind::Minus, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "-"); break; }
                }
                case '*': {
                    if (try_consume('=')) { add_token(TokenKind::StarEq, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "*="); break; }
                    else { add_token(TokenKind::Star, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "*"); break; }
                }
                case '/': {
                    if (try_consume('=')) { add_token(TokenKind::SlashEq, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "/="); break; }
                    else { add_token(TokenKind::Slash, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "/"); break; }
                }
                case '%': {
                    if (try_consume('=')) { add_token(TokenKind::PercentEq, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "%="); break; }
                    else { add_token(TokenKind::Percent, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "%"); break; }
                }
                case '&': {
                    if (try_consume('=')) { add_token(TokenKind::AmpersandEq, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "&="); break; }
                    else if (try_consume('&')) { add_token(TokenKind::DoubleAmpersand, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "&&"); break; }
                    else { add_token(TokenKind::Ampersand, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "&"); break; }
                }
                case '|': {
                    if (try_consume('=')) { add_token(TokenKind::PipeEq, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "|="); break; }
                    else if (try_consume('|')) { add_token(TokenKind::DoublePipe, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "||"); break; }
                    else { add_token(TokenKind::Pipe, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "|"); break; }
                }
                case '^': {
                    if (try_consume('=')) { add_token(TokenKind::UpArrowEq, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "^="); break; }
                    else { add_token(TokenKind::UpArrow, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "^"); break; }
                }
                case '=': {
                    if (try_consume('=')) { add_token(TokenKind::EqEq, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "=="); break; }
                    else { add_token(TokenKind::Eq, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "="); break; }
                }
                case '!': {
                    if (try_consume('=')) { add_token(TokenKind::BangEq, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "!="); break; }
                    else { add_token(TokenKind::Bang, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "!"); break; }
                }
                case '<': {
                    if (try_consume('<')) {
                        if (try_consume('=')) { add_token(TokenKind::LessLessEq, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "<<="); break; }
                        else { add_token(TokenKind::LessLess, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "<<"); break; }
                    }

                    if (try_consume('=')) { add_token(TokenKind::LessEq, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "<="); break; }
                    else { add_token(TokenKind::Less, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "<"); break; }
                }
                case '>': {
                    if (try_consume('>')) {
                        if (try_consume('=')) { add_token(TokenKind::GreaterGreaterEq, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), ">>="); break; }
                        else { add_token(TokenKind::GreaterGreater, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), ">>"); break; }
                    }

                    if (try_consume('=')) { add_token(TokenKind::GreaterEq, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), ">="); break; }
                    else { add_token(TokenKind::Greater, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), ">"); break; }
                }

                case '@': {
                    backtrack();
                    parse_at_symbol();
                    break;
                }

                case '$': {
                    backtrack();
                    parse_dollar_symbol();
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

                    fmt::print("Unknown character: {} ({:x})\n", c, c);

                    ARIA_UNREACHABLE();
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
        SourceLocation start = SourceLocation(m_current_line, get_column(m_index));

        if (try_consume('\'')) {
            SourceLocation loc = SourceLocation(m_current_line, get_column(m_index));
            m_context->report_compiler_diagnostic(loc, SourceRange(start, loc), "Empty character literal");
            return;
        }

        char c = parse_char();

        if (!try_consume('\'')) {
            SourceLocation loc = SourceLocation(m_current_line, get_column(m_index));
            m_context->report_compiler_diagnostic(loc, SourceRange(start, loc), "Unterminated character literal");
            return;
        }

        SourceLocation loc = SourceLocation(m_current_line, get_column(m_index));
        add_token_with_integer(TokenKind::CharLit, SourceRange(start, loc), static_cast<u64>(c));
    }

    void Lexer::parse_decimal_literal() {
        bool encounteredPeriod = false;
                
        bool isHex = false;
        bool isBinary = false;
        bool isOctal = false;
        bool isDecimal = true;
        bool isUnsigned = false;

        if (peek(1)) {
            if (peek() == '0' && std::tolower(peek(1)) == 'x') {
                consume(2);
                isHex = true;
            } else if (peek() == '0' && std::tolower(peek(1)) == 'b') {
                consume(2);
                isBinary = true;
            } else if (peek() == '0' && std::tolower(peek(1)) == 'o') {
                consume(2);
                isOctal = true;
            }
        }

        isDecimal = !(isHex || isOctal || isBinary);

        size_t startIndex = m_index;
        size_t numberStartIndex = m_index;

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
                    SourceLocation loc = SourceLocation(m_current_line, get_column(m_index));
                    m_context->report_compiler_diagnostic(loc, SourceRange(loc, loc), fmt::format("invalid hexadecimal digit '{}' in decimal literal", peek()));
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
                if (peek() >= '0' && peek() <= '8') {
                    consume();
                } else if (peek() == '9') {
                    SourceLocation loc = SourceLocation(m_current_line, get_column(m_index));
                    m_context->report_compiler_diagnostic(loc, SourceRange(loc, loc), "invalid digit '9' in octal literal");
                    consume();
                    errored = true;
                } else if (std::tolower(peek()) == 'a' || std::tolower(peek()) == 'b'
                        || std::tolower(peek()) == 'c' || std::tolower(peek()) == 'd'
                        || std::tolower(peek()) == 'e' || std::tolower(peek()) == 'f') {
                    SourceLocation loc = SourceLocation(m_current_line, get_column(m_index));
                    m_context->report_compiler_diagnostic(loc, SourceRange(loc, loc), fmt::format("invalid hexadecimal digit '{}' in octal literal", peek()));
                    consume();
                    errored = true;
                } else {
                    break;
                }
            } else if (isBinary) {
                if (peek() == '0' || peek() == '1') {
                    consume();
                } else if (std::isdigit(peek())) {
                    SourceLocation loc = SourceLocation(m_current_line, get_column(m_index));
                    m_context->report_compiler_diagnostic(loc, SourceRange(loc, loc), fmt::format("invalid digit '{}' in binary literal", peek()));
                    consume();
                    errored = true;
                } else if (std::tolower(peek()) == 'a' || std::tolower(peek()) == 'b'
                        || std::tolower(peek()) == 'c' || std::tolower(peek()) == 'd'
                        || std::tolower(peek()) == 'e' || std::tolower(peek()) == 'f') {
                    SourceLocation loc = SourceLocation(m_current_line, get_column(m_index));
                    m_context->report_compiler_diagnostic(loc, SourceRange(loc, loc), fmt::format("invalid hexadecimal digit '{}' in binary literal", peek()));
                    consume();
                    errored = true;
                } else {
                    break;
                }
            }
        }

        std::string_view buf(m_source.data() + startIndex, m_index - startIndex);
        
        // Check for unsigned
        if (peek() && std::tolower(peek()) == 'u') {
            if (encounteredPeriod) {
                SourceLocation loc = SourceLocation(m_current_line, get_column(m_index));
                m_context->report_compiler_diagnostic(loc, SourceRange(loc, loc), fmt::format("cannot use 'u' suffix in a floating-point literal"));
                consume();
                errored = true;
            } else {
                consume();
                isUnsigned = true;
            }
        }

        if (encounteredPeriod) {
            f64 number = 0.0;
            if (!errored) {
                auto [ptr, ec] = std::from_chars(buf.data(), buf.data() + buf.length(), number); 

                if (ec == std::errc::result_out_of_range) {
                    SourceLocation loc(m_current_line, get_column(m_index - buf.length()));
                    m_context->report_compiler_diagnostic(loc, SourceRange(loc, SourceLocation(m_current_line, get_column(m_index))), 
                                                   "magnitude of floating-point literal is too large, maximum is 1.7976931348623157E+308");
                    number = 0.0;
                }
            }

            add_token_with_number(TokenKind::NumLit,
                SourceRange(m_current_line, get_column(startIndex), m_current_line, get_column(m_index)),
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
                    SourceLocation loc(m_current_line, get_column(m_index - buf.length()));
                    m_context->report_compiler_diagnostic(loc, SourceRange(loc, SourceLocation(m_current_line, get_column(m_index))), 
                                                   "integer literal is too large to fit into any integer type");
                    integer = 0;
                }
            }

            if (isUnsigned) {
                if (integer <= UINT32_MAX) {
                    add_token_with_integer(TokenKind::UIntLit,
                        SourceRange(m_current_line, get_column(m_index - buf.length()), m_current_line, get_column(m_index)),
                        integer);
                } else {
                    add_token_with_integer(TokenKind::ULongLit,
                        SourceRange(m_current_line, get_column(m_index - buf.length()), m_current_line, get_column(m_index)),
                        integer);
                }
            } else {
                if (integer <= INT32_MAX) {
                    add_token_with_integer(TokenKind::IntLit,
                        SourceRange(m_current_line, get_column(m_index - buf.length()), m_current_line, get_column(m_index)),
                        integer);
                } else {
                    add_token_with_integer(TokenKind::LongLit,
                        SourceRange(m_current_line, get_column(m_index - buf.length()), m_current_line, get_column(m_index)),
                        integer);
                }
            }
        }
    }

    void Lexer::parse_string_literal() {
        SourceLocation start = SourceLocation(m_current_line, get_column(m_index - 1));
        SourceLocation end;

        scratch_buffer_clear();

        while (!end.is_valid()) {
            switch (peek()) {
                case '"': {
                    consume();
                    end = SourceLocation(m_current_line, get_column(m_index));
                    break;
                }

                case '\n':
                case '\0': {
                    end = SourceLocation(m_current_line, get_column(m_index));
                    m_context->report_compiler_diagnostic(start, SourceRange(start, end), "Unterminated string literal");
                    return;
                }

                default: {
                    scratch_buffer_append(parse_char());
                    break;
                }
            }
        }

        add_token(TokenKind::StrLit, SourceRange(start, end), scratch_buffer_to_str(m_context));
    }

    void Lexer::parse_at_symbol() {
        scratch_buffer_clear();
        SourceLocation start = SourceLocation(m_current_line, get_column(m_index));

        scratch_buffer_append('@');
        consume();

        while (true) {
            if (std::isalpha(peek())) {
                scratch_buffer_append(peek());
                consume();
            } else {
                break;
            }
        }

        if (scratch_buffer_cmp("@extern")) { add_token(TokenKind::AtExtern, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "@extern"); return; }
        else if (scratch_buffer_cmp("@nomangle")) { add_token(TokenKind::AtNoMangle, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "@nomangle"); return; }
        else if (scratch_buffer_cmp("@unsafe")) { add_token(TokenKind::AtUnsafe, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "@unsafe"); return; }
        else if (scratch_buffer_cmp("@private")) { add_token(TokenKind::AtPrivate, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "@private"); return; }

        m_context->report_compiler_diagnostic(start, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "Unknown attribute starting with '@'");
    }

    void Lexer::parse_dollar_symbol() {
        scratch_buffer_clear();
        SourceLocation start = SourceLocation(m_current_line, get_column(m_index));

        scratch_buffer_append('$');
        consume();

        while (true) {
            if (std::isalpha(peek())) {
                scratch_buffer_append(peek());
                consume();
            } else {
                break;
            }
        }

        if (scratch_buffer_cmp("$format")) { add_token(TokenKind::DollarFormat, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "$format"); return; }
        m_context->report_compiler_diagnostic(start, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "Unknown identifier following '$'");
    }

    void Lexer::parse_identifier() {
        scratch_buffer_clear();
        SourceLocation start = SourceLocation(m_current_line, get_column(m_index));

        while (true) {
            if (std::isalnum(peek()) || peek() == '_') {
                scratch_buffer_append(peek());
                consume();
            } else {
                break;
            }
        }

        if (scratch_buffer_cmp("true"))     { add_token(TokenKind::True,   SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "true");   return; }
        if (scratch_buffer_cmp("false"))    { add_token(TokenKind::False,  SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "false");  return; }
        if (scratch_buffer_cmp("null"))     { add_token(TokenKind::Null,  SourceRange(start, SourceLocation(m_current_line, get_column(m_index))),  "null");  return; }

        if (scratch_buffer_cmp("module"))   { add_token(TokenKind::Module, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "module"); return; }
        if (scratch_buffer_cmp("import"))   { add_token(TokenKind::Import, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "import"); return; }
        if (scratch_buffer_cmp("let"))      { add_token(TokenKind::Let,    SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "let");    return; }
        if (scratch_buffer_cmp("if"))       { add_token(TokenKind::If,     SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "if");     return; }
        if (scratch_buffer_cmp("else"))     { add_token(TokenKind::Else,   SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "else");   return; }
        if (scratch_buffer_cmp("while"))    { add_token(TokenKind::While,  SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "while");  return; }
        if (scratch_buffer_cmp("do"))       { add_token(TokenKind::Do,     SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "do");     return; }
        if (scratch_buffer_cmp("for"))      { add_token(TokenKind::For,    SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "for");    return; }
        if (scratch_buffer_cmp("break"))    { add_token(TokenKind::Break,  SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "break");  return; }
        if (scratch_buffer_cmp("continue")) { add_token(TokenKind::Continue,  SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "continue");  return; }
        if (scratch_buffer_cmp("return"))   { add_token(TokenKind::Return, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "return"); return; }
        if (scratch_buffer_cmp("self"))     { add_token(TokenKind::Self,   SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "self");   return; }
        if (scratch_buffer_cmp("fn"))       { add_token(TokenKind::Fn,     SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "fn");     return; }
        if (scratch_buffer_cmp("struct"))   { add_token(TokenKind::Struct, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "struct"); return; }
        if (scratch_buffer_cmp("new"))      { add_token(TokenKind::New,    SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "new");    return; }
        if (scratch_buffer_cmp("delete"))   { add_token(TokenKind::Delete, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "delete"); return; }
        if (scratch_buffer_cmp("unsafe"))   { add_token(TokenKind::Unsafe, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "unsafe"); return; }

        if (scratch_buffer_cmp("void"))     { add_token(TokenKind::Void,   SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "void");   return; }
        if (scratch_buffer_cmp("bool"))     { add_token(TokenKind::Bool,   SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "bool");   return; }
        if (scratch_buffer_cmp("char"))     { add_token(TokenKind::Char,   SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "char");   return; }
        if (scratch_buffer_cmp("uchar"))    { add_token(TokenKind::UChar,  SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "uchar");  return; }
        if (scratch_buffer_cmp("short"))    { add_token(TokenKind::Short,  SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "short");  return; }
        if (scratch_buffer_cmp("ushort"))   { add_token(TokenKind::UShort, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "ushort"); return; }
        if (scratch_buffer_cmp("int"))      { add_token(TokenKind::Int,    SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "int");    return; }
        if (scratch_buffer_cmp("uint"))     { add_token(TokenKind::UInt,   SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "uint");   return; }
        if (scratch_buffer_cmp("long"))     { add_token(TokenKind::Long,   SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "long");   return; }
        if (scratch_buffer_cmp("ulong"))    { add_token(TokenKind::ULong,  SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "ulong");  return; }
        if (scratch_buffer_cmp("float"))    { add_token(TokenKind::Float,  SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "float");  return; }
        if (scratch_buffer_cmp("double"))   { add_token(TokenKind::Double, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "double"); return; }
        if (scratch_buffer_cmp("string"))   { add_token(TokenKind::String, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), "string"); return; }

        add_token(TokenKind::Identifier, SourceRange(start, SourceLocation(m_current_line, get_column(m_index))), scratch_buffer_to_str(m_context));
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
                    SourceLocation loc = SourceLocation(m_current_line, get_column(m_index));
                    m_context->report_compiler_diagnostic(loc, SourceRange(loc, loc), "Unknown escape sequence");
                    return '\\';
                }
            }
        } else {
            consume();
            return peek(-1);
        }

        ARIA_UNREACHABLE();
    }

    void Lexer::add_token(TokenKind kind, const SourceRange& range, std::string_view string) {
        Token token;
        token.kind = kind;
        token.range = range;
        token.string = string;

        m_tokens.push_back(token);
    }

    void Lexer::add_token_with_integer(TokenKind kind, const SourceRange& range, u64 integer) {
        Token token;
        token.kind = kind;
        token.range = range;
        token.integer = integer;

        m_tokens.push_back(token);
    }

    void Lexer::add_token_with_number(TokenKind kind, const SourceRange& range, f64 number) {
        Token token;
        token.kind = kind;
        token.range = range;
        token.number = number;

        m_tokens.push_back(token);
    }

    size_t Lexer::get_column(size_t index) {
        return (m_current_line_start == 0) ? (index - m_current_line_start) + 1 : index - m_current_line_start;
    }

} // namespace Aria::Internal
