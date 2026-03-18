#pragma once

#include "aria/internal/compiler/core/source_location.hpp"
#include "aria/internal/compiler/core/string_view.hpp"

#include <vector>

namespace Aria::Internal {

    enum class TokenType {
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
        Hash,

        Squigly,
        Comma,
        Colon,
        Dot,
        DoubleDot,
        TripleDot,

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

        Struct,

        Construct,
        Destruct,

        CharLit,
        IntLit,
        FloatLit,
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

        Identifier
    };

    inline const char* TokenTypeToString(TokenType type) {
        switch (type) {
            case TokenType::Semi: return ";";
            case TokenType::LeftParen: return "(";
            case TokenType::RightParen: return ")";
            case TokenType::LeftBracket: return "[";
            case TokenType::RightBracket: return "]";
            case TokenType::LeftCurly: return "{";
            case TokenType::RightCurly: return "}";

            case TokenType::Plus: return "+";
            case TokenType::PlusEq: return "+=";
            case TokenType::Minus: return "-";
            case TokenType::MinusEq: return "-=";
            case TokenType::Star: return "*";
            case TokenType::StarEq: return "*=";
            case TokenType::Slash: return "/";
            case TokenType::SlashEq: return "/=";
            case TokenType::Percent: return "%";
            case TokenType::PercentEq: return "%=";
            case TokenType::Ampersand: return "&";
            case TokenType::AmpersandEq: return "&=";
            case TokenType::DoubleAmpersand: return "&&";
            case TokenType::Pipe: return "|";
            case TokenType::PipeEq: return "|=";
            case TokenType::DoublePipe: return "||";
            case TokenType::UpArrow: return "^";
            case TokenType::UpArrowEq: return "^=";
            case TokenType::Eq: return "=";
            case TokenType::IsEq: return "==";
            case TokenType::Not: return "!";
            case TokenType::IsNotEq: return "!=";
            case TokenType::Less: return "<";
            case TokenType::LessOrEq: return "<=";
            case TokenType::Greater: return ">";
            case TokenType::GreaterOrEq: return ">=";
            case TokenType::Hash: return "#";

            case TokenType::Squigly: return "~";
            case TokenType::Comma: return ",";
            case TokenType::Colon: return ":";
            case TokenType::Dot: return ".";
            case TokenType::DoubleDot: return "..";
            case TokenType::TripleDot: return "...";

            case TokenType::Self: return "self";

            case TokenType::If: return "if";
            case TokenType::Else: return "else";

            case TokenType::While: return "while";
            case TokenType::Do: return "do";
            case TokenType::For: return "for";

            case TokenType::Break: return "break";
            case TokenType::Return: return "return";

            case TokenType::Struct: return "struct";

            case TokenType::Construct: return "construct";
            case TokenType::Destruct: return "destruct";

            case TokenType::True: return "true";
            case TokenType::False: return "false";

            case TokenType::CharLit: return "char-lit";
            case TokenType::IntLit: return "int-lit";
            case TokenType::FloatLit: return "float-lit";
            case TokenType::StrLit: return "str-lit";

            case TokenType::Void: return "void";

            case TokenType::Bool: return "bool";

            case TokenType::Char: return "char";
            case TokenType::UChar: return "uchar";
            case TokenType::Short: return "short";
            case TokenType::UShort: return "ushort";
            case TokenType::Int: return "int";
            case TokenType::UInt: return "uint";
            case TokenType::Long: return "long";
            case TokenType::ULong: return "ulong";

            case TokenType::Float: return "float";
            case TokenType::Double: return "double";

            case TokenType::String: return "string";

            case TokenType::Extern: return "extern";

            case TokenType::Identifier: return "identifier";
        }

        ARIA_ASSERT(false, "Unreachable!");

        return nullptr;
    }

    struct Token {
        TokenType Type = TokenType::Semi;
        StringView Data;
        SourceRange Range;
    };

    using Tokens = std::vector<Token>;

} // namespace Aria::Internal
