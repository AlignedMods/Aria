#pragma once

#include "aria/internal/compiler/core/source_location.hpp"
#include "aria/internal/compiler/core/string_view.hpp"
#include "aria/internal/types.hpp"

#include <vector>

namespace Aria::Internal {

    enum class TokenKind {
        Semi,
        LeftParen,
        RightParen,
        LeftBracket,
        RightBracket,
        LeftCurly,
        RightCurly,

        Plus, PlusEq,
        Minus, MinusEq,
        Star, StarEq,
        Slash, SlashEq,
        Percent, PercentEq,
        Ampersand, AmpersandEq, DoubleAmpersand,
        Pipe, PipeEq, DoublePipe,
        UpArrow, UpArrowEq,
        Eq, IsEq,
        Not, IsNotEq,
        Less, LessOrEq,
        Greater, GreaterOrEq,

        Comma,
        Colon,
        Dot,

        Self,

        If,
        Else,

        While,
        Do,
        For,

        Break,
        Return,

        True,
        False,

        Fn,
        Struct,

        Construct,
        Destruct,

        CharLit,
        IntLit,
        UintLit,
        NumLit,
        StrLit,

        Void,

        Bool,

        Char,
        UChar,
        Short,
        UShort,
        Int,
        UInt,
        Long,
        ULong,

        Float,
        Double,

        String,

        Extern,

        Identifier,

        Last
    };

    inline const char* TokenKindToString(TokenKind kind) {
        switch (kind) {
            case TokenKind::Semi: return ";";
            case TokenKind::LeftParen: return "(";
            case TokenKind::RightParen: return ")";
            case TokenKind::LeftBracket: return "[";
            case TokenKind::RightBracket: return "]";
            case TokenKind::LeftCurly: return "{";
            case TokenKind::RightCurly: return "}";

            case TokenKind::Plus: return "+";
            case TokenKind::PlusEq: return "+=";
            case TokenKind::Minus: return "-";
            case TokenKind::MinusEq: return "-=";
            case TokenKind::Star: return "*";
            case TokenKind::StarEq: return "*=";
            case TokenKind::Slash: return "/";
            case TokenKind::SlashEq: return "/=";
            case TokenKind::Percent: return "%";
            case TokenKind::PercentEq: return "%=";
            case TokenKind::Ampersand: return "&";
            case TokenKind::AmpersandEq: return "&=";
            case TokenKind::DoubleAmpersand: return "&&";
            case TokenKind::Pipe: return "|";
            case TokenKind::PipeEq: return "|=";
            case TokenKind::DoublePipe: return "||";
            case TokenKind::UpArrow: return "^";
            case TokenKind::UpArrowEq: return "^=";
            case TokenKind::Eq: return "=";
            case TokenKind::IsEq: return "==";
            case TokenKind::Not: return "!";
            case TokenKind::IsNotEq: return "!=";
            case TokenKind::Less: return "<";
            case TokenKind::LessOrEq: return "<=";
            case TokenKind::Greater: return ">";
            case TokenKind::GreaterOrEq: return ">=";

            case TokenKind::Comma: return ",";
            case TokenKind::Colon: return ":";
            case TokenKind::Dot: return ".";

            case TokenKind::Self: return "self";

            case TokenKind::If: return "if";
            case TokenKind::Else: return "else";

            case TokenKind::While: return "while";
            case TokenKind::Do: return "do";
            case TokenKind::For: return "for";

            case TokenKind::Break: return "break";
            case TokenKind::Return: return "return";

            case TokenKind::Fn: return "fn";
            case TokenKind::Struct: return "struct";

            case TokenKind::Construct: return "construct";
            case TokenKind::Destruct: return "destruct";

            case TokenKind::True: return "true";
            case TokenKind::False: return "false";

            case TokenKind::CharLit: return "character literal";
            case TokenKind::IntLit: return "integer literal";
            case TokenKind::NumLit: return "number literal";
            case TokenKind::StrLit: return "string literal";

            case TokenKind::Void: return "void";

            case TokenKind::Bool: return "bool";

            case TokenKind::Char: return "char";
            case TokenKind::UChar: return "uchar";
            case TokenKind::Short: return "short";
            case TokenKind::UShort: return "ushort";
            case TokenKind::Int: return "int";
            case TokenKind::UInt: return "uint";
            case TokenKind::Long: return "long";
            case TokenKind::ULong: return "ulong";

            case TokenKind::Float: return "float";
            case TokenKind::Double: return "double";

            case TokenKind::String: return "string";

            case TokenKind::Identifier: return "identifier";
        }

        ARIA_ASSERT(false, "Unreachable!");

        return nullptr;
    }

    struct Token {
        TokenKind Kind = TokenKind::Semi;
        SourceRange Range;

        StringView String;
        u64 Integer = 0;
        f64 Number = 0.0;
    };

    using Tokens = std::vector<Token>;

} // namespace Aria::Internal
