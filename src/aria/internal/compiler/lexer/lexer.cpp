#include "aria/internal/compiler/lexer/lexer.hpp"

#include <charconv>
#include <limits>

#define ARIA_TOKEN_DATA(str, type) \
    if (buf == str) { \
        AddToken(TokenKind::type, \
            SourceRange(m_CurrentLine, GetColumn(m_Index - buf.Size()), m_CurrentLine, GetColumn(m_Index)), \
            buf); \
        continue; \
    }

#define ARIA_TOKEN_POSSIBLE_EQ(base, noEq, yesEq) \
    case base: { \
        bool isEq = false; \
        \
        if (Peek()) { \
            char nc = *Peek(); \
            \
            if (nc == '=') { \
                Consume(); \
                isEq = true; \
            } \
            \
            if (isEq) { \
                AddToken(TokenKind::yesEq, SourceRange(m_CurrentLine, GetColumn(m_Index - 2), m_CurrentLine, GetColumn(m_Index))); \
            } else { \
                AddToken(TokenKind::noEq, SourceRange(m_CurrentLine, GetColumn(m_Index - 1), m_CurrentLine, GetColumn(m_Index))); \
            } \
        } \
        break; \
    }

namespace Aria::Internal {

    Lexer::Lexer(CompilationContext* ctx) {
        m_Context = ctx;
        m_Source = ctx->GetSourceCode();

        LexImpl();
    }

    void Lexer::LexImpl() {
        while (Peek()) {
            if (std::isalpha(*Peek())) {
                size_t startIndex = m_Index;

                while (std::isalnum(*Peek()) || *Peek() == '_') {
                    Consume();
                }

                StringView buf(m_Source.Data() + startIndex, m_Index - startIndex);
                ARIA_TOKEN_DATA("self", Self)

                ARIA_TOKEN_DATA("if", If)
                ARIA_TOKEN_DATA("else", Else)

                ARIA_TOKEN_DATA("while", While)
                ARIA_TOKEN_DATA("do", Do)
                ARIA_TOKEN_DATA("for", For)

                ARIA_TOKEN_DATA("break", Break)
                ARIA_TOKEN_DATA("return", Return)

                ARIA_TOKEN_DATA("struct", Struct)

                ARIA_TOKEN_DATA("construct", Construct)
                ARIA_TOKEN_DATA("destruct", Destruct)

                ARIA_TOKEN_DATA("true", True)
                ARIA_TOKEN_DATA("false", False)

                ARIA_TOKEN_DATA("void", Void)

                ARIA_TOKEN_DATA("bool", Bool)

                ARIA_TOKEN_DATA("char", Char)
                ARIA_TOKEN_DATA("uchar", UChar)
                ARIA_TOKEN_DATA("short", Short)
                ARIA_TOKEN_DATA("ushort", UShort)
                ARIA_TOKEN_DATA("int", Int)
                ARIA_TOKEN_DATA("uint", UInt)
                ARIA_TOKEN_DATA("long", Long)
                ARIA_TOKEN_DATA("ulong", ULong)

                ARIA_TOKEN_DATA("float", Float)
                ARIA_TOKEN_DATA("double", Double)

                ARIA_TOKEN_DATA("string", String)

                ARIA_TOKEN_DATA("extern", Extern)

                // If all else fails, it's an identifier
                AddToken(TokenKind::Identifier, 
                         SourceRange(m_CurrentLine, GetColumn(m_Index - buf.Size()), m_CurrentLine, GetColumn(m_Index)),
                         buf);
            } else if (std::isdigit(*Peek())) {
                bool encounteredPeriod = false;
                
                bool isHex = false;
                bool isBinary = false;
                bool isOctal = false;
                bool isDecimal = true;
                bool isUnsigned = false;

                if (Peek(1)) {
                    if (*Peek() == '0' && std::tolower(*Peek(1)) == 'x') {
                        Consume();
                        Consume();

                        isHex = true;
                    } else if (*Peek() == '0' && std::tolower(*Peek(1)) == 'b') {
                        Consume();
                        Consume();

                        isBinary = true;
                    } else if (*Peek() == '0' && std::tolower(*Peek(1)) == 'o') {
                        Consume();
                        Consume();

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
                        if (std::isdigit(*Peek())) {
                            Consume();
                        } else if (*Peek() == '.' && !encounteredPeriod) {
                            Consume();
                            encounteredPeriod = true;
                        } else if (std::tolower(*Peek()) == 'a' || std::tolower(*Peek()) == 'b'
                                || std::tolower(*Peek()) == 'c' || std::tolower(*Peek()) == 'd'
                                || std::tolower(*Peek()) == 'e' || std::tolower(*Peek()) == 'f') {
                            SourceLocation loc = SourceLocation(m_CurrentLine, GetColumn(m_Index));
                            m_Context->ReportCompilerError(loc, SourceRange(loc, loc), fmt::format("invalid hexadecimal digit '{}' in decimal literal", *Peek()));
                            Consume();
                            errored = true;
                        } else {
                            break;
                        }
                    } else if (isHex) {
                        if (std::isdigit(*Peek()) || std::tolower(*Peek()) == 'a' || std::tolower(*Peek()) == 'b'
                                                  || std::tolower(*Peek()) == 'c' || std::tolower(*Peek()) == 'd'
                                                  || std::tolower(*Peek()) == 'e' || std::tolower(*Peek()) == 'f') {
                            Consume();
                        } else {
                            break;
                        }
                    } else if (isOctal) {
                        if (*Peek() >= '0' && *Peek() <= '8') {
                            Consume();
                        } else if (*Peek() == '9') {
                            SourceLocation loc = SourceLocation(m_CurrentLine, GetColumn(m_Index));
                            m_Context->ReportCompilerError(loc, SourceRange(loc, loc), "invalid digit '9' in octal literal");
                            Consume();
                            errored = true;
                        } else if (std::tolower(*Peek()) == 'a' || std::tolower(*Peek()) == 'b'
                                || std::tolower(*Peek()) == 'c' || std::tolower(*Peek()) == 'd'
                                || std::tolower(*Peek()) == 'e' || std::tolower(*Peek()) == 'f') {
                            SourceLocation loc = SourceLocation(m_CurrentLine, GetColumn(m_Index));
                            m_Context->ReportCompilerError(loc, SourceRange(loc, loc), fmt::format("invalid hexadecimal digit '{}' in octal literal", *Peek()));
                            Consume();
                            errored = true;
                        } else {
                            break;
                        }
                    } else if (isBinary) {
                        if (*Peek() == '0' || *Peek() == '1') {
                            Consume();
                        } else if (std::isdigit(*Peek())) {
                            SourceLocation loc = SourceLocation(m_CurrentLine, GetColumn(m_Index));
                            m_Context->ReportCompilerError(loc, SourceRange(loc, loc), fmt::format("invalid digit '{}' in binary literal", *Peek()));
                            Consume();
                            errored = true;
                        } else if (std::tolower(*Peek()) == 'a' || std::tolower(*Peek()) == 'b'
                                || std::tolower(*Peek()) == 'c' || std::tolower(*Peek()) == 'd'
                                || std::tolower(*Peek()) == 'e' || std::tolower(*Peek()) == 'f') {
                            SourceLocation loc = SourceLocation(m_CurrentLine, GetColumn(m_Index));
                            m_Context->ReportCompilerError(loc, SourceRange(loc, loc), fmt::format("invalid hexadecimal digit '{}' in binary literal", *Peek()));
                            Consume();
                            errored = true;
                        } else {
                            break;
                        }
                    }
                }

                StringView buf(m_Source.Data() + startIndex, m_Index - startIndex);
                
                // Check for unsigned
                if (Peek() && std::tolower(*Peek()) == 'u') {
                    if (encounteredPeriod) {
                        SourceLocation loc = SourceLocation(m_CurrentLine, GetColumn(m_Index));
                        m_Context->ReportCompilerError(loc, SourceRange(loc, loc), fmt::format("cannot use 'u' suffix in a floating-point literal"));
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
                            m_Context->ReportCompilerError(loc, SourceRange(loc, SourceLocation(m_CurrentLine, GetColumn(m_Index))), 
                                                           "magnitude of floating-point literal is too large, maximum is 1.7976931348623157E+308");
                            number = 0.0;
                        }
                    }

                    AddToken(TokenKind::NumLit,
                        SourceRange(m_CurrentLine, GetColumn(startIndex), m_CurrentLine, GetColumn(m_Index)),
                        buf, 0, number);
                    continue;
                } else {
                    u64 integer = 0;

                    if (!errored) {
                        size_t base = 10;
                        if (isHex) { base = 16; }
                        else if (isBinary) { base = 2; }
                        else if (isOctal) { base = 8; }

                        auto [ptr, ec] = std::from_chars(buf.Data(), buf.Data() + buf.Size(), integer, base); 

                        if (ec == std::errc::result_out_of_range) {
                            SourceLocation loc(m_CurrentLine, GetColumn(m_Index - buf.Size()));
                            m_Context->ReportCompilerError(loc, SourceRange(loc, SourceLocation(m_CurrentLine, GetColumn(m_Index))), 
                                                           "integer literal is too large to fit into any integer type");
                            integer = 0;
                        }
                    }

                    if (isUnsigned) {
                        AddToken(TokenKind::UintLit,
                            SourceRange(m_CurrentLine, GetColumn(m_Index - buf.Size()), m_CurrentLine, GetColumn(m_Index)),
                            buf, integer);
                    } else {
                        AddToken(TokenKind::IntLit,
                            SourceRange(m_CurrentLine, GetColumn(m_Index - buf.Size()), m_CurrentLine, GetColumn(m_Index)),
                            buf, integer);
                    }
                    continue;
                }

                continue;
            } else if (!std::isspace(*Peek())) {
                switch (Consume()) {
                    case ';': AddToken(TokenKind::Semi, 
                        SourceRange(m_CurrentLine, GetColumn(m_Index - 1), m_CurrentLine, GetColumn(m_Index))); break;
                    case '(': AddToken(TokenKind::LeftParen,
                        SourceRange(m_CurrentLine, GetColumn(m_Index - 1), m_CurrentLine, GetColumn(m_Index))); break;
                    case ')': AddToken(TokenKind::RightParen,
                        SourceRange(m_CurrentLine, GetColumn(m_Index - 1), m_CurrentLine, GetColumn(m_Index))); break;
                    case '[': AddToken(TokenKind::LeftBracket,
                        SourceRange(m_CurrentLine, GetColumn(m_Index - 1), m_CurrentLine, GetColumn(m_Index))); break;
                    case ']': AddToken(TokenKind::RightBracket,
                        SourceRange(m_CurrentLine, GetColumn(m_Index - 1), m_CurrentLine, GetColumn(m_Index))); break;
                    case '{': AddToken(TokenKind::LeftCurly,
                        SourceRange(m_CurrentLine, GetColumn(m_Index - 1), m_CurrentLine, GetColumn(m_Index))); break;
                    case '}': AddToken(TokenKind::RightCurly,
                        SourceRange(m_CurrentLine, GetColumn(m_Index - 1), m_CurrentLine, GetColumn(m_Index))); break;
                    case '~': AddToken(TokenKind::Squigly,
                        SourceRange(m_CurrentLine, GetColumn(m_Index - 1), m_CurrentLine, GetColumn(m_Index))); break;
                    case ',': AddToken(TokenKind::Comma,
                        SourceRange(m_CurrentLine, GetColumn(m_Index - 1), m_CurrentLine, GetColumn(m_Index))); break;
                    case ':': AddToken(TokenKind::Colon,
                        SourceRange(m_CurrentLine, GetColumn(m_Index - 1), m_CurrentLine, GetColumn(m_Index))); break;
                    case '.': AddToken(TokenKind::Dot,
                        SourceRange(m_CurrentLine, GetColumn(m_Index - 1), m_CurrentLine, GetColumn(m_Index))); break;

                    ARIA_TOKEN_POSSIBLE_EQ('+', Plus, PlusEq)
                    ARIA_TOKEN_POSSIBLE_EQ('-', Minus, MinusEq)
                    ARIA_TOKEN_POSSIBLE_EQ('*', Star, StarEq)
                    ARIA_TOKEN_POSSIBLE_EQ('%', Percent, PercentEq)
                    ARIA_TOKEN_POSSIBLE_EQ('=', Eq, IsEq)
                    ARIA_TOKEN_POSSIBLE_EQ('!', Not, IsNotEq)
                    ARIA_TOKEN_POSSIBLE_EQ('<', Less, LessOrEq)
                    ARIA_TOKEN_POSSIBLE_EQ('>', Greater, GreaterOrEq)

                    case '/': {
                        bool isComment = false;
                        bool isEq = false;

                        if (Peek()) {
                            char nc = *Peek(); // Don't consume the character just in case

                            if (nc == '/') {
                                Consume();
                                isComment = true;
                            } else if (nc == '=') {
                                Consume();
                                isEq = true;
                            }
                        }

                        if (isComment) {
                            while (Peek()) {
                                char nc = Consume();

                                if (nc == '\n' || nc == EOF) {
                                    m_CurrentLine++;
                                    m_CurrentLineStart = m_Index - 1;
                                    break;
                                }
                            }
                        } else if (isEq) {
                            AddToken(TokenKind::SlashEq, 
                                SourceRange(m_CurrentLine, GetColumn(m_Index - 2), m_CurrentLine, GetColumn(m_Index)));
                        } else {
                            AddToken(TokenKind::Slash,
                                SourceRange(m_CurrentLine, GetColumn(m_Index - 1), m_CurrentLine, GetColumn(m_Index)));
                        }

                        break;
                    }

                    case '&': {
                        bool isEq = false;
                        bool isDouble = false;

                        if (Peek()) {
                            char nc = *Peek();

                            if (nc == '&') {
                                Consume();
                                isDouble = true;
                            } else if (nc == '=') {
                                Consume();
                                isEq = true;
                            }
                        }

                        if (isEq) {
                            AddToken(TokenKind::AmpersandEq,
                                SourceRange(m_CurrentLine, GetColumn(m_Index - 2), m_CurrentLine, GetColumn(m_Index)));
                        } else if (isDouble) {
                            AddToken(TokenKind::DoubleAmpersand,
                                SourceRange(m_CurrentLine, GetColumn(m_Index - 2), m_CurrentLine, GetColumn(m_Index)));
                        } else {
                            AddToken(TokenKind::Ampersand,
                                SourceRange(m_CurrentLine, GetColumn(m_Index - 1), m_CurrentLine, GetColumn(m_Index)));
                        }

                        break;
                    }

                    case '|': {
                        bool isEq = false;
                        bool isDouble = false;

                        if (Peek()) {
                            char nc = *Peek();

                            if (nc == '|') {
                                Consume();
                                isDouble = true;
                            } else if (nc == '=') {
                                Consume();
                                isEq = true;
                            }
                        }

                        if (isEq) {
                            AddToken(TokenKind::PipeEq,
                                SourceRange(m_CurrentLine, GetColumn(m_Index - 2), m_CurrentLine, GetColumn(m_Index)));
                        } else if (isDouble) {
                            AddToken(TokenKind::DoublePipe,
                                SourceRange(m_CurrentLine, GetColumn(m_Index - 2), m_CurrentLine, GetColumn(m_Index)));
                        } else {
                            AddToken(TokenKind::Pipe,
                                SourceRange(m_CurrentLine, GetColumn(m_Index - 1), m_CurrentLine, GetColumn(m_Index)));
                        }

                        break;
                    }

                    case '^': {
                        bool isEq = false;

                        if (Peek()) {
                            char nc = *Peek();

                            if (nc == '=') {
                                Consume();
                                isEq = true;
                            }
                        }

                        if (isEq) {
                            AddToken(TokenKind::UpArrowEq,
                                SourceRange(m_CurrentLine, GetColumn(m_Index - 2), m_CurrentLine, GetColumn(m_Index)));
                        } else {
                            AddToken(TokenKind::UpArrow,
                                SourceRange(m_CurrentLine, GetColumn(m_Index - 1), m_CurrentLine, GetColumn(m_Index)));
                        }

                        break;
                    }

                    case '\'': {
                        size_t startIndex = m_Index - 1;

                        if (Peek()) {
                            Consume();
                            
                            if (Peek() && *Peek() == '\'') {
                                Consume();
                            } else {
                                // TODO: Add error message
                            }
                        }

                        AddToken(TokenKind::CharLit,
                            SourceRange(m_CurrentLine, GetColumn(startIndex), m_CurrentLine, GetColumn(m_Index)),
                            StringView(m_Source.Data() + startIndex + 1, 1));
                        break;
                    }

                    case '"': {
                        size_t startIndex = m_Index - 1;
                       
                        while (Peek()) {
                            char nc = Consume();
                        
                            if (nc == '"' || nc == EOF) {
                                break;
                            }
                        }

                        AddToken(TokenKind::StrLit, 
                            SourceRange(m_CurrentLine, GetColumn(startIndex), m_CurrentLine, GetColumn(m_Index)), 
                            StringView(m_Source.Data() + startIndex + 1, m_Index - startIndex));
                        break;
                    }
                }
            } else {
                if (*Peek() == '\n') {
                    m_CurrentLine++;
                    m_CurrentLineStart = m_Index;
                }

                Consume();

                continue;
            }
        }

        m_Context->SetTokens(m_Tokens);
    }

    const char* Lexer::Peek(size_t count) {
        if (m_Index + count < m_Source.Size()) {
            return m_Source.Data() + m_Index + count;
        } else {
            return nullptr;
        }
    }

    char Lexer::Consume() {
        ARIA_ASSERT(m_Index < m_Source.Size(), "Consume out of bounds!");
        m_Index++;
        return m_Source.At(m_Index - 1);
    }

    void Lexer::AddToken(TokenKind kind, const SourceRange& loc, const StringView string, u64 integer, f64 number) {
        Token token;
        token.Kind = kind;
        token.Range = loc;

        token.String = string;
        token.Integer = integer;
        token.Number = number;

        m_Tokens.push_back(token);
    }

    size_t Lexer::GetColumn(size_t index) {
        return (m_CurrentLineStart == 0) ? (index - m_CurrentLineStart) + 1 : index - m_CurrentLineStart;
    }

} // namespace Aria::Internal
