#pragma once

#include "aria/internal/compiler/core/source_location.hpp"
#include "aria/internal/compiler/core/string_view.hpp"
#include "aria/internal/types.hpp"

#include <vector>

namespace Aria::Internal {

    enum class TokenKind {
        // VVV Punctuation VVV //
        Semi,
        LeftParen,
        RightParen,
        LeftBracket,
        RightBracket,
        LeftCurly,
        RightCurly,
        Squigly,
        Comma,
        Colon,
        ColonColon,
        Dot,
        Arrow,
        // ^^^ Punctuation ^^^ //

        // VVV Operators VVV //
        Plus, PlusEq,
        Minus, MinusEq,
        Star, StarEq,
        Slash, SlashEq,
        Percent, PercentEq,
        Ampersand, AmpersandEq, DoubleAmpersand,
        Pipe, PipeEq, DoublePipe,
        UpArrow, UpArrowEq,
        LessLess, LessLessEq,
        GreaterGreater, GreaterGreaterEq,
        Eq, EqEq,
        Bang, BangEq,
        Less, LessEq,
        Greater, GreaterEq,
        // ^^^ Operators ^^^ //

        // VVV Literals and constants VVV //
        True,
        False,
        CharLit,
        IntLit,
        UIntLit,
        LongLit,
        ULongLit,
        NumLit,
        StrLit,
        Null,
        // ^^^ Literals and constants ^^^ //

        // VVV Keywords VVV //
        Module,
        Import,
        Let,
        If,
        Else,
        While,
        Do,
        For,
        Break,
        Continue,
        Return,
        Self,
        Fn,
        Struct,
        AtExtern, // @extern
        AtNoMangle, // @nomangle
        AtUnsafe, // @unsafe
        AtPrivate, // @private
        DollarFormat, // $format
        // ^^^ Keywords ^^^ //

        // VVV Types VVV //
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
        // ^^^ Types ^^^ //

        Identifier,

        Last
    };

    inline const char* TokenKindToString(TokenKind kind) {
        switch (kind) {
            // VVV Punctuation VVV //
            case TokenKind::Semi: return ";";
            case TokenKind::LeftParen: return "(";
            case TokenKind::RightParen: return ")";
            case TokenKind::LeftBracket: return "[";
            case TokenKind::RightBracket: return "]";
            case TokenKind::LeftCurly: return "{";
            case TokenKind::RightCurly: return "}";
            case TokenKind::Comma: return ",";
            case TokenKind::Colon: return ":";
            case TokenKind::ColonColon: return "::";
            case TokenKind::Dot: return ".";
            case TokenKind::Arrow: return "->";
            // ^^^ Punctuation ^^^ //

            // VVV Operators VVV //
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
            case TokenKind::LessLess: return "<<";
            case TokenKind::LessLessEq: return "<<=";
            case TokenKind::GreaterGreater: return ">>";
            case TokenKind::GreaterGreaterEq: return ">>=";
            case TokenKind::Eq: return "=";
            case TokenKind::EqEq: return "==";
            case TokenKind::Bang: return "!";
            case TokenKind::BangEq: return "!=";
            case TokenKind::Less: return "<";
            case TokenKind::LessEq: return "<=";
            case TokenKind::Greater: return ">";
            case TokenKind::GreaterEq: return ">=";
            // ^^^ Operators ^^^ //

            // VVV Literals and constants VVV //
            case TokenKind::True: return "true";
            case TokenKind::False: return "false";
            case TokenKind::CharLit: return "character literal";
            case TokenKind::IntLit: return "int literal";
            case TokenKind::UIntLit: return "uint literal";
            case TokenKind::LongLit: return "long literal";
            case TokenKind::ULongLit: return "ulong literal";
            case TokenKind::NumLit: return "number literal";
            case TokenKind::StrLit: return "string literal";
            case TokenKind::Null: return "null";
            // ^^^ Literals and constants ^^^ //

            // VVV Keywords VVV //
            case TokenKind::Module: return "module";
            case TokenKind::Import: return "import";
            case TokenKind::Let: return "let";
            case TokenKind::If: return "if";
            case TokenKind::Else: return "else";
            case TokenKind::While: return "while";
            case TokenKind::Do: return "do";
            case TokenKind::For: return "for";
            case TokenKind::Break: return "break";
            case TokenKind::Continue: return "continue";
            case TokenKind::Return: return "return";
            case TokenKind::Self: return "self";
            case TokenKind::Fn: return "fn";
            case TokenKind::Struct: return "struct";
            case TokenKind::AtExtern: return "@extern";
            case TokenKind::AtNoMangle: return "@nomangle";
            case TokenKind::AtUnsafe: return "@unsafe";
            case TokenKind::AtPrivate: return "@private";
            // ^^^ Keywords ^^^ //
            
            // VVV Types VVV //
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
            // ^^^ Types ^^^ //

            case TokenKind::Identifier: return "identifier";

            default: ARIA_UNREACHABLE();
        }

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
