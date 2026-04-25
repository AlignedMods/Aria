#include "aria/internal/compiler/lexer/lexer.hpp"
#include "aria/internal/compiler/core/string_builder.hpp"

#include <charconv>
#include <limits>

namespace Aria::Internal {

    Lexer::Lexer(CompilationContext* ctx) {
        m_Context = ctx;
        m_Source = ctx->ActiveCompUnit->Source;

        LexImpl();
    }

    void Lexer::LexImpl() {
        while (true) {
            SkipWhitespace();

            SourceLocation start = SourceLocation(m_CurrentLine, GetColumn(m_Index));
            char c = Peek();

            if (c == '\0') {
                m_Context->ActiveCompUnit->Tokens = m_Tokens;
                return;
            }

            Consume();

            switch (c) {
                case '\n': { m_CurrentLine++; m_CurrentLineStart = m_Index - 1; break; }

                // Punctuation
                case ';': AddToken(TokenKind::Semi, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), ";");
                          break;
                case '(': AddToken(TokenKind::LeftParen, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "(");
                          break;
                case ')': AddToken(TokenKind::RightParen, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), ")");
                          break;
                case '[': AddToken(TokenKind::LeftBracket, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "[");
                          break;
                case ']': AddToken(TokenKind::RightBracket, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "]");
                          break;
                case '{': AddToken(TokenKind::LeftCurly, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "{");
                          break;
                case '}': AddToken(TokenKind::RightCurly, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "}");
                          break;
                case '~': AddToken(TokenKind::Squigly, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "~");
                          break;
                case ',': AddToken(TokenKind::Comma, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), ",");
                          break;
                case ':': {
                    if (TryConsume(':')) { AddToken(TokenKind::ColonColon, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "::"); break; }
                    else { AddToken(TokenKind::Colon, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), ":"); break; }
                }
                case '.': AddToken(TokenKind::Dot, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), ".");
                          break;

                // Operators
                case '+': {
                    if (TryConsume('=')) { AddToken(TokenKind::PlusEq, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "+="); break; }
                    else { AddToken(TokenKind::Plus, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "+"); break; }
                }
                case '-': {
                    if (TryConsume('=')) { AddToken(TokenKind::MinusEq, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "-="); break; }
                    else if (TryConsume('>')) { AddToken(TokenKind::Arrow, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "->"); break; }
                    else { AddToken(TokenKind::Minus, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "-"); break; }
                }
                case '*': {
                    if (TryConsume('=')) { AddToken(TokenKind::StarEq, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "*="); break; }
                    else { AddToken(TokenKind::Star, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "*"); break; }
                }
                case '/': {
                    if (TryConsume('=')) { AddToken(TokenKind::SlashEq, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "/="); break; }
                    else { AddToken(TokenKind::Slash, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "/"); break; }
                }
                case '%': {
                    if (TryConsume('=')) { AddToken(TokenKind::PercentEq, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "%="); break; }
                    else { AddToken(TokenKind::Percent, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "%"); break; }
                }
                case '&': {
                    if (TryConsume('=')) { AddToken(TokenKind::AmpersandEq, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "&="); break; }
                    else if (TryConsume('&')) { AddToken(TokenKind::DoubleAmpersand, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "&&"); break; }
                    else { AddToken(TokenKind::Ampersand, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "&"); break; }
                }
                case '|': {
                    if (TryConsume('=')) { AddToken(TokenKind::PipeEq, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "|="); break; }
                    else if (TryConsume('|')) { AddToken(TokenKind::DoublePipe, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "||"); break; }
                    else { AddToken(TokenKind::Pipe, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "|"); break; }
                }
                case '^': {
                    if (TryConsume('=')) { AddToken(TokenKind::UpArrowEq, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "^="); break; }
                    else { AddToken(TokenKind::UpArrow, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "^"); break; }
                }
                case '=': {
                    if (TryConsume('=')) { AddToken(TokenKind::EqEq, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "=="); break; }
                    else { AddToken(TokenKind::Eq, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "="); break; }
                }
                case '!': {
                    if (TryConsume('=')) { AddToken(TokenKind::BangEq, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "!="); break; }
                    else { AddToken(TokenKind::Bang, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "!"); break; }
                }
                case '<': {
                    if (TryConsume('<')) {
                        if (TryConsume('=')) { AddToken(TokenKind::LessLessEq, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "<<="); break; }
                        else { AddToken(TokenKind::LessLess, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "<<"); break; }
                    }

                    if (TryConsume('=')) { AddToken(TokenKind::LessEq, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "<="); break; }
                    else { AddToken(TokenKind::Less, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "<"); break; }
                }
                case '>': {
                    if (TryConsume('>')) {
                        if (TryConsume('=')) { AddToken(TokenKind::GreaterGreaterEq, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), ">>="); break; }
                        else { AddToken(TokenKind::GreaterGreater, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), ">>"); break; }
                    }

                    if (TryConsume('=')) { AddToken(TokenKind::GreaterEq, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), ">="); break; }
                    else { AddToken(TokenKind::Greater, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), ">"); break; }
                }

                case '@': {
                    Backtrack();
                    ParseAtSymbol();
                    break;
                }

                case '$': {
                    Backtrack();
                    ParseDollarSymbol();
                    break;
                }

                // Literals and constants
                case '\'': {
                    ParseCharLiteral();
                    break;
                }

                case '"': {
                    ParseStringLiteral();
                    break;
                }

                default: {
                    if (std::isdigit(c)) {
                        Backtrack();
                        ParseDecimalLiteral();
                        break;
                    }

                    if (std::isalpha(c) || c == '_') {
                        Backtrack();
                        ParseIdentifier();
                        break;
                    }

                    fmt::print("Unknown character: {} ({:x})\n", c, c);

                    ARIA_UNREACHABLE();
                    break;
                }
            }
        }
    }

    char Lexer::Peek(size_t count) {
        if (m_Index + count < m_Source.Size()) {
            return m_Source.At(m_Index + count);
        } else {
            return '\0';
        }
    }

    void Lexer::Backtrack(size_t count) {
        ARIA_ASSERT(static_cast<ptrdiff_t>(m_Index - count) >= 0, "Lexer::Backtrack() out of bounds!");
        m_Index -= count;
    }

    void Lexer::Consume(size_t count) {
        ARIA_ASSERT(m_Index + count <= m_Source.Size(), "Lexer::Consume() out of bounds!");
        m_Index += count;
    }

    bool Lexer::TryConsume(char c) {
        if (Peek() == c) {
            Consume();
            return true;
        }

        return false;
    }

    void Lexer::ParseCharLiteral() {
        SourceLocation start = SourceLocation(m_CurrentLine, GetColumn(m_Index));

        if (TryConsume('\'')) {
            SourceLocation loc = SourceLocation(m_CurrentLine, GetColumn(m_Index));
            m_Context->ReportCompilerDiagnostic(loc, SourceRange(start, loc), "Empty character literal");
            return;
        }

        char c = ParseChar();

        if (!TryConsume('\'')) {
            SourceLocation loc = SourceLocation(m_CurrentLine, GetColumn(m_Index));
            m_Context->ReportCompilerDiagnostic(loc, SourceRange(start, loc), "Unterminated character literal");
            return;
        }

        SourceLocation loc = SourceLocation(m_CurrentLine, GetColumn(m_Index));
        AddTokenWithInteger(TokenKind::CharLit, SourceRange(start, loc), static_cast<u64>(c));
    }

    void Lexer::ParseDecimalLiteral() {
        bool encounteredPeriod = false;
                
        bool isHex = false;
        bool isBinary = false;
        bool isOctal = false;
        bool isDecimal = true;
        bool isUnsigned = false;

        if (Peek(1)) {
            if (Peek() == '0' && std::tolower(Peek(1)) == 'x') {
                Consume(2);
                isHex = true;
            } else if (Peek() == '0' && std::tolower(Peek(1)) == 'b') {
                Consume(2);
                isBinary = true;
            } else if (Peek() == '0' && std::tolower(Peek(1)) == 'o') {
                Consume(2);
                isOctal = true;
            }
        }

        isDecimal = !(isHex || isOctal || isBinary);

        size_t startIndex = m_Index;
        size_t numberStartIndex = m_Index;

        bool errored = false;

        // Parse the actual integer
        while (Peek()) {
            if (isDecimal) {
                if (std::isdigit(Peek())) {
                    Consume();
                } else if (Peek() == '.' && !encounteredPeriod) {
                    Consume();
                    encounteredPeriod = true;
                } else if (std::tolower(Peek()) == 'a' || std::tolower(Peek()) == 'b'
                        || std::tolower(Peek()) == 'c' || std::tolower(Peek()) == 'd'
                        || std::tolower(Peek()) == 'e' || std::tolower(Peek()) == 'f') {
                    SourceLocation loc = SourceLocation(m_CurrentLine, GetColumn(m_Index));
                    m_Context->ReportCompilerDiagnostic(loc, SourceRange(loc, loc), fmt::format("invalid hexadecimal digit '{}' in decimal literal", Peek()));
                    Consume();
                    errored = true;
                } else {
                    break;
                }
            } else if (isHex) {
                if (std::isdigit(Peek()) || std::tolower(Peek()) == 'a' || std::tolower(Peek()) == 'b'
                                         || std::tolower(Peek()) == 'c' || std::tolower(Peek()) == 'd'
                                         || std::tolower(Peek()) == 'e' || std::tolower(Peek()) == 'f') {
                    Consume();
                } else {
                    break;
                }
            } else if (isOctal) {
                if (Peek() >= '0' && Peek() <= '8') {
                    Consume();
                } else if (Peek() == '9') {
                    SourceLocation loc = SourceLocation(m_CurrentLine, GetColumn(m_Index));
                    m_Context->ReportCompilerDiagnostic(loc, SourceRange(loc, loc), "invalid digit '9' in octal literal");
                    Consume();
                    errored = true;
                } else if (std::tolower(Peek()) == 'a' || std::tolower(Peek()) == 'b'
                        || std::tolower(Peek()) == 'c' || std::tolower(Peek()) == 'd'
                        || std::tolower(Peek()) == 'e' || std::tolower(Peek()) == 'f') {
                    SourceLocation loc = SourceLocation(m_CurrentLine, GetColumn(m_Index));
                    m_Context->ReportCompilerDiagnostic(loc, SourceRange(loc, loc), fmt::format("invalid hexadecimal digit '{}' in octal literal", Peek()));
                    Consume();
                    errored = true;
                } else {
                    break;
                }
            } else if (isBinary) {
                if (Peek() == '0' || Peek() == '1') {
                    Consume();
                } else if (std::isdigit(Peek())) {
                    SourceLocation loc = SourceLocation(m_CurrentLine, GetColumn(m_Index));
                    m_Context->ReportCompilerDiagnostic(loc, SourceRange(loc, loc), fmt::format("invalid digit '{}' in binary literal", Peek()));
                    Consume();
                    errored = true;
                } else if (std::tolower(Peek()) == 'a' || std::tolower(Peek()) == 'b'
                        || std::tolower(Peek()) == 'c' || std::tolower(Peek()) == 'd'
                        || std::tolower(Peek()) == 'e' || std::tolower(Peek()) == 'f') {
                    SourceLocation loc = SourceLocation(m_CurrentLine, GetColumn(m_Index));
                    m_Context->ReportCompilerDiagnostic(loc, SourceRange(loc, loc), fmt::format("invalid hexadecimal digit '{}' in binary literal", Peek()));
                    Consume();
                    errored = true;
                } else {
                    break;
                }
            }
        }

        StringView buf(m_Source.Data() + startIndex, m_Index - startIndex);
        
        // Check for unsigned
        if (Peek() && std::tolower(Peek()) == 'u') {
            if (encounteredPeriod) {
                SourceLocation loc = SourceLocation(m_CurrentLine, GetColumn(m_Index));
                m_Context->ReportCompilerDiagnostic(loc, SourceRange(loc, loc), fmt::format("cannot use 'u' suffix in a floating-point literal"));
                Consume();
                errored = true;
            } else {
                Consume();
                isUnsigned = true;
            }
        }

        if (encounteredPeriod) {
            f64 number = 0.0;
            if (!errored) {
                auto [ptr, ec] = std::from_chars(buf.Data(), buf.Data() + buf.Size(), number); 

                if (ec == std::errc::result_out_of_range) {
                    SourceLocation loc(m_CurrentLine, GetColumn(m_Index - buf.Size()));
                    m_Context->ReportCompilerDiagnostic(loc, SourceRange(loc, SourceLocation(m_CurrentLine, GetColumn(m_Index))), 
                                                   "magnitude of floating-point literal is too large, maximum is 1.7976931348623157E+308");
                    number = 0.0;
                }
            }

            AddTokenWithNumber(TokenKind::NumLit,
                SourceRange(m_CurrentLine, GetColumn(startIndex), m_CurrentLine, GetColumn(m_Index)),
                number);
        } else {
            u64 integer = 0;

            if (!errored) {
                int base = 10;
                if (isHex) { base = 16; }
                else if (isBinary) { base = 2; }
                else if (isOctal) { base = 8; }

                auto [ptr, ec] = std::from_chars(buf.Data(), buf.Data() + buf.Size(), integer, base); 

                if (ec == std::errc::result_out_of_range) {
                    SourceLocation loc(m_CurrentLine, GetColumn(m_Index - buf.Size()));
                    m_Context->ReportCompilerDiagnostic(loc, SourceRange(loc, SourceLocation(m_CurrentLine, GetColumn(m_Index))), 
                                                   "integer literal is too large to fit into any integer type");
                    integer = 0;
                }
            }

            if (isUnsigned) {
                if (integer <= UINT32_MAX) {
                    AddTokenWithInteger(TokenKind::UIntLit,
                        SourceRange(m_CurrentLine, GetColumn(m_Index - buf.Size()), m_CurrentLine, GetColumn(m_Index)),
                        integer);
                } else {
                    AddTokenWithInteger(TokenKind::ULongLit,
                        SourceRange(m_CurrentLine, GetColumn(m_Index - buf.Size()), m_CurrentLine, GetColumn(m_Index)),
                        integer);
                }
            } else {
                if (integer <= INT32_MAX) {
                    AddTokenWithInteger(TokenKind::IntLit,
                        SourceRange(m_CurrentLine, GetColumn(m_Index - buf.Size()), m_CurrentLine, GetColumn(m_Index)),
                        integer);
                } else {
                    AddTokenWithInteger(TokenKind::LongLit,
                        SourceRange(m_CurrentLine, GetColumn(m_Index - buf.Size()), m_CurrentLine, GetColumn(m_Index)),
                        integer);
                }
            }
        }
    }

    void Lexer::ParseStringLiteral() {
        SourceLocation start = SourceLocation(m_CurrentLine, GetColumn(m_Index - 1));
        SourceLocation end;

        StringBuilder str;

        while (!end.IsValid()) {
            switch (Peek()) {
                case '"': {
                    Consume();
                    end = SourceLocation(m_CurrentLine, GetColumn(m_Index));
                    break;
                }

                case '\n':
                case '\0': {
                    end = SourceLocation(m_CurrentLine, GetColumn(m_Index));
                    m_Context->ReportCompilerDiagnostic(start, SourceRange(start, end), "Unterminated string literal");
                    return;
                }

                default: {
                    str.Append(m_Context, ParseChar());
                    break;
                }
            }
        }

        AddToken(TokenKind::StrLit, SourceRange(start, end), str);
    }

    void Lexer::ParseAtSymbol() {
        StringBuilder str;
        SourceLocation start = SourceLocation(m_CurrentLine, GetColumn(m_Index));

        str.Append(m_Context, '@');
        Consume();

        while (true) {
            if (std::isalpha(Peek())) {
                str.Append(m_Context, Peek());
                Consume();
            } else {
                break;
            }
        }

        if (str == "@extern") { AddToken(TokenKind::AtExtern, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "@extern"); return; }
        else if (str == "@nomangle") { AddToken(TokenKind::AtNoMangle, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "@nomangle"); return; }
        else if (str == "@private") { AddToken(TokenKind::AtPrivate, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "@private"); return; }

        m_Context->ReportCompilerDiagnostic(start, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "Unknown flag starting with '@'");
    }

    void Lexer::ParseDollarSymbol() {
        StringBuilder str;
        SourceLocation start = SourceLocation(m_CurrentLine, GetColumn(m_Index));

        str.Append(m_Context, '$');
        Consume();

        while (true) {
            if (std::isalpha(Peek())) {
                str.Append(m_Context, Peek());
                Consume();
            } else {
                break;
            }
        }

        if (str == "$format") { AddToken(TokenKind::DollarFormat, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "$format"); return; }
        m_Context->ReportCompilerDiagnostic(start, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "Unknown identifier following '$'");
    }

    void Lexer::ParseIdentifier() {
        StringBuilder str;
        SourceLocation start = SourceLocation(m_CurrentLine, GetColumn(m_Index));

        while (true) {
            if (std::isalnum(Peek()) || Peek() == '_') {
                str.Append(m_Context, Peek());
                Consume();
            } else {
                break;
            }
        }

        if (str == "true")     { AddToken(TokenKind::True,   SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "true");   return; }
        if (str == "false")    { AddToken(TokenKind::False,  SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "false");  return; }

        if (str == "module")   { AddToken(TokenKind::Module, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "module"); return; }
        if (str == "import")   { AddToken(TokenKind::Import, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "import"); return; }
        if (str == "let")      { AddToken(TokenKind::Let,    SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "let");    return; }
        if (str == "if")       { AddToken(TokenKind::If,     SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "if");     return; }
        if (str == "else")     { AddToken(TokenKind::Else,   SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "else");   return; }
        if (str == "while")    { AddToken(TokenKind::While,  SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "while");  return; }
        if (str == "do")       { AddToken(TokenKind::Do,     SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "do");     return; }
        if (str == "for")      { AddToken(TokenKind::For,    SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "for");    return; }
        if (str == "break")    { AddToken(TokenKind::Break,  SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "break");  return; }
        if (str == "continue") { AddToken(TokenKind::Continue,  SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "continue");  return; }
        if (str == "return")   { AddToken(TokenKind::Return, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "return"); return; }
        if (str == "self")     { AddToken(TokenKind::Self,   SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "self");   return; }
        if (str == "fn")       { AddToken(TokenKind::Fn,     SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "fn");     return; }
        if (str == "struct")   { AddToken(TokenKind::Struct, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "struct"); return; }

        if (str == "void")     { AddToken(TokenKind::Void,   SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "void");   return; }
        if (str == "bool")     { AddToken(TokenKind::Bool,   SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "bool");   return; }
        if (str == "char")     { AddToken(TokenKind::Char,   SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "char");   return; }
        if (str == "uchar")    { AddToken(TokenKind::UChar,  SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "uchar");  return; }
        if (str == "short")    { AddToken(TokenKind::Short,  SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "short");  return; }
        if (str == "ushort")   { AddToken(TokenKind::UShort, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "ushort"); return; }
        if (str == "int")      { AddToken(TokenKind::Int,    SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "int");    return; }
        if (str == "uint")     { AddToken(TokenKind::UInt,   SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "uint");   return; }
        if (str == "long")     { AddToken(TokenKind::Long,   SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "long");   return; }
        if (str == "ulong")    { AddToken(TokenKind::ULong,  SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "ulong");  return; }
        if (str == "float")    { AddToken(TokenKind::Float,  SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "float");  return; }
        if (str == "double")   { AddToken(TokenKind::Double, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "double"); return; }
        if (str == "string")   { AddToken(TokenKind::String, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), "string"); return; }

        AddToken(TokenKind::Identifier, SourceRange(start, SourceLocation(m_CurrentLine, GetColumn(m_Index))), str);
    }

    void Lexer::ParseSingleLineComment() {
        Consume(2); // Consume '//' 

        while (true) {
            switch (Peek()) {
                case '\n': {
                    m_CurrentLine++;
                    m_CurrentLineStart = m_Index;

                    Consume(); 
                    return;
                }
                case '\0': return;

                default: Consume(); break;
            }
        }
    }

    void Lexer::SkipWhitespace() {
        while (true) {
            switch (Peek()) {
                case '/': { // Potentially a comment
                    if (Peek(1) == '/') {
                        ParseSingleLineComment();
                        break;
                    }

                    return;
                }

                case '\n': {
                    m_CurrentLine++;
                    m_CurrentLineStart = m_Index;

                    Consume();
                    break;
                }

                case '\r':
                case ' ':
                case '\t': {
                    Consume();
                    break;
                }

                default: return;
            }
        }
    }

    char Lexer::ParseChar() {
        if (Peek() == '\\') {
            Consume();
            switch (Peek()) {
                case '\\': Consume(); return '\\';
                case '0': Consume(); return '\0';
                case 'n': Consume(); return '\n';
                case 't': Consume(); return '\t';
                case 's': Consume(); return ' ';
                case 'r': Consume(); return '\r';
                case '"': Consume(); return '"';
                case '\'': Consume(); return '\'';

                default: {
                    SourceLocation loc = SourceLocation(m_CurrentLine, GetColumn(m_Index));
                    m_Context->ReportCompilerDiagnostic(loc, SourceRange(loc, loc), "Unknown escape sequence");
                    return '\\';
                }
            }
        } else {
            Consume();
            return Peek(-1);
        }

        ARIA_UNREACHABLE();
    }

    void Lexer::AddToken(TokenKind kind, const SourceRange& range, StringView string) {
        Token token;
        token.Kind = kind;
        token.Range = range;
        token.String = string;

        m_Tokens.push_back(token);
    }

    void Lexer::AddTokenWithInteger(TokenKind kind, const SourceRange& range, u64 integer) {
        Token token;
        token.Kind = kind;
        token.Range = range;
        token.Integer = integer;

        m_Tokens.push_back(token);
    }

    void Lexer::AddTokenWithNumber(TokenKind kind, const SourceRange& range, f64 number) {
        Token token;
        token.Kind = kind;
        token.Range = range;
        token.Number = number;

        m_Tokens.push_back(token);
    }

    size_t Lexer::GetColumn(size_t index) {
        return (m_CurrentLineStart == 0) ? (index - m_CurrentLineStart) + 1 : index - m_CurrentLineStart;
    }

} // namespace Aria::Internal
