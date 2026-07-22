#include "ariac/parser/parser.hpp"
#include "ariac/core/scratch_buffer.hpp"

#include "fmt/format.h"

#include <string>
#include <charconv>
#include <limits>

#define BIND_PARSE_RULE(fn) [this](Expr* expr) -> Expr* { return fn(expr); }

static constexpr size_t PREC_NONE = 0;
static constexpr size_t PREC_ASSIGNMENT = 10;
static constexpr size_t PREC_OR = 20;
static constexpr size_t PREC_AND = 30;
static constexpr size_t PREC_RELATIONAL = 40;
static constexpr size_t PREC_ADDITIVE = 50;
static constexpr size_t PREC_BIT = 60;
static constexpr size_t PREC_SHIFT = 70;
static constexpr size_t PREC_MULTIPLICATIVE = 80;
static constexpr size_t PREC_CALL = 90;

namespace ariac {

    Parser::Parser() {
        m_tokens = context.active_comp_unit->tokens;

        if (error_expr.type == nullptr) { error_expr.type = TypeInfo::get_error(); }

        add_expr_rules();
        parse_impl();
    }

    void Parser::add_expr_rules() {
        m_expr_rules.reserve(static_cast<size_t>(TokenKind::Last));

        // PREC_CALL
        m_expr_rules[TokenKind::LeftParen] =         { BIND_PARSE_RULE(parse_grouping), BIND_PARSE_RULE(parse_call), PREC_CALL };
        m_expr_rules[TokenKind::LeftBracket] =       { BIND_PARSE_RULE(parse_type_expr), BIND_PARSE_RULE(parse_array_subscript), PREC_CALL };
        m_expr_rules[TokenKind::Dot] =               { nullptr, BIND_PARSE_RULE(parse_member), PREC_CALL };
        m_expr_rules[TokenKind::Bang] =              { BIND_PARSE_RULE(parse_unary), nullptr, PREC_CALL };
        m_expr_rules[TokenKind::PlusPlus] =          { BIND_PARSE_RULE(parse_unary), BIND_PARSE_RULE(parse_infix_unary), PREC_CALL };
        m_expr_rules[TokenKind::MinusMinus] =        { BIND_PARSE_RULE(parse_unary), BIND_PARSE_RULE(parse_infix_unary), PREC_CALL };

        // PREC_MULTIPLICATIVE
        m_expr_rules[TokenKind::Star] =              { BIND_PARSE_RULE(parse_unary), BIND_PARSE_RULE(parse_binary), PREC_MULTIPLICATIVE };
        m_expr_rules[TokenKind::Slash] =             { nullptr, BIND_PARSE_RULE(parse_binary), PREC_MULTIPLICATIVE };
        m_expr_rules[TokenKind::Percent] =           { nullptr, BIND_PARSE_RULE(parse_binary), PREC_MULTIPLICATIVE };

        // PREC_SHIFT
        m_expr_rules[TokenKind::LessLess] =          { nullptr, BIND_PARSE_RULE(parse_binary), PREC_SHIFT };
        m_expr_rules[TokenKind::GreaterGreater] =    { nullptr, BIND_PARSE_RULE(parse_binary), PREC_SHIFT };

        // PREC_BIT
        m_expr_rules[TokenKind::Ampersand] =         { BIND_PARSE_RULE(parse_unary), BIND_PARSE_RULE(parse_binary), PREC_BIT };
        m_expr_rules[TokenKind::Pipe] =              { nullptr, BIND_PARSE_RULE(parse_binary), PREC_BIT };
        m_expr_rules[TokenKind::UpArrow] =           { nullptr, BIND_PARSE_RULE(parse_binary), PREC_BIT };

        // PREC_ADDITIVE
        m_expr_rules[TokenKind::Plus] =              { nullptr, BIND_PARSE_RULE(parse_binary), PREC_ADDITIVE };
        m_expr_rules[TokenKind::Minus] =             { BIND_PARSE_RULE(parse_unary), BIND_PARSE_RULE(parse_binary), PREC_ADDITIVE };

        // PREC_RELATIONAL
        m_expr_rules[TokenKind::EqEq] =              { nullptr, BIND_PARSE_RULE(parse_binary), PREC_RELATIONAL };
        m_expr_rules[TokenKind::BangEq] =            { nullptr, BIND_PARSE_RULE(parse_binary), PREC_RELATIONAL };
        m_expr_rules[TokenKind::Less] =              { nullptr, BIND_PARSE_RULE(parse_binary), PREC_RELATIONAL };
        m_expr_rules[TokenKind::LessEq] =            { nullptr, BIND_PARSE_RULE(parse_binary), PREC_RELATIONAL };
        m_expr_rules[TokenKind::Greater] =           { nullptr, BIND_PARSE_RULE(parse_binary), PREC_RELATIONAL };
        m_expr_rules[TokenKind::GreaterEq] =         { nullptr, BIND_PARSE_RULE(parse_binary), PREC_RELATIONAL };
        
        // PREC_AND
        m_expr_rules[TokenKind::DoubleAmpersand] =   { BIND_PARSE_RULE(parse_unary), BIND_PARSE_RULE(parse_binary), PREC_AND };

        // PREC_OR
        m_expr_rules[TokenKind::DoublePipe] =        { nullptr, BIND_PARSE_RULE(parse_binary), PREC_OR };
               
        // PREC_ASSIGNMENT
        m_expr_rules[TokenKind::Eq] =                { nullptr, BIND_PARSE_RULE(parse_binary), PREC_ASSIGNMENT };
        m_expr_rules[TokenKind::PlusEq] =            { nullptr, BIND_PARSE_RULE(parse_compound_assignment), PREC_ASSIGNMENT };
        m_expr_rules[TokenKind::MinusEq] =           { nullptr, BIND_PARSE_RULE(parse_compound_assignment), PREC_ASSIGNMENT };
        m_expr_rules[TokenKind::StarEq] =            { nullptr, BIND_PARSE_RULE(parse_compound_assignment), PREC_ASSIGNMENT };
        m_expr_rules[TokenKind::SlashEq] =           { nullptr, BIND_PARSE_RULE(parse_compound_assignment), PREC_ASSIGNMENT };
        m_expr_rules[TokenKind::PercentEq] =         { nullptr, BIND_PARSE_RULE(parse_compound_assignment), PREC_ASSIGNMENT };
        m_expr_rules[TokenKind::AmpersandEq] =       { nullptr, BIND_PARSE_RULE(parse_compound_assignment), PREC_ASSIGNMENT };
        m_expr_rules[TokenKind::PipeEq] =            { nullptr, BIND_PARSE_RULE(parse_compound_assignment), PREC_ASSIGNMENT };
        m_expr_rules[TokenKind::UpArrowEq] =         { nullptr, BIND_PARSE_RULE(parse_compound_assignment), PREC_ASSIGNMENT };
        m_expr_rules[TokenKind::LessLessEq] =        { nullptr, BIND_PARSE_RULE(parse_compound_assignment), PREC_ASSIGNMENT };
        m_expr_rules[TokenKind::GreaterGreaterEq] =  { nullptr, BIND_PARSE_RULE(parse_compound_assignment), PREC_ASSIGNMENT };

        // PREC_NONE                                     
        m_expr_rules[TokenKind::True] =              { BIND_PARSE_RULE(parse_primary),       nullptr, PREC_NONE };
        m_expr_rules[TokenKind::False] =             { BIND_PARSE_RULE(parse_primary),       nullptr, PREC_NONE };
        m_expr_rules[TokenKind::CharLit] =           { BIND_PARSE_RULE(parse_primary),       nullptr, PREC_NONE };
        m_expr_rules[TokenKind::IntLit] =            { BIND_PARSE_RULE(parse_primary),       nullptr, PREC_NONE };
        m_expr_rules[TokenKind::UIntLit] =           { BIND_PARSE_RULE(parse_primary),       nullptr, PREC_NONE };
        m_expr_rules[TokenKind::LongLit] =           { BIND_PARSE_RULE(parse_primary),       nullptr, PREC_NONE };
        m_expr_rules[TokenKind::ULongLit] =          { BIND_PARSE_RULE(parse_primary),       nullptr, PREC_NONE };
        m_expr_rules[TokenKind::NumLit] =            { BIND_PARSE_RULE(parse_primary),       nullptr, PREC_NONE };
        m_expr_rules[TokenKind::StrLit] =            { BIND_PARSE_RULE(parse_primary),       nullptr, PREC_NONE };
        m_expr_rules[TokenKind::Null] =              { BIND_PARSE_RULE(parse_primary),       nullptr, PREC_NONE };
        m_expr_rules[TokenKind::Identifier] =        { BIND_PARSE_RULE(parse_primary),       nullptr, PREC_NONE };
        m_expr_rules[TokenKind::Self] =              { BIND_PARSE_RULE(parse_primary),       nullptr, PREC_NONE };
        m_expr_rules[TokenKind::Env] =               { BIND_PARSE_RULE(parse_env),           nullptr, PREC_NONE };
        m_expr_rules[TokenKind::Cast] =              { BIND_PARSE_RULE(parse_cast),          nullptr, PREC_NONE };

        m_expr_rules[TokenKind::AtSizeof] =          { BIND_PARSE_RULE(parse_builtin_call),  nullptr, PREC_NONE };
        m_expr_rules[TokenKind::AtTypeid] =          { BIND_PARSE_RULE(parse_builtin_call),  nullptr, PREC_NONE };
        m_expr_rules[TokenKind::AtMemcpy] =          { BIND_PARSE_RULE(parse_builtin_call),  nullptr, PREC_NONE};
        m_expr_rules[TokenKind::AtMemset] =          { BIND_PARSE_RULE(parse_builtin_call),  nullptr, PREC_NONE};

        m_expr_rules[TokenKind::Void] =        { BIND_PARSE_RULE(parse_type_expr), nullptr, PREC_NONE };
        m_expr_rules[TokenKind::Bool] =        { BIND_PARSE_RULE(parse_type_expr), nullptr, PREC_NONE };
        m_expr_rules[TokenKind::Char] =        { BIND_PARSE_RULE(parse_type_expr), nullptr, PREC_NONE };
        m_expr_rules[TokenKind::IChar] =       { BIND_PARSE_RULE(parse_type_expr), nullptr, PREC_NONE };
        m_expr_rules[TokenKind::Short] =       { BIND_PARSE_RULE(parse_type_expr), nullptr, PREC_NONE };
        m_expr_rules[TokenKind::UShort] =      { BIND_PARSE_RULE(parse_type_expr), nullptr, PREC_NONE };
        m_expr_rules[TokenKind::Int] =         { BIND_PARSE_RULE(parse_type_expr), nullptr, PREC_NONE };
        m_expr_rules[TokenKind::UInt] =        { BIND_PARSE_RULE(parse_type_expr), nullptr, PREC_NONE };
        m_expr_rules[TokenKind::Long] =        { BIND_PARSE_RULE(parse_type_expr), nullptr, PREC_NONE };
        m_expr_rules[TokenKind::ULong] =       { BIND_PARSE_RULE(parse_type_expr), nullptr, PREC_NONE };
        m_expr_rules[TokenKind::Sz] =          { BIND_PARSE_RULE(parse_type_expr), nullptr, PREC_NONE };
        m_expr_rules[TokenKind::Isz] =         { BIND_PARSE_RULE(parse_type_expr), nullptr, PREC_NONE };
        m_expr_rules[TokenKind::Float] =       { BIND_PARSE_RULE(parse_type_expr), nullptr, PREC_NONE };
        m_expr_rules[TokenKind::Double] =      { BIND_PARSE_RULE(parse_type_expr), nullptr, PREC_NONE };
        m_expr_rules[TokenKind::TypeInfo] =    { BIND_PARSE_RULE(parse_type_expr), nullptr, PREC_NONE };
        m_expr_rules[TokenKind::Any] =         { BIND_PARSE_RULE(parse_type_expr), nullptr, PREC_NONE };
    }

    void Parser::parse_impl() {
        TinyVector<Stmt*> stmts;
        while (peek()) {
            Decl* d = parse_global();

            if (!m_declared_module) {
                context.report_compiler_diagnostic(peek()->loc, "All translation units must contain a module declaration");
                break;
            }
        }
    }

    Token* Parser::peek(size_t count) {
        // While the count is a size_t, you are still allowed to use -1
        // Even if you pass -1 the actual data underneath is the same
        if (m_index + count < m_tokens.size()) {
            return &m_tokens.at(m_index + count);
        } else {
            return nullptr;
        }
    }

    Token& Parser::consume() {
        ARIA_ASSERT(m_index < m_tokens.size(), "consume out of bounds!");

        return m_tokens.at(m_index++);
    }

    Token* Parser::try_consume(TokenKind kind, const std::string& expect) {
        if (match(kind)) {
            Token& t = consume();
            return &t;
        }

        Token* cur = (peek()) ? peek() : peek(-1);
        error_expected(expect, cur->loc);
        return nullptr;
    }

    bool Parser::match(TokenKind kind) {
        if (!peek()) {
            return false;
        }

        if (peek()->kind == kind) {
            return true;
        } else {
            return false;
        }
    }

    Expr* Parser::parse_grouping(Expr* left) {
        ARIA_ASSERT(left == nullptr, "Parser::parse_grouping() should not have a left side");

        Token* lp = try_consume(TokenKind::LeftParen, "(");
        Expr* child = parse_expression();
        Token* rp = try_consume(TokenKind::RightParen, ")");

        if (!rp) {
            sync_local();
            return &error_expr;
        }

        return Expr::Create(lp->loc + rp->loc, ExprKind::Paren, 
            child->value_kind, child->type,
            ParenExpr(child));
    }

    Expr* Parser::parse_cast(Expr* left) {
        ARIA_ASSERT(left == nullptr, "Parser::parse_cast() should not have a left side");

        Token& c = consume(); // consume "cast"
        try_consume(TokenKind::LeftParen, "(");

        TypeInfo* type = TypeInfo::get_error();

        if (is_type()) {
            type = parse_type();
        } else {
            context.report_compiler_diagnostic(peek()->loc, "Expected a type");
        }

        try_consume(TokenKind::Comma, ",");
        Expr* child = parse_expression();

        if (!try_consume(TokenKind::RightParen, ")")) {
            sync_local();
            return &error_expr;
        }

        return Expr::Create(c.loc + peek(-1)->loc, ExprKind::Cast, 
            ExprValueKind::RValue, type,
            CastExpr(child, type));
    }

    Expr* Parser::parse_call(Expr* left) {
        ARIA_ASSERT(left, "Parser::parse_call() expects a left side");

        Token* lp = try_consume(TokenKind::LeftParen, "(");
        TinyVector<Expr*> args;
        
        while (!match(TokenKind::RightParen)) {
            Expr* val = parse_expression();

            if (!expr_ok(val)) {
                sync_local();
                break;
            }

            args.append(val);
    
            if (match(TokenKind::Comma)) {
                consume();
                continue;
            }

            break;
        }
    
        Token* rp = try_consume(TokenKind::RightParen, ")");
        if (!rp) { return &error_expr; }
    
        return Expr::Create(left->loc + rp->loc, ExprKind::Call,
            ExprValueKind::RValue, nullptr, 
            CallExpr(left, args));
    }

    BuiltinCallKind Parser::get_builtin_call_from_token(Token* token) {
        switch (token->kind) {
            case TokenKind::AtSizeof: return BuiltinCallKind::Sizeof;
            case TokenKind::AtTypeid: return BuiltinCallKind::Typeid;
            case TokenKind::AtMemcpy: return BuiltinCallKind::Memcpy;
            case TokenKind::AtMemset: return BuiltinCallKind::Memset;

            default: ARIA_UNREACHABLE("Invalid builtin call kind");
        }
    }

    UnaryOperatorKind Parser::get_unary_operator_from_token(Token* token) {
        switch (token->kind) {
            case TokenKind::Bang: return UnaryOperatorKind::Not;
            case TokenKind::Minus: return UnaryOperatorKind::Negate;
            case TokenKind::Ampersand: return UnaryOperatorKind::AddressOf;
            case TokenKind::DoubleAmpersand: return UnaryOperatorKind::RValueAddressOf;
            case TokenKind::Star: return UnaryOperatorKind::Dereference;
            case TokenKind::PlusPlus: return UnaryOperatorKind::Increment;
            case TokenKind::MinusMinus: return UnaryOperatorKind::Decrement;
            default: return UnaryOperatorKind::Invalid;
        }
    }

    BinaryOperatorKind Parser::get_binary_operator_from_token(Token* token) {
        switch (token->kind) {
            case TokenKind::Plus: return BinaryOperatorKind::Add;
            case TokenKind::PlusEq: return BinaryOperatorKind::CompoundAdd;
            case TokenKind::Minus: return BinaryOperatorKind::Sub;
            case TokenKind::MinusEq: return BinaryOperatorKind::CompoundSub;
            case TokenKind::Star: return BinaryOperatorKind::Mul;
            case TokenKind::StarEq: return BinaryOperatorKind::CompoundMul;
            case TokenKind::Slash: return BinaryOperatorKind::Div;
            case TokenKind::SlashEq: return BinaryOperatorKind::CompoundDiv;
            case TokenKind::Percent: return BinaryOperatorKind::Mod;
            case TokenKind::PercentEq: return BinaryOperatorKind::CompoundMod;
            case TokenKind::Ampersand: return BinaryOperatorKind::BitAnd;
            case TokenKind::AmpersandEq: return BinaryOperatorKind::CompoundBitAnd;
            case TokenKind::DoubleAmpersand: return BinaryOperatorKind::LogAnd;
            case TokenKind::Pipe: return BinaryOperatorKind::BitOr;
            case TokenKind::PipeEq: return BinaryOperatorKind::CompoundBitOr;
            case TokenKind::DoublePipe: return BinaryOperatorKind::LogOr;
            case TokenKind::UpArrow: return BinaryOperatorKind::BitXor;
            case TokenKind::UpArrowEq: return BinaryOperatorKind::CompoundBitXor;
            case TokenKind::Less: return BinaryOperatorKind::Less;
            case TokenKind::LessEq: return BinaryOperatorKind::LessOrEq;
            case TokenKind::Greater: return BinaryOperatorKind::Greater;
            case TokenKind::GreaterEq: return BinaryOperatorKind::GreaterOrEq;
            case TokenKind::LessLess: return BinaryOperatorKind::Shl;
            case TokenKind::LessLessEq: return BinaryOperatorKind::CompoundShl;
            case TokenKind::GreaterGreater: return BinaryOperatorKind::Shr;
            case TokenKind::GreaterGreaterEq: return BinaryOperatorKind::CompoundShr;
            case TokenKind::Eq: return BinaryOperatorKind::Eq;
            case TokenKind::EqEq: return BinaryOperatorKind::IsEq;
            case TokenKind::BangEq: return BinaryOperatorKind::IsNotEq;
            default: return BinaryOperatorKind::Invalid;
        }
    }

    Expr* Parser::parse_unary(Expr* left) {
        ARIA_ASSERT(left == nullptr, "Parser::parse_unary() should not have a left side");

        Token op = consume();

        // Check for *const, this is very likely a const pointer type
        if (op.kind == TokenKind::Star && match(TokenKind::Const)) {
            consume();
            if (!is_type()) {
                context.report_compiler_diagnostic(peek()->loc, "Expected a type");
            } else {
                TypeInfo* base = parse_type();

                return Expr::Create(op.loc + base->loc, ExprKind::TypeInfo,
                    ExprValueKind::RValue, TypeInfo::create_pointer(base, true, op.loc + base->loc),
                    ErrorExpr());
            }
        }

        Expr* expr = parse_term();
        if (!expr_ok(expr)) { return &error_expr; }

        return Expr::Create(op.loc + expr->loc, ExprKind::UnaryOperator,
            ExprValueKind::RValue, expr->type,
            UnaryOperatorExpr(expr, get_unary_operator_from_token(&op), false));
    }

    Expr* Parser::parse_infix_unary(Expr* left) {
        ARIA_ASSERT(left, "Parser::parse_infix_unary() expects a left side");

        Token op = consume();

        return Expr::Create(left->loc + op.loc, ExprKind::UnaryOperator,
            ExprValueKind::RValue, left->type,
            UnaryOperatorExpr(left, get_unary_operator_from_token(&op), true));
    }

    Expr* Parser::parse_binary(Expr* left) {
        ARIA_ASSERT(left, "Parser::parse_binary() expects a left side");

        Token op = consume();
        Expr* right = nullptr;
        
        if (op.kind == TokenKind::Eq) { right = parse_precedence(PREC_ASSIGNMENT); }
        else { right = parse_precedence(m_expr_rules[op.kind].precedence + 1); }

        if (expr_ok(right)) {
            return Expr::Create(left->loc + right->loc, ExprKind::BinaryOperator,
                ExprValueKind::RValue, nullptr,
                BinaryOperatorExpr(left, right, get_binary_operator_from_token(&op)));
        }
        
        return &error_expr;
    }

    Expr* Parser::parse_array_subscript(Expr* left) {
        ARIA_ASSERT(left, "Parser::parse_array_subscript() expects a left side");

        Token lb = consume(); // consume "["

        if (match(TokenKind::Colon)) {
            consume();
            Expr* len = parse_expression();
            try_consume(TokenKind::RightBracket, "]");

            return Expr::Create(left->loc + peek(-1)->loc, ExprKind::ToSlice,
                ExprValueKind::RValue, nullptr,
                ToSliceExpr(left, len));
        } else if (match(TokenKind::DotDot)) {
            consume();
            try_consume(TokenKind::RightBracket, "]");

            return Expr::Create(left->loc + peek(-1)->loc, ExprKind::ToSlice,
                ExprValueKind::RValue, nullptr,
                ToSliceExpr(left, nullptr));
        } else {
            Expr* index = parse_expression();
            try_consume(TokenKind::RightBracket, "]");

            return Expr::Create(left->loc + peek(-1)->loc, ExprKind::ArraySubscript,
                ExprValueKind::LValue, nullptr,
                ArraySubscriptExpr(left, index));
        }
    }

    Expr* Parser::parse_compound_assignment(Expr* left) {
        ARIA_ASSERT(left, "Parser::parse_compound_assignment() expects a left side");

        Token op = consume();
        Expr* right = parse_precedence(PREC_ASSIGNMENT);

        if (expr_ok(right)) {
            return Expr::Create(left->loc + right->loc, ExprKind::CompoundAssign,
                ExprValueKind::RValue, nullptr,
                BinaryOperatorExpr(left, right, get_binary_operator_from_token(&op)));
        }
        
        return &error_expr;
    }

    Expr* Parser::parse_member(Expr* left) {
        ARIA_ASSERT(left, "Parser::parse_member() expects a left side");

        Token d = consume(); // consume "."
        Token* ident = try_consume(TokenKind::Identifier, "identifier");
        if (!ident) { return &error_expr; }

        return Expr::Create(left->loc + ident->loc, ExprKind::Member,
            ExprValueKind::LValue, nullptr,
            MemberExpr(ident->string, left));
    }

    Expr* Parser::parse_primary(Expr* left) {
        ARIA_ASSERT(left == nullptr, "Parser::parse_primary() should not have a left side");

        Token& t = consume();
        switch (t.kind) {
            case TokenKind::False: {
                return Expr::Create(t.loc, ExprKind::BooleanLiteral, 
                    ExprValueKind::RValue, TypeInfo::get_basic(TypeKind::Bool), 
                    BooleanLiteralExpr(false));
            }

            case TokenKind::True: {
                return Expr::Create(t.loc, ExprKind::BooleanLiteral, 
                    ExprValueKind::RValue, TypeInfo::get_basic(TypeKind::Bool), 
                    BooleanLiteralExpr(true));
            }

            case TokenKind::CharLit: {
                int8_t ch = static_cast<int8_t>(t.integer);
                return Expr::Create(t.loc, ExprKind::CharacterLiteral, 
                    ExprValueKind::RValue, TypeInfo::get_basic(TypeKind::Char), 
                    CharacterLiteralExpr(ch));
            }
    
            case TokenKind::IntLit: {
                return Expr::Create(t.loc, ExprKind::IntegerLiteral,
                    ExprValueKind::RValue, TypeInfo::get_basic(TypeKind::Int), 
                    IntegerLiteralExpr(t.integer));
            }

            case TokenKind::UIntLit: {
                return Expr::Create(t.loc, ExprKind::IntegerLiteral, 
                    ExprValueKind::RValue, TypeInfo::get_basic(TypeKind::UInt), 
                    IntegerLiteralExpr(t.integer));
            }

            case TokenKind::LongLit: {
                return Expr::Create(t.loc, ExprKind::IntegerLiteral,
                    ExprValueKind::RValue, TypeInfo::get_basic(TypeKind::Long), 
                    IntegerLiteralExpr(t.integer));
            }

            case TokenKind::ULongLit: {
                return Expr::Create(t.loc, ExprKind::IntegerLiteral, 
                    ExprValueKind::RValue, TypeInfo::get_basic(TypeKind::ULong), 
                    IntegerLiteralExpr(t.integer));
            }
    
            case TokenKind::NumLit: {
                return Expr::Create(t.loc, ExprKind::FloatingLiteral,
                    ExprValueKind::RValue, TypeInfo::get_basic(TypeKind::Double), 
                    FloatingLiteralExpr(t.number));
            }
    
            case TokenKind::StrLit: {
                scratch_buffer_clear();
                scratch_buffer_append(t.string);

                while (match(TokenKind::StrLit)) {
                    Token& news = consume();
                    scratch_buffer_append(news.string);
                }

                return Expr::Create(t.loc, ExprKind::StringLiteral,
                    ExprValueKind::RValue, nullptr, 
                    StringLiteralExpr(scratch_buffer_to_str()));
            }

            case TokenKind::Null: {
                return Expr::Create(t.loc, ExprKind::Null,
                    ExprValueKind::RValue, TypeInfo::get_void_ptr(), ErrorExpr());
            }

            case TokenKind::Self: {
                return Expr::Create(t.loc, ExprKind::Self, ExprValueKind::LValue, nullptr, ErrorExpr());
            }

            case TokenKind::Identifier: {
                return parse_identifier(t);
            }

            default: return &error_expr;
        }
    }

    Expr* Parser::parse_type_expr(Expr* left) {
        ARIA_ASSERT(left == nullptr, "Parser::parse_type_expr() should not have a left side");

        TypeInfo* t = parse_type();
        return Expr::Create(t->loc, ExprKind::TypeInfo, ExprValueKind::CValue, t, ErrorExpr());
    }

    Expr* Parser::parse_identifier(Token t) {
        Specifier* specifier = nullptr;
        TinyVector<TypeInfo*> generic_args;
        Token ident = t;

        while (match(TokenKind::ColonColon)) {
            consume();

            if (match(TokenKind::Less)) {
                consume();
            
                while (!match(TokenKind::Greater)) {
                    if (is_type()) {
                        generic_args.append(parse_type());
                    } else {
                        context.report_compiler_diagnostic(peek()->loc, "Expected a type");
                    }
            
                    if (match(TokenKind::Comma)) { consume(); continue; }
                    else if (match(TokenKind::Greater)) { break; }
            
                    context.report_compiler_diagnostic(peek()->loc, "Expected ',' or '>'");
                    consume();
                }
            
                try_consume(TokenKind::Greater, ">");
                break;
            }

            Token* c = try_consume(TokenKind::Identifier, "identifier");
            if (!c) { return &error_expr; }

            if (specifier) {
                Specifier* child = Specifier::Create(ident.loc, SpecifierKind::Name, NameSpecifier(ident.string));
                child->name.parent = specifier;
                specifier = child;
                ident = *c;
            } else {
                specifier = Specifier::Create(t.loc, SpecifierKind::Name, NameSpecifier(t.string));
                ident = *c;
            }

            if (match(TokenKind::ColonColon)) {
                continue;
            } else {
                break;
            }
        }

        return Expr::Create(ident.loc, ExprKind::DeclRef,
                       ExprValueKind::LValue, nullptr, 
                       DeclRefExpr(ident.string, specifier, generic_args));
    }

    Expr* Parser::parse_array_literal(Expr* left) {
        ARIA_ASSERT(left == nullptr, "Parser::parse_array_literal() should not have a left side");

        Token& lb = consume(); // consume "["
        TinyVector<Expr*> args;

        while (!match(TokenKind::RightBracket)) {
            Expr* val = parse_expression();

            if (!expr_ok(val)) {
                sync_local();
                break;
            }

            args.append(val);
    
            if (match(TokenKind::Comma)) {
                consume();
                continue;
            }

            break;
        }

        try_consume(TokenKind::RightBracket, "]");

        return Expr::Create(lb.loc + peek(-1)->loc, ExprKind::ArrayLiteral,
            ExprValueKind::RValue, nullptr,
            ArrayLiteralExpr(args));
    }

    Expr* Parser::parse_env(Expr* left) {
        ARIA_ASSERT(left == nullptr, "Parser::parse_env() should not have a left side");

        Token& e = consume(); // consume "env"

        if (!try_consume(TokenKind::ColonColon, "::")) { return &error_expr; }
        Token* ident = try_consume(TokenKind::Identifier, "identifier");
        if (!ident) { return &error_expr; }

        std::unordered_map<std::string_view, bool> env;
        env["WIN32"] = context.opts->triple.isOSWindows();
        env["LINUX"] = context.opts->triple.isOSLinux();
        env["X86"] = context.opts->triple.getArch() == llvm::Triple::x86;
        env["X86_64"] = context.opts->triple.getArch() == llvm::Triple::x86_64;

        if (env.contains(ident->string)) {
            return Expr::Create(e.loc + ident->loc, ExprKind::BooleanLiteral, 
                    ExprValueKind::RValue, TypeInfo::get_basic(TypeKind::Bool), 
                    BooleanLiteralExpr(env.at(ident->string)));
        }

        context.report_compiler_diagnostic(ident->loc, fmt::format("Unknown environment '{}'", ident->string));
        return &error_expr;
    }

    Expr* Parser::parse_builtin_call(Expr* left) {
        ARIA_ASSERT(left == nullptr, "Parser::parse_builtin_call() should not have a left side");

        Token& op = consume(); // consume op
        TinyVector<Expr*> args;
        try_consume(TokenKind::LeftParen, "(");

        while (!match(TokenKind::RightParen)) {
            Expr* val = parse_expression();

            if (!expr_ok(val)) {
                sync_local();
                break;
            }

            args.append(val);
    
            if (match(TokenKind::Comma)) {
                consume();
                continue;
            }

            break;
        }

        try_consume(TokenKind::RightParen, ")");

        BuiltinCallKind kind = get_builtin_call_from_token(&op);

        return Expr::Create(op.loc + peek(-1)->loc, ExprKind::BuiltinCall,
            ExprValueKind::RValue, nullptr,
            BuiltinCallExpr(kind, args));
    }

    Expr* Parser::parse_precedence_with_left(Expr* left, size_t precedence) {
        ARIA_ASSERT(left, "Parser::parse_precedence_with_left() expects a left side");

        while (true) {
            Token* tok = peek();
            if (!tok) { break; }
            size_t tokPrecedence = m_expr_rules[tok->kind].precedence;

            if (precedence > tokPrecedence) { break; }

            ParseExprFn infixRule = m_expr_rules[tok->kind].infix;

            if (!infixRule) {
                sync_local();
                context.report_compiler_diagnostic(tok->loc, fmt::format("'{}' cannot appear here, did you forget something before the operator?", token_kind_to_string(tok->kind)));
                return &error_expr;
            }

            left = infixRule(left);

            if (!expr_ok(left)) {
                sync_local();
                return &error_expr;
            }
        }

        return left;
    }

    Expr* Parser::parse_precedence(size_t precedence) {
        Token* tok = peek();
        if (!tok) { return &error_expr; }
        ParseExprFn prefixRule = m_expr_rules[tok->kind].prefix;

        if (!prefixRule) {
            sync_local();
            context.report_compiler_diagnostic(tok->loc, "Expected an expression");
            return &error_expr;
        }

        Expr* left = prefixRule(nullptr);
        if (!expr_ok(left)) { return &error_expr; }

        return parse_precedence_with_left(left, precedence);
    }

    Expr* Parser::parse_expression() {
        return parse_precedence(PREC_ASSIGNMENT);
    }

    Expr* Parser::parse_term() {
        return parse_precedence(PREC_CALL);
    }

    bool Parser::is_expression() {
        if (!peek()) { return false; }
        ParseExprFn prefixRule = m_expr_rules[peek()->kind].prefix;

        if (!prefixRule) { return false; }
        return true;
    }

    bool Parser::is_primitive_type() {
        if (!peek()) { return false; }

        Token type = *peek();

        switch (type.kind) {
            case TokenKind::Void:
            case TokenKind::Bool:
            case TokenKind::Char:
            case TokenKind::IChar:
            case TokenKind::Short:
            case TokenKind::UShort:
            case TokenKind::Int:
            case TokenKind::UInt:
            case TokenKind::Long:
            case TokenKind::ULong:
            case TokenKind::Sz:
            case TokenKind::Isz:
            case TokenKind::Float:
            case TokenKind::Double:
            case TokenKind::TypeInfo:
            case TokenKind::Any:
            case TokenKind::Fn:
            case TokenKind::AtTypeof: return true;
            default: return false;
        }

        return false;
    }

    bool Parser::is_type() {
        if (!peek()) { return false; }
        if (is_primitive_type() || match(TokenKind::Identifier)) { return true; }

        if (match(TokenKind::Star) || match(TokenKind::LeftBracket)) { return true; }
        return false;
    }

    TypeInfo* Parser::parse_type() {
        ARIA_ASSERT(is_type(), "Cannot parse a type out of a non type");

        TypeInfo* type = TypeInfo::create_basic(TypeKind::Error);
        type->loc = peek()->loc;

        switch (peek()->kind) {
            case TokenKind::LeftBracket: {
                consume();
                if (is_expression()) {
                    Expr* e = parse_expression();
                    try_consume(TokenKind::RightBracket, "]");
                    type->array = ArrayType(parse_type(), e);
                    type->kind = TypeKind::Array;
                } else {
                    try_consume(TokenKind::RightBracket, "]");

                    if (is_type()) {
                        type->slice.base = parse_type();
                        type->kind = TypeKind::Slice;
                    } else {
                        error_expected("type", peek()->loc);
                    }
                }
        
                break;
            }
        
            case TokenKind::Star: {
                consume();

                bool is_const = false;
                if (match(TokenKind::Const)) {
                    consume();
                    is_const = true;
                }
        
                if (is_type()) {
                    type->pointer = PointerType(parse_type(), is_const);
                    type->kind = TypeKind::Pointer;
                } else {
                    error_expected("type", peek()->loc);
                }
        
                break;
            }
        
            case TokenKind::Void:       consume(); type->kind = TypeKind::Void; break;
        
            case TokenKind::Bool:       consume(); type->kind = TypeKind::Bool; break;
        
            case TokenKind::Char:       consume(); type->kind = TypeKind::Char; break;
            case TokenKind::IChar:      consume(); type->kind = TypeKind::IChar; break;
            case TokenKind::Short:      consume(); type->kind = TypeKind::Short; break;
            case TokenKind::UShort:     consume(); type->kind = TypeKind::UShort; break;
            case TokenKind::Int:        consume(); type->kind = TypeKind::Int; break;
            case TokenKind::UInt:       consume(); type->kind = TypeKind::UInt; break;
            case TokenKind::Long:       consume(); type->kind = TypeKind::Long; break;
            case TokenKind::ULong:      consume(); type->kind = TypeKind::ULong; break;

            case TokenKind::Sz:         consume(); type->kind = TypeKind::Sz; break;
            case TokenKind::Isz:        consume(); type->kind = TypeKind::Isz; break;
        
            case TokenKind::Float:      consume(); type->kind = TypeKind::Float; break;
            case TokenKind::Double:     consume(); type->kind = TypeKind::Double; break;

            case TokenKind::TypeInfo:   consume(); type->kind = TypeKind::TypeInfo; break;
            case TokenKind::Any:        consume(); type->kind = TypeKind::Any; break;

            case TokenKind::Fn: {
                consume();

                TypeInfo* ret_type = TypeInfo::get_void();

                VariadicKind var = VariadicKind::None;
                auto[_, param_types] = parse_function_params(&var);

                if (match(TokenKind::Arrow)) {
                    consume();

                    if (!is_type()) {
                        context.report_compiler_diagnostic(peek()->loc, "Expected a type after '->'");
                    } else {
                        ret_type = parse_type();
                    }
                }

                type->kind = TypeKind::Function;
                type->function = FunctionType(ret_type, param_types, var);
                break;
            }

            case TokenKind::AtTypeof: {
                consume();

                try_consume(TokenKind::LeftParen, "(");
                Expr* expr = parse_expression();
                try_consume(TokenKind::RightParen, ")");

                type->loc += peek(-1)->loc;
                type->kind = TypeKind::Typeof;
                type->typeof = TypeofType(expr);
                break;
            }
        
            case TokenKind::Identifier: {
                consume();
                Expr* ident = parse_identifier(*peek(-1));
                type->kind = TypeKind::Unresolved;
                type->unresolved = UnresolvedType(ident);
        
                if (match(TokenKind::Less)) {
                    consume();
                    // try_consume(TokenKind::LeftParen, "(");
                    TinyVector<TypeInfo*> args;
        
                    while (peek()) {
                        if (!is_type()) {
                            context.report_compiler_diagnostic(peek()->loc, "Expected a type");
                            break;
                        }
        
                        args.append(parse_type());
        
                        if (match(TokenKind::Comma)) { consume(); continue; }
                        else if (match(TokenKind::Greater)) { break; }
                        else if (match(TokenKind::GreaterGreater)) {
                            split_token();
                            break;
                        }
        
                        context.report_compiler_diagnostic(peek()->loc, "Expected ',' or '>'");
                    }
        
                    try_consume(TokenKind::Greater, ">");
        
                    type->generic_instantiation = GenericInstantiationType(TypeInfo::dup(type), args);
                    type->kind = TypeKind::GenericInstantiation;
        
                    type->loc += peek(-1)->loc;
                }
        
                break;
            }
        
            default: break;
        }

        if (match(TokenKind::Star)) {
            context.report_compiler_diagnostic_with_notes(peek()->loc, "Unexpected token '*' after type",
                { "Did you mean to put '*' before the type?" });

            consume();
            type->pointer.base = TypeInfo::dup(type);
            type->pointer.is_const = false;
            type->kind = TypeKind::Pointer;
        }

        if (match(TokenKind::LeftBracket)) {
            context.report_compiler_diagnostic_with_notes(peek()->loc, "Unexpected token '[' after type",
                { "Did you mean to put '[' before the type?" });

            consume();
            if (is_expression()) {
                type->array = ArrayType(TypeInfo::dup(type), parse_expression());
                type->kind = TypeKind::Array;
            } else {
                type->slice.base = TypeInfo::dup(type);
                type->kind = TypeKind::Slice;
            }

            try_consume(TokenKind::RightBracket, "]");
        }

        return type;
    }

    Stmt* Parser::parse_block() {
        TinyVector<Stmt*> stmts;
        Token* l = try_consume(TokenKind::LeftCurly, "{");
        if (!l) { return &error_stmt; }

        while (peek() && !match(TokenKind::RightCurly)) {
            Stmt* stmt = parse_statement();

            if (stmt != nullptr) {
                stmts.append(stmt);
            }
        }

        try_consume(TokenKind::RightCurly, "}");

        return Stmt::Create(l->loc, StmtKind::Block, BlockStmt(stmts));
    }

    Stmt* Parser::parse_block_inline() {
        if (match(TokenKind::LeftCurly)) {
            return parse_block();
        } else {
            Stmt* stmt = parse_statement();
            if (!stmt) { return &error_stmt; }

            TinyVector<Stmt*> stmts;
            stmts.append(stmt);

            return Stmt::Create(stmt->loc, StmtKind::Block, BlockStmt(stmts));
        }
    }

    Stmt* Parser::parse_while() {
        Token w = consume(); // consume "while"

        Expr* condition = parse_expression();
        Stmt* body = parse_block_inline();

        return Stmt::Create(w.loc + condition->loc, StmtKind::While, WhileStmt(condition, body));
    }

    Stmt* Parser::parse_do_while() {
        Token d = consume(); // consume "do"

        Stmt* body = parse_block_inline();
        try_consume(TokenKind::While, "while");
        Expr* condition = parse_expression();
        try_consume(TokenKind::Semi, ";");
        
        return Stmt::Create(d.loc, StmtKind::DoWhile, DoWhileStmt(condition, body));
    }

    Stmt* Parser::parse_for() {
        Token f = consume(); // consume "for"
        
        // We still try to keep parsing if we are missing a '('
        // It could be quite likely that the user just forgot it
        try_consume(TokenKind::LeftParen, "(");
        
        Decl* prologue = nullptr;
        Expr* condition = nullptr;
        Expr* step = nullptr;
        Stmt* body = nullptr;
        SourceLoc end_loc;

        if (match(TokenKind::Semi)) {
            consume();
        } else {
            prologue = parse_variable_decl(false, false);
            if (!decl_ok(prologue)) {
                sync_local();

                if (match(TokenKind::Semi)) {
                    consume();
                }
            }
        }

        if (match(TokenKind::Semi)) {
            consume();
        } else {
            condition = parse_expression();
            try_consume(TokenKind::Semi, "';'");
        }
        
        if (match(TokenKind::RightParen)) {
            Token& rp = consume();
            end_loc = rp.loc;
        } else {
            step = parse_expression();
            if (step) { step->result_discarded = true; }
            if (Token* rp = try_consume(TokenKind::RightParen, "')'")) {
                end_loc = rp->loc;
            } else {
                end_loc = step->loc;
            }
        }
        
        body = parse_block_inline();
        
        return Stmt::Create(f.loc + end_loc, StmtKind::For, ForStmt(prologue, condition, step, body));
    }

    Stmt* Parser::parse_if() {
        Token i = consume(); // consume "if"

        Expr* condition = parse_expression();
        Stmt* body = parse_block_inline();
        Stmt* else_body = nullptr;

        if (match(TokenKind::Else)) {
            consume();
            else_body = parse_block_inline();
        }
        
        return Stmt::Create(i.loc + condition->loc, StmtKind::If, IfStmt(condition, body, else_body));
    }

    Stmt* Parser::parse_break() {
        Token& b = consume(); // consume "break"
        try_consume(TokenKind::Semi, ";");
        return Stmt::Create(b.loc, StmtKind::Break, ErrorStmt());
    }

    Stmt* Parser::parse_continue() {
        Token& b = consume(); // consume "continue"
        try_consume(TokenKind::Semi, ";");
        return Stmt::Create(b.loc, StmtKind::Continue, ErrorStmt());
    }

    Stmt* Parser::parse_return() {
        Token r = consume(); // consume "return"

        Expr* val = nullptr;

        if (!match(TokenKind::Semi)) {
            val = parse_expression();
        }

        try_consume(TokenKind::Semi, ";");
        return Stmt::Create(r.loc + peek(-1)->loc, StmtKind::Return, ReturnStmt(val));
    }

    Stmt* Parser::parse_defer() {
        Token& d = consume(); // consume "defer"

        Stmt* stmt = parse_statement();
        return Stmt::Create(d.loc + peek(-1)->loc, StmtKind::Defer, DeferStmt(stmt));
    }

    Stmt* Parser::parse_expression_statement() {
        Expr* expr = parse_expression();
        if (!expr_ok(expr)) { return &error_stmt; }
        try_consume(TokenKind::Semi, ";");
        expr->result_discarded = true;
        
        return Stmt::Create(expr->loc + peek(-1)->loc, StmtKind::Expr, expr);
    }

    Stmt* Parser::parse_statement() {
        if (!peek()) {
            return nullptr;
        }

        switch (peek()->kind) {
            case TokenKind::Semi: {
                Token& tok = consume();
                return Stmt::Create(tok.loc, StmtKind::Nop, ErrorStmt());
            }

            case TokenKind::LeftParen:
            case TokenKind::Minus:
            case TokenKind::Ampersand:
            case TokenKind::Less:
            case TokenKind::Star:
            case TokenKind::Bang:
            case TokenKind::True:
            case TokenKind::False:
            case TokenKind::CharLit:
            case TokenKind::IntLit:
            case TokenKind::UIntLit:
            case TokenKind::LongLit:
            case TokenKind::ULongLit:
            case TokenKind::NumLit:
            case TokenKind::StrLit:
            case TokenKind::Identifier:
            case TokenKind::Cast:
            case TokenKind::Self:
            case TokenKind::Void:
            case TokenKind::Bool:
            case TokenKind::Char:
            case TokenKind::IChar:
            case TokenKind::Short:
            case TokenKind::UShort:
            case TokenKind::Int:
            case TokenKind::UInt:
            case TokenKind::Long:
            case TokenKind::ULong:
            case TokenKind::Sz:
            case TokenKind::Isz:
            case TokenKind::Float:
            case TokenKind::Double:
            case TokenKind::TypeInfo:
            case TokenKind::Any:
            case TokenKind::AtSizeof:
            case TokenKind::AtTypeid:
            case TokenKind::AtMemcpy:
            case TokenKind::AtMemset:
                return parse_expression_statement();

            case TokenKind::LeftCurly:
                return parse_block();

            case TokenKind::If:
                return parse_if();

            case TokenKind::While:
                return parse_while();

            case TokenKind::Do:
                return parse_do_while();

            case TokenKind::For:
                return parse_for();

            case TokenKind::Break:
                return parse_break();

            case TokenKind::Continue:
                return parse_continue();

            case TokenKind::Return:
                return parse_return();

            case TokenKind::Defer:
                return parse_defer();

            case TokenKind::Const: {
                Decl* d = parse_const_decl(false);
                if (!decl_ok(d)) { return &error_stmt; }
                return Stmt::Create(d->loc, StmtKind::Decl, d);
            }

            case TokenKind::Let: {
                Decl* d = parse_let_decl();
                if (!decl_ok(d)) { return &error_stmt; }
                return Stmt::Create(d->loc, StmtKind::Decl, d);
            }

            case TokenKind::Fn: {
                Token& tok = consume();
                context.report_compiler_diagnostic(tok.loc, fmt::format("Function declaration is not allowed here"));
                return &error_stmt;
            }

            case TokenKind::Struct: {
                Token& tok = consume();
                context.report_compiler_diagnostic(tok.loc, fmt::format("Structure declaration is not allowed here"));
                return &error_stmt;
            }

            case TokenKind::Plus:
            case TokenKind::PlusEq:
            case TokenKind::MinusEq:
            case TokenKind::StarEq:
            case TokenKind::Slash:
            case TokenKind::SlashEq:
            case TokenKind::Percent:
            case TokenKind::PercentEq:
            case TokenKind::AmpersandEq:
            case TokenKind::DoubleAmpersand:
            case TokenKind::Pipe:
            case TokenKind::PipeEq:
            case TokenKind::DoublePipe:
            case TokenKind::UpArrow:
            case TokenKind::UpArrowEq:
            case TokenKind::LessLess:
            case TokenKind::LessLessEq:
            case TokenKind::GreaterGreater:
            case TokenKind::GreaterGreaterEq:
            case TokenKind::Eq:
            case TokenKind::EqEq:
            case TokenKind::BangEq:
            case TokenKind::LessEq:
            case TokenKind::Greater:
            case TokenKind::GreaterEq: {
                Token& tok = consume();
                context.report_compiler_diagnostic(tok.loc, fmt::format("Unexpected binary operator '{}' while looking for statement", token_kind_to_string(tok.kind)));
                return &error_stmt;
            }
            case TokenKind::Module:
            case TokenKind::Import:
            case TokenKind::Else:
            case TokenKind::Extern:
            case TokenKind::Arrow:
            case TokenKind::RightParen:
            case TokenKind::LeftBracket:
            case TokenKind::RightBracket:
            case TokenKind::RightCurly:
            case TokenKind::Comma:
            case TokenKind::Colon:
            case TokenKind::ColonColon:
            case TokenKind::Dot:
            case TokenKind::TripleDot: {
                Token& tok = consume();
                context.report_compiler_diagnostic(tok.loc, fmt::format("Unexpected token '{}' while looking for statement", token_kind_to_string(tok.kind)));
                return &error_stmt;
            }

            case TokenKind::Last: ARIA_UNREACHABLE("Invalid token kind");
        }

        ARIA_UNREACHABLE("Should never be reached");
        return nullptr;
    }

    Decl* Parser::parse_module_decl() {
        Token& mod = consume(); // consume "module"
        
        std::string_view path = parse_module_path();
        TinyVector<DeclAttribute> attrs = parse_decl_attributes(DeclKind::Module);

        try_consume(TokenKind::Semi, ";");

        if (m_declared_module) {
            CompilationUnit* unit = new CompilationUnit(context.active_comp_unit->filename, context.active_comp_unit->source, context.active_comp_unit->is_stdlib);
            context.compilation_units.push_back(unit);
            context.active_comp_unit = unit;
        }

        m_declared_module = true;

        Module* module = context.find_or_create_module(path);
        context.active_comp_unit->parent = module;
        module->units.push_back(context.active_comp_unit);

        for (auto& attr : attrs) {
            if (attr.kind == DeclAttributeKind::If) {
                context.active_comp_unit->if_attr = attr.expr;
            }
        }

        return Decl::Create(mod.loc + peek(-1)->loc, DeclKind::Module, DeclVisibility::Public, ModuleDecl(path));
    }

    Decl* Parser::parse_import_decl() {
        Token& imp = consume(); // consume "import"

        std::string_view path = parse_module_path();
        std::string_view alias;

        if (match(TokenKind::As)) {
            consume();
            alias = parse_module_path();
        }

        try_consume(TokenKind::Semi, ";");

        Decl* import = Decl::Create(imp.loc + peek(-1)->loc, DeclKind::Import, DeclVisibility::Public, ImportDecl(path, alias));
        context.active_comp_unit->imports.push_back(import);
        return import;
    }

    Decl* Parser::parse_let_decl() {
        Token& l = consume(); // consume "let"
        return parse_variable_decl(false, false);
    }

    Decl* Parser::parse_const_decl(bool global) {
        Token& c = consume(); // consume "const"
        return parse_variable_decl(global, true);
    }

    Decl* Parser::parse_variable_decl(bool global, bool const_, LinkageKind linkage) {
        SourceLoc loc = peek()->loc;

        Token* ident = try_consume(TokenKind::Identifier, "identifier");
        if (!ident) { return &error_decl; }
        TypeInfo* type = nullptr;
        Expr* initializer = nullptr;

        if (match(TokenKind::Colon)) {
            try_consume(TokenKind::Colon, ":");

            if (is_type()) {
                type = parse_type();
            } else {
                context.report_compiler_diagnostic(peek()->loc, "Expected a type after ':'");
                type = TypeInfo::get_error();
            }
        }

        if (match(TokenKind::Eq)) {
            consume();
            initializer = parse_expression();
        } else if (const_) {
            context.report_compiler_diagnostic(ident->loc, "No initializer provided for const variable declaration");
            type = TypeInfo::get_error();
            return &error_decl;
        } else if (!type) {
            context.report_compiler_diagnostic(ident->loc, "No initializer provided for type-inffered variable declaration");
            type = TypeInfo::get_error();

            return &error_decl;
        }

        try_consume(TokenKind::Semi, ";");

        Decl* decl = Decl::Create(ident->loc + peek(-1)->loc, DeclKind::Var, global ? m_current_visibility : DeclVisibility::Public, VarDecl(ident->string, type, initializer, global, const_, linkage));

        if (global) {
            context.active_comp_unit->globals.push_back(decl);
        }

        return decl;
    }

    Decl* Parser::parse_function_decl(LinkageKind linkage) {
        SourceLoc loc = peek()->loc;
        Token fn = consume(); // consume "fn"
        TypeInfo* ret_type = TypeInfo::get_void();

        if (is_primitive_type()) {
            context.report_compiler_diagnostic(peek()->loc, "Expected an indentifier but got a type (NOTE: function declarations look like: fn name() -> type {...})");
            return &error_decl;
        }

        Token* ident = try_consume(TokenKind::Identifier, "identifier");
        if (!ident) {
            sync_local();
            return &error_decl;
        }

        TinyVector<Decl*> generic_params;
        if (match(TokenKind::Less)) {
            generic_params = parse_generic_params();
        }

        VariadicKind variadic = VariadicKind::None;
        auto[params, param_types] = parse_function_params(&variadic);

        if (match(TokenKind::Arrow)) {
            consume();

            if (!is_type()) {
                context.report_compiler_diagnostic(peek()->loc, "Expected a type after '->'");
                sync_local();
            } else {
                ret_type = parse_type();
            }
        }

        TinyVector<DeclAttribute> attrs = parse_decl_attributes(DeclKind::Function);
        
        Stmt* body = nullptr;

        SourceLoc end_loc = peek(-1)->loc;

        if (match(TokenKind::LeftCurly)) {
            body = parse_block();
        } else {
            try_consume(TokenKind::Semi, ";");
        }

        TypeInfo* final_type = TypeInfo::create_function(TypeKind::Function, ret_type, param_types, variadic);

        Decl* f = Decl::Create(ident->loc + end_loc, DeclKind::Function, m_current_visibility, FunctionDecl(ident->string, final_type, params, body, linkage));
        f->attributes = attrs;

        if (generic_params.size == 0) {
            context.active_comp_unit->funcs.push_back(f);
            return f;
        } else {
            Decl* g = Decl::Create(f->loc, DeclKind::Generic, m_current_visibility, GenericDecl(generic_params, f));
            context.active_comp_unit->generics.push_back(g);
            return g;
        }
    }

    std::pair<TinyVector<Decl*>, TinyVector<TypeInfo*>> Parser::parse_function_params(VariadicKind* variadic) {
        TinyVector<Decl*> params;
        TinyVector<TypeInfo*> param_types;

        try_consume(TokenKind::LeftParen, "(");

        while (!match(TokenKind::RightParen)) {
            if (is_primitive_type()) {
                context.report_compiler_diagnostic(peek()->loc, "Expected an identifier but got a type (<name>: <type>)");
                sync_params();
                continue;
            }

            if (match(TokenKind::TripleDot)) {
                Token& triple = consume();
                *variadic = VariadicKind::Unnamed;

                if (match(TokenKind::Comma)) {
                    Token& c = consume();
                    context.report_compiler_diagnostic(c.loc, "Cannot declare parameters after '...'");
                } else if (match(TokenKind::RightParen)) {
                    break;
                } else {
                    context.report_compiler_diagnostic(peek()->loc, "Expected ')'");
                    sync_params();
                }
            } else {
                Token* param_ident = try_consume(TokenKind::Identifier, "identifier");
            
                if (!param_ident) {
                    sync_params();
                    continue;
                }

                if (match(TokenKind::TripleDot)) {
                    Token& triple = consume();
                    *variadic = VariadicKind::Named;

                    TypeInfo* t = TypeInfo::get_basic(TypeKind::Any);

                    params.append(Decl::Create(param_ident->loc, DeclKind::Param, DeclVisibility::Public, ParamDecl(param_ident->string, t, true)));
                    param_types.append(t);

                    if (match(TokenKind::Comma)) {
                        Token& c = consume();
                        context.report_compiler_diagnostic(c.loc, "Cannot declare parameters after '...'");
                        continue;
                    } else if (match(TokenKind::RightParen)) {
                        break;
                    } else {
                        context.report_compiler_diagnostic(peek()->loc, "Expected ')'");
                        sync_params();
                        continue;
                    }
                }

                try_consume(TokenKind::Colon, ":");

                if (!is_type()) {
                    context.report_compiler_diagnostic(peek()->loc, "Expected a type");
                    sync_params();
                    continue;
                }

                TypeInfo* paramType = parse_type();
                
                params.append(Decl::Create(param_ident->loc, DeclKind::Param, DeclVisibility::Public, ParamDecl(param_ident->string, paramType, false)));
                param_types.append(paramType);

                if (match(TokenKind::Comma)) { consume(); continue; }
                if (match(TokenKind::RightParen)) { break; }

                context.report_compiler_diagnostic(peek()->loc, "Expected either ',' or ')'");
                sync_params();
            }
        }

        try_consume(TokenKind::RightParen, ")");

        return { params, param_types };
    }

    TinyVector<Decl*> Parser::parse_generic_params() {
        TinyVector<Decl*> params;

        try_consume(TokenKind::Less, "<");
        if (match(TokenKind::Greater)) {
            Token& g = consume();
            context.report_compiler_diagnostic(g.loc, "Empty generic parameter list is not allowed");
            return params;
        }

        bool variadic = false;
        while (peek() && !match(TokenKind::Greater)) {
            Token* ident = try_consume(TokenKind::Identifier, "identifier");
            if (!ident) { break; }
            SourceLoc loc = ident->loc;

            if (match(TokenKind::TripleDot)) {
                Token& t = consume();
                variadic = true;
                loc += t.loc;
            }

            params.append(Decl::Create(ident->loc, DeclKind::GenericParameter, DeclVisibility::Public, GenericParameterDecl(ident->string, variadic)));

            if (match(TokenKind::Comma)) {
                Token& c = consume();

                if (variadic) {
                    context.report_compiler_diagnostic(c.loc, "Cannot declare more generic parameters after a variadic parameter");
                }
                continue;
            }
            if (match(TokenKind::Greater)) { break; }

            context.report_compiler_diagnostic(peek()->loc, "Expected either ',' or '>'");
            sync_params();
        }

        try_consume(TokenKind::Greater, ">");
        return params;
    }

    Decl* Parser::parse_struct_decl() {
        Token s = consume(); // consume "struct"
        Token* ident = try_consume(TokenKind::Identifier, "identifier");
        if (!ident) { return &error_decl; }
        
        Decl* struc = Decl::Create(s.loc + ident->loc, DeclKind::Struct, m_current_visibility, StructDecl(ident->string, {}));
        TinyVector<Decl*> generic_params;
        DeclVisibility visibility = DeclVisibility::Public;

        if (match(TokenKind::Less)) {
            generic_params = parse_generic_params();
        }

        try_consume(TokenKind::LeftCurly, "{");
        while (!match(TokenKind::RightCurly)) {
            SourceLoc loc = peek()->loc;

            if (match(TokenKind::Identifier)) {
                Token* fieldName = try_consume(TokenKind::Identifier, "identifier");
                if (!fieldName) { sync_local(); continue; }

                try_consume(TokenKind::Colon, ":");

                if (!is_type()) {
                    context.report_compiler_diagnostic(peek()->loc, "Expected type after ':'");
                    sync_local();
                    continue;
                }

                TypeInfo* type = parse_type();

                if (!try_consume(TokenKind::Semi, ";")) {
                    sync_local();
                    if (match(TokenKind::Semi)) { consume(); }
                }

                struc->struct_.fields.append(Decl::Create(fieldName->loc + peek(-1)->loc, DeclKind::Field, visibility, FieldDecl(fieldName->string, type)));
            } else if (match(TokenKind::HashPrivate)) {
                context.report_compiler_diagnostic(loc + peek()->loc, "Structs do not support private fields");
                consume();
            } else {
                context.report_compiler_diagnostic(loc, "Expected identifier");
                sync_local();
                if (match(TokenKind::Semi)) { consume(); }
            }
        }
        try_consume(TokenKind::RightCurly, "}");
        
        if (generic_params.size == 0) {
            context.active_comp_unit->structs.push_back(struc);
            return struc;
        } else {
            Decl* g = Decl::Create(struc->loc, DeclKind::Generic, m_current_visibility, GenericDecl(generic_params, struc));
            context.active_comp_unit->generics.push_back(g);
            return g;
        }
    }

    Decl* Parser::parse_impl_decl() {
        Token s = consume(); // consume "impl"
        Token* ident = try_consume(TokenKind::Identifier, "identifier");
        if (!ident) { return &error_decl; }
        
        Decl* impl = Decl::Create(s.loc + ident->loc, DeclKind::Impl, m_current_visibility, ImplDecl(ident->string, {}));
        DeclVisibility visibility = DeclVisibility::Public;

        try_consume(TokenKind::LeftCurly, "{");
        while (peek() && !match(TokenKind::RightCurly)) {
            SourceLoc loc = peek()->loc;

            if (match(TokenKind::Identifier)) {
                context.report_compiler_diagnostic(loc, "Unexpected identifier found, did you mean to put 'fn' before it?");
                sync_local();
            } else if (match(TokenKind::Fn)) {
                consume();
                SourceLoc end_loc;

                Token* name = try_consume(TokenKind::Identifier, "identifier");
                if (!name) { sync_local(); continue; }
                
                VariadicKind variadic = VariadicKind::None;
                auto[params, param_types] = parse_function_params(&variadic);
                end_loc = peek(-1)->loc;

                TypeInfo* ret_type = TypeInfo::get_void();

                if (match(TokenKind::Arrow)) {
                    Token& a =consume();

                    if (!is_type()) {
                        context.report_compiler_diagnostic(peek()->loc, "Expected a type after '->'");
                        sync_local();
                        ret_type = TypeInfo::get_error();
                        end_loc = a.loc;
                    } else {
                        ret_type = parse_type();
                        end_loc = ret_type->loc;
                    }
                }

                Stmt* body = parse_block();

                TypeInfo* final_type = TypeInfo::create_function(TypeKind::Method, ret_type, param_types, variadic);
                impl->impl.fields.append(Decl::Create(loc + end_loc, DeclKind::Method,
                    visibility, MethodDecl(impl, name->string, final_type, params, body)));
            } else if (match(TokenKind::HashPrivate)) {
                context.report_compiler_diagnostic(loc + peek()->loc, "Impls do not support private fields");
                consume();
            } else {
                context.report_compiler_diagnostic(loc, "Expected 'fn' or 'static'");
                sync_local();
                if (match(TokenKind::Semi)) { consume(); }
            }
        }
        try_consume(TokenKind::RightCurly, "}");
        
        context.active_comp_unit->impls.push_back(impl);
        return impl;
    }

    Decl* Parser::parse_extern_decl() {
        Token& ext = consume(); // consume "extern"

        switch (peek()->kind) {
            case TokenKind::Fn: return parse_function_decl(LinkageKind::Extern);

            default: {
                Token& tok = consume();
                context.report_compiler_diagnostic(tok.loc, "Expected 'fn'");
                sync_global();
                return &error_decl;
            }
        }
    }

    Decl* Parser::parse_static_decl() {
        Token& sta = consume(); // consume "static"

        switch (peek()->kind) {
            case TokenKind::Identifier: return parse_variable_decl(true, false, LinkageKind::Static);

            default: {
                Token& tok = consume();
                context.report_compiler_diagnostic(tok.loc, "Expected 'identifier'");
                sync_global();
                return &error_decl;
            }
        }
    }

    Decl* Parser::parse_typedef_decl() {
        Token& td = consume(); // consume "typedef"

        if (is_type()) {
            TypeInfo* type = parse_type();
            try_consume(TokenKind::As, "as");

            Token* name = try_consume(TokenKind::Identifier, "identifier");
            if (!name) {
                sync_global();
                return &error_decl;
            }

            TinyVector<DeclAttribute> attrs = parse_decl_attributes(DeclKind::Typedef);

            try_consume(TokenKind::Semi, ";");

            Decl* d = Decl::Create(td.loc + peek(-1)->loc, DeclKind::Typedef, m_current_visibility, TypedefDecl(type, name->string));
            d->attributes = attrs;
            context.active_comp_unit->typedefs.push_back(d);
            return d;
        }

        error_expected("type", peek()->loc);
        return &error_decl;
    }

    Decl* Parser::parse_enum_decl() {
        Token& enu = consume(); // consume "enum"

        Token* ident = try_consume(TokenKind::Identifier, "identifier");
        if (!ident) { return &error_decl; }

        TinyVector<Decl*> fields;
        try_consume(TokenKind::LeftCurly, "{");
        while (peek() && !match(TokenKind::RightCurly)) {
            Token* fd_ident = try_consume(TokenKind::Identifier, "identifier");
            Expr* value = nullptr;

            if (match(TokenKind::Eq)) {
                consume();
                value = parse_expression();
            }

            SourceLoc loc = value ? fd_ident->loc + value->loc : fd_ident->loc;
            fields.append(Decl::Create(loc, DeclKind::EnumConstant, DeclVisibility::Public, EnumConstantDecl(fd_ident->string, value)));

            if (match(TokenKind::Comma)) { consume(); continue; }
            else if (match(TokenKind::RightCurly)) { break; }

            context.report_compiler_diagnostic(peek()->loc, "Expected either ',' or '}'");
            sync_local();
        }

        try_consume(TokenKind::RightCurly, "}");

        Decl* d = Decl::Create(enu.loc + ident->loc, DeclKind::Enum, m_current_visibility, EnumDecl(fields, ident->string));
        context.active_comp_unit->enums.push_back(d);
        return d;
    }

    TinyVector<DeclAttribute> Parser::parse_decl_attributes(DeclKind kind) {
        TinyVector<DeclAttribute> attrs;

        while (peek()) {
            switch (peek()->kind) {
                case TokenKind::AtIf: {
                    consume();
                    try_consume(TokenKind::LeftParen, "(");
                    Expr* arg = parse_expression();
                    try_consume(TokenKind::RightParen, ")");

                    attrs.append(DeclAttribute(DeclAttributeKind::If, arg));
                    break;
                }

                case TokenKind::AtBuiltin: {
                    consume();
                    try_consume(TokenKind::LeftParen, "(");

                    std::string_view arg;

                    Token* str = try_consume(TokenKind::StrLit, "string literal");
                    if (str) { arg = str->string;}

                    try_consume(TokenKind::RightParen, ")");

                    attrs.append(DeclAttribute(DeclAttributeKind::Builtin, arg));
                    break;
                }

                case TokenKind::AtNoreturn: {
                    if (kind != DeclKind::Function) {
                        context.report_compiler_diagnostic(peek()->loc, "Cannot use '@noreturn' with this declaration");
                    }

                    consume();
                    attrs.append(DeclAttribute(DeclAttributeKind::Noreturn));
                    break;
                }

                default: return attrs;
            }
        }

        return attrs;
    }

    std::string_view Parser::parse_module_path() {
        scratch_buffer_clear();

        for (;;) {
            Token* tok = try_consume(TokenKind::Identifier, "identifier");
            if (!tok) { break; }
            scratch_buffer_append(tok->string);

            if (match(TokenKind::ColonColon)) {
                consume();
                scratch_buffer_append("::");
                continue;
            }

            break;
        }

        return scratch_buffer_to_str();
    }

    Decl* Parser::parse_global() {
        switch (peek()->kind) {
            case TokenKind::Module:
                return parse_module_decl();

            case TokenKind::Import:
                return parse_import_decl();

            case TokenKind::Const:
                return parse_const_decl(true);

            case TokenKind::Let: {
                context.report_compiler_diagnostic(peek()->loc, "'let' cannot be used for global variables, try using 'const' or 'static' instead"); 
                consume(); 
                return &error_decl;
            }

            case TokenKind::Fn:
                return parse_function_decl(LinkageKind::None);

            case TokenKind::Struct:
                return parse_struct_decl();

            case TokenKind::Impl:
                return parse_impl_decl();

            case TokenKind::Typedef:
                return parse_typedef_decl();

            case TokenKind::Enum:
                return parse_enum_decl();

            case TokenKind::Extern:
                return parse_extern_decl();

            case TokenKind::Static:
                return parse_static_decl();

            case TokenKind::Semi: {
                context.report_compiler_diagnostic(peek()->loc, fmt::format("Token ';' was unexpected, try removing it")); 
                consume(); 
                return &error_decl;
            }

            case TokenKind::HashPrivate: {
                consume();
                m_current_visibility = DeclVisibility::Private;
                return &error_decl;
            }

            case TokenKind::Void:
            case TokenKind::Bool:
            case TokenKind::Char:
            case TokenKind::IChar:
            case TokenKind::Short:
            case TokenKind::UShort:
            case TokenKind::Int:
            case TokenKind::UInt:
            case TokenKind::Long:
            case TokenKind::ULong:
            case TokenKind::Float:
            case TokenKind::Double: {
                Token& t = consume();
                context.report_compiler_diagnostic(t.loc, fmt::format("Unexpected type '{}', did you mean to declare a global variable? (let <name>: <type>)", token_kind_to_string(t.kind)));
                return &error_decl;
            }

            default: {
                context.report_compiler_diagnostic(peek()->loc, fmt::format("Expected the start of a global declaration", token_kind_to_string(peek()->kind)));
                sync_global();
                return &error_decl;
            }
        }
    }

    void Parser::split_token() {
        ARIA_ASSERT(peek()->kind == TokenKind::GreaterGreater, "Invalid token split");

        SourceLoc loc = peek()->loc;
        loc.len = 1;
        loc.col++;
        loc.offset++;
        m_tokens.at(m_index).kind = TokenKind::Greater;
        m_tokens.at(m_index).loc.len--;
        m_tokens.insert(m_tokens.begin() + m_index + 1, { TokenKind::Greater, loc, ">" });
    }

    void Parser::sync_global() {
        consume();

        while (peek()) {
            TokenKind kind = peek()->kind;

            if (kind == TokenKind::Fn || kind == TokenKind::Struct || is_type()) {
                return;
            }

            consume();
        }
    }

    void Parser::sync_local() {
        if (peek()) { consume(); }
        else { return; }

        while (peek()) {
            TokenKind type = peek()->kind;

            if (type == TokenKind::Semi || type == TokenKind::LeftCurly || type == TokenKind::Comma) {
                return;
            }

            consume();
        }
    }

    void Parser::sync_params() {
        consume();

        while (peek()) {
            TokenKind kind = peek()->kind;

            if (kind == TokenKind::Comma || kind == TokenKind::RightParen) {
                return;
            }

            consume();
        }
    }

    void Parser::error_expected(const std::string& expect, SourceLoc loc) {
        if (peek()) {
            context.report_compiler_diagnostic(loc, fmt::format("Expected '{}' but got '{}'", expect, token_kind_to_string(peek()->kind)));
        } else {
            context.report_compiler_diagnostic(loc, fmt::format("Expected '{}' but got EOF", expect));
        }
    }

    bool Parser::stmt_ok(Stmt* stmt) {
        if (stmt == nullptr) { return true; }
        if (stmt->kind == StmtKind::Error) { return false; }

        return true;
    }

    bool Parser::expr_ok(Expr* expr) {
        if (expr == nullptr) { return true; }
        if (expr->kind == ExprKind::Error) { return false; }

        return true;
    }

    bool Parser::decl_ok(Decl* decl) {
        if (decl == nullptr) { return true; }
        if (decl->kind == DeclKind::Error) { return false; }

        return true;
    }

} // namespace ariac
