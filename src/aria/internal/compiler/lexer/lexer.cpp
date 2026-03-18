#include "aria/internal/compiler/lexer/lexer.hpp"

#define ARIA_TOKEN_DATA(str, type) \
    if (buf == str) { \
        AddToken(TokenType::type, \
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
                AddToken(TokenType::yesEq, SourceRange(m_CurrentLine, GetColumn(m_Index - 2), m_CurrentLine, GetColumn(m_Index))); \
            } else { \
                AddToken(TokenType::noEq, SourceRange(m_CurrentLine, GetColumn(m_Index - 1), m_CurrentLine, GetColumn(m_Index))); \
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
                AddToken(TokenType::Identifier, 
                         SourceRange(m_CurrentLine, GetColumn(m_Index - buf.Size()), m_CurrentLine, GetColumn(m_Index)),
                         buf);
            } else if (std::isdigit(*Peek())) {
                size_t startIndex = m_Index;
                size_t endIndex = 0;

                bool encounteredPeriod = false;
                bool isUnsigned = false;
                bool isLong = false;
                bool isFloat = false;

                while (Peek()) {
                    if (std::isdigit(*Peek())) {
                        Consume();
                    } else if (*Peek() == '.' && !encounteredPeriod) {
                        Consume();
                        encounteredPeriod = true;
                    } else {
                        break;
                    }
                }

                StringView buf(m_Source.Data() + startIndex, m_Index - startIndex);

                // Handle suffixes (u, l, f)
                while (Peek()) {
                    if (*Peek() == 'u') {
                        ARIA_ASSERT(false, "TODO");
                    } else if (*Peek() == 'l') {
                        ARIA_ASSERT(false, "TODO");
                    } else if (*Peek() == 'f') {
                        ARIA_ASSERT(false, "TODO");
                    } else {
                        break;
                    }
                }

                if (encounteredPeriod) {
                    AddToken(TokenType::FloatLit,
                        SourceRange(m_CurrentLine, GetColumn(m_Index - buf.Size()), m_CurrentLine, GetColumn(m_Index)),
                        buf);
                    continue;
                } else {
                    AddToken(TokenType::IntLit,
                        SourceRange(m_CurrentLine, GetColumn(m_Index - buf.Size()), m_CurrentLine, GetColumn(m_Index)),
                        buf);
                    continue;
                }

                continue;
            } else if (!std::isspace(*Peek())) {
                switch (Consume()) {
                    case ';': AddToken(TokenType::Semi, 
                        SourceRange(m_CurrentLine, GetColumn(m_Index - 1), m_CurrentLine, GetColumn(m_Index))); break;
                    case '(': AddToken(TokenType::LeftParen,
                        SourceRange(m_CurrentLine, GetColumn(m_Index - 1), m_CurrentLine, GetColumn(m_Index))); break;
                    case ')': AddToken(TokenType::RightParen,
                        SourceRange(m_CurrentLine, GetColumn(m_Index - 1), m_CurrentLine, GetColumn(m_Index))); break;
                    case '[': AddToken(TokenType::LeftBracket,
                        SourceRange(m_CurrentLine, GetColumn(m_Index - 1), m_CurrentLine, GetColumn(m_Index))); break;
                    case ']': AddToken(TokenType::RightBracket,
                        SourceRange(m_CurrentLine, GetColumn(m_Index - 1), m_CurrentLine, GetColumn(m_Index))); break;
                    case '{': AddToken(TokenType::LeftCurly,
                        SourceRange(m_CurrentLine, GetColumn(m_Index - 1), m_CurrentLine, GetColumn(m_Index))); break;
                    case '}': AddToken(TokenType::RightCurly,
                        SourceRange(m_CurrentLine, GetColumn(m_Index - 1), m_CurrentLine, GetColumn(m_Index))); break;
                    case '~': AddToken(TokenType::Squigly,
                        SourceRange(m_CurrentLine, GetColumn(m_Index - 1), m_CurrentLine, GetColumn(m_Index))); break;
                    case ',': AddToken(TokenType::Comma,
                        SourceRange(m_CurrentLine, GetColumn(m_Index - 1), m_CurrentLine, GetColumn(m_Index))); break;
                    case ':': AddToken(TokenType::Colon,
                        SourceRange(m_CurrentLine, GetColumn(m_Index - 1), m_CurrentLine, GetColumn(m_Index))); break;
                    case '.': AddToken(TokenType::Dot,
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
                            AddToken(TokenType::SlashEq, 
                                SourceRange(m_CurrentLine, GetColumn(m_Index - 2), m_CurrentLine, GetColumn(m_Index)));
                        } else {
                            AddToken(TokenType::Slash,
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
                            AddToken(TokenType::AmpersandEq,
                                SourceRange(m_CurrentLine, GetColumn(m_Index - 2), m_CurrentLine, GetColumn(m_Index)));
                        } else if (isDouble) {
                            AddToken(TokenType::DoubleAmpersand,
                                SourceRange(m_CurrentLine, GetColumn(m_Index - 2), m_CurrentLine, GetColumn(m_Index)));
                        } else {
                            AddToken(TokenType::Ampersand,
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
                            AddToken(TokenType::PipeEq,
                                SourceRange(m_CurrentLine, GetColumn(m_Index - 2), m_CurrentLine, GetColumn(m_Index)));
                        } else if (isDouble) {
                            AddToken(TokenType::DoublePipe,
                                SourceRange(m_CurrentLine, GetColumn(m_Index - 2), m_CurrentLine, GetColumn(m_Index)));
                        } else {
                            AddToken(TokenType::Pipe,
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
                            AddToken(TokenType::UpArrowEq,
                                SourceRange(m_CurrentLine, GetColumn(m_Index - 2), m_CurrentLine, GetColumn(m_Index)));
                        } else {
                            AddToken(TokenType::UpArrow,
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

                        AddToken(TokenType::CharLit,
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

                        AddToken(TokenType::StrLit, 
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

    const char* Lexer::Peek() {
        if (m_Index < m_Source.Size()) {
            return m_Source.Data() + m_Index;
        } else {
            return nullptr;
        }
    }

    char Lexer::Consume() {
        ARIA_ASSERT(m_Index < m_Source.Size(), "Consume out of bounds!");
        m_Index++;
        return m_Source.At(m_Index - 1);
    }

    void Lexer::AddToken(TokenType type, const SourceRange& loc, const StringView data) {
        Token token;
        token.Type = type;
        token.Data = data;
        token.Range = loc;
        m_Tokens.push_back(token);
    }

    size_t Lexer::GetColumn(size_t index) {
        return (m_CurrentLineStart == 0) ? (index - m_CurrentLineStart) + 1 : index - m_CurrentLineStart;
    }

} // namespace Aria::Internal
