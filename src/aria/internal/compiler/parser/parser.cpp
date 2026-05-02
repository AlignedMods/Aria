#include "aria/internal/compiler/parser/parser.hpp"

#include "fmt/format.h"

#include <string>
#include <charconv>
#include <limits>

#define BIND_PARSE_RULE(fn) [this](Expr* expr) -> Expr* { return fn(expr); }

static constexpr size_t PREC_NONE = 0;
static constexpr size_t PREC_ASSIGNMENT = 10;
static constexpr size_t PREC_RELATIONAL = 20;
static constexpr size_t PREC_ADDITIVE = 20;
static constexpr size_t PREC_BIT = 40;
static constexpr size_t PREC_MULTIPLICATIVE = 50;
static constexpr size_t PREC_CALL = 60;

namespace Aria::Internal {

    Parser::Parser(CompilationContext* ctx) {
        m_Context = ctx;
        m_Tokens = ctx->ActiveCompUnit->Tokens;

        if (error_expr.Type == nullptr) { error_expr.Type = &error_type; }

        add_expr_rules();
        parse_impl();
    }

    void Parser::add_expr_rules() {
        m_ExprRules.reserve(static_cast<size_t>(TokenKind::Last));

        m_ExprRules[TokenKind::LeftParen] =         { BIND_PARSE_RULE(parse_grouping), BIND_PARSE_RULE(parse_call), PREC_CALL };
        m_ExprRules[TokenKind::Plus] =              { nullptr, BIND_PARSE_RULE(parse_binary), PREC_ADDITIVE };
        m_ExprRules[TokenKind::Minus] =             { BIND_PARSE_RULE(parse_unary), BIND_PARSE_RULE(parse_binary), PREC_ADDITIVE };
        m_ExprRules[TokenKind::Star] =              { BIND_PARSE_RULE(parse_unary), BIND_PARSE_RULE(parse_binary), PREC_MULTIPLICATIVE };
        m_ExprRules[TokenKind::Slash] =             { nullptr, BIND_PARSE_RULE(parse_binary), PREC_MULTIPLICATIVE };
        m_ExprRules[TokenKind::Percent] =           { nullptr, BIND_PARSE_RULE(parse_binary), PREC_MULTIPLICATIVE };
        m_ExprRules[TokenKind::Ampersand] =         { BIND_PARSE_RULE(parse_unary), BIND_PARSE_RULE(parse_binary), PREC_BIT };
        m_ExprRules[TokenKind::DoubleAmpersand] =   { nullptr, BIND_PARSE_RULE(parse_binary), PREC_RELATIONAL };
        m_ExprRules[TokenKind::Pipe] =              { nullptr, BIND_PARSE_RULE(parse_binary), PREC_BIT };
        m_ExprRules[TokenKind::DoublePipe] =        { nullptr, BIND_PARSE_RULE(parse_binary), PREC_RELATIONAL };
        m_ExprRules[TokenKind::UpArrow] =           { nullptr, BIND_PARSE_RULE(parse_binary), PREC_BIT };
        m_ExprRules[TokenKind::EqEq] =              { nullptr, BIND_PARSE_RULE(parse_binary), PREC_RELATIONAL };
        m_ExprRules[TokenKind::BangEq] =            { nullptr, BIND_PARSE_RULE(parse_binary), PREC_RELATIONAL };
        m_ExprRules[TokenKind::Less] =              { BIND_PARSE_RULE(parse_cast), BIND_PARSE_RULE(parse_binary), PREC_RELATIONAL };
        m_ExprRules[TokenKind::LessEq] =            { nullptr, BIND_PARSE_RULE(parse_binary), PREC_RELATIONAL };
        m_ExprRules[TokenKind::Greater] =           { nullptr, BIND_PARSE_RULE(parse_binary), PREC_RELATIONAL };
        m_ExprRules[TokenKind::GreaterEq] =         { nullptr, BIND_PARSE_RULE(parse_binary), PREC_RELATIONAL };
        m_ExprRules[TokenKind::LessLess] =          { nullptr, BIND_PARSE_RULE(parse_binary), PREC_BIT };
        m_ExprRules[TokenKind::GreaterGreater] =    { nullptr, BIND_PARSE_RULE(parse_binary), PREC_BIT };
        m_ExprRules[TokenKind::LeftBracket] =       { nullptr, BIND_PARSE_RULE(parse_array_subscript), PREC_BIT };
                                                    
        m_ExprRules[TokenKind::Eq] =                { nullptr, BIND_PARSE_RULE(parse_binary), PREC_ASSIGNMENT };
        m_ExprRules[TokenKind::PlusEq] =            { nullptr, BIND_PARSE_RULE(parse_compound_assignment), PREC_ASSIGNMENT };
        m_ExprRules[TokenKind::MinusEq] =           { nullptr, BIND_PARSE_RULE(parse_compound_assignment), PREC_ASSIGNMENT };
        m_ExprRules[TokenKind::StarEq] =            { nullptr, BIND_PARSE_RULE(parse_compound_assignment), PREC_ASSIGNMENT };
        m_ExprRules[TokenKind::SlashEq] =           { nullptr, BIND_PARSE_RULE(parse_compound_assignment), PREC_ASSIGNMENT };
        m_ExprRules[TokenKind::PercentEq] =         { nullptr, BIND_PARSE_RULE(parse_compound_assignment), PREC_ASSIGNMENT };
        m_ExprRules[TokenKind::AmpersandEq] =       { nullptr, BIND_PARSE_RULE(parse_compound_assignment), PREC_ASSIGNMENT };
        m_ExprRules[TokenKind::PipeEq] =            { nullptr, BIND_PARSE_RULE(parse_compound_assignment), PREC_ASSIGNMENT };
        m_ExprRules[TokenKind::UpArrowEq] =         { nullptr, BIND_PARSE_RULE(parse_compound_assignment), PREC_ASSIGNMENT };
        m_ExprRules[TokenKind::LessLessEq] =        { nullptr, BIND_PARSE_RULE(parse_compound_assignment), PREC_ASSIGNMENT };
        m_ExprRules[TokenKind::GreaterGreaterEq] =  { nullptr, BIND_PARSE_RULE(parse_compound_assignment), PREC_ASSIGNMENT };

        m_ExprRules[TokenKind::Dot] =               { nullptr, BIND_PARSE_RULE(parse_member), PREC_CALL };
                                                    
        m_ExprRules[TokenKind::Self] =              { BIND_PARSE_RULE(parse_primary), nullptr, PREC_NONE };
        m_ExprRules[TokenKind::True] =              { BIND_PARSE_RULE(parse_primary), nullptr, PREC_NONE };
        m_ExprRules[TokenKind::False] =             { BIND_PARSE_RULE(parse_primary), nullptr, PREC_NONE };
        m_ExprRules[TokenKind::CharLit] =           { BIND_PARSE_RULE(parse_primary), nullptr, PREC_NONE };
        m_ExprRules[TokenKind::IntLit] =            { BIND_PARSE_RULE(parse_primary), nullptr, PREC_NONE };
        m_ExprRules[TokenKind::UIntLit] =           { BIND_PARSE_RULE(parse_primary), nullptr, PREC_NONE };
        m_ExprRules[TokenKind::LongLit] =           { BIND_PARSE_RULE(parse_primary), nullptr, PREC_NONE };
        m_ExprRules[TokenKind::ULongLit] =          { BIND_PARSE_RULE(parse_primary), nullptr, PREC_NONE };
        m_ExprRules[TokenKind::NumLit] =            { BIND_PARSE_RULE(parse_primary), nullptr, PREC_NONE };
        m_ExprRules[TokenKind::StrLit] =            { BIND_PARSE_RULE(parse_primary), nullptr, PREC_NONE };
        m_ExprRules[TokenKind::Null] =              { BIND_PARSE_RULE(parse_primary), nullptr, PREC_NONE };
        m_ExprRules[TokenKind::Identifier] =        { BIND_PARSE_RULE(parse_primary), nullptr, PREC_NONE };

        m_ExprRules[TokenKind::New] =               { BIND_PARSE_RULE(parse_new), nullptr, PREC_NONE };
        m_ExprRules[TokenKind::Delete] =            { BIND_PARSE_RULE(parse_delete), nullptr, PREC_NONE };

        m_ExprRules[TokenKind::DollarFormat] =      { BIND_PARSE_RULE(parse_format),  nullptr, PREC_NONE };
    }

    void Parser::parse_impl() {
        TinyVector<Stmt*> stmts;
        while (peek()) {
            Stmt* stmt = parse_global();

            if (!m_DeclaredModule) {
                m_Context->report_compiler_diagnostic(peek()->Range.Start, peek()->Range, "All translation units must contain a module declaration");
                break;
            }

            if (stmt_ok(stmt)) {
                stmts.append(m_Context, stmt);
            }
        }

        TranslationUnitDecl root(stmts);
        Decl* decl = Decl::Create(m_Context, SourceLocation(), SourceRange(), DeclKind::TranslationUnit, root);
        m_Context->ActiveCompUnit->RootASTNode = Stmt::Create(m_Context, SourceLocation(), SourceRange(), StmtKind::Decl, decl);
    }

    Token* Parser::peek(size_t count) {
        // While the count is a size_t, you are still allowed to use -1
        // Even if you pass -1 the actual data underneath is the same
        if (m_Index + count < m_Tokens.size()) {
            return &m_Tokens.at(m_Index + count);
        } else {
            return nullptr;
        }
    }

    Token& Parser::consume() {
        ARIA_ASSERT(m_Index < m_Tokens.size(), "consume out of bounds!");

        return m_Tokens.at(m_Index++);
    }

    Token* Parser::try_consume(TokenKind kind, const std::string& expect) {
        if (match(kind)) {
            Token& t = consume();
            return &t;
        }

        Token* cur = (peek()) ? peek() : peek(-1);
        error_expected(expect, cur->Range.Start, cur->Range);
        return nullptr;
    }

    bool Parser::match(TokenKind kind) {
        if (!peek()) {
            return false;
        }

        if (peek()->Kind == kind) {
            return true;
        } else {
            return false;
        }
    }

    Expr* Parser::parse_grouping(Expr* left) {
        ARIA_ASSERT(left == nullptr, "Parser::parse_grouping() should not have a left side");

        Token* lp = try_consume(TokenKind::LeftParen, "(");

        if (is_primitive_type()) {
            SourceLocation typeStart = peek()->Range.Start;
            parse_type();
            m_Context->report_compiler_diagnostic(typeStart, SourceRange(typeStart, peek(-1)->Range.End), "Unexpected type found, did you mean to perform a cast? (<<type>> <expr>)");
            sync_local();

            return &error_expr;
        }

        Expr* child = parse_expression();
        Token* rp = try_consume(TokenKind::RightParen, ")");

        if (!rp) {
            sync_local();
            return &error_expr;
        }

        return Expr::Create(m_Context, lp->Range.Start, SourceRange(lp->Range.Start, rp->Range.End), ExprKind::Paren, 
            child->ValueKind, child->Type,
            ParenExpr(child));
    }

    Expr* Parser::parse_cast(Expr* left) {
        ARIA_ASSERT(left == nullptr, "Parser::parse_cast() should not have a left side");

        Token* lp = try_consume(TokenKind::Less, "<");
        TypeInfo* type = nullptr;

        if (is_primitive_type() || match(TokenKind::Identifier)) {
            type = parse_type();
        } else {
            m_Context->report_compiler_diagnostic(peek()->Range.Start, peek()->Range, "Expected a type");
            type = &error_type;
        }

        if (!try_consume(TokenKind::Greater, ">")) {
            sync_local();
            return &error_expr;
        }

        Expr* child = parse_expression();

        return Expr::Create(m_Context, lp->Range.Start, SourceRange(lp->Range.Start, child->Range.End), ExprKind::Cast, 
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

            args.append(m_Context, val);
    
            if (match(TokenKind::Comma)) {
                consume();
                continue;
            }

            break;
        }
    
        Token* rp = try_consume(TokenKind::RightParen, ")");
        if (!rp) { return &error_expr; }
    
        return Expr::Create(m_Context, lp->Range.Start, SourceRange(left->Range.Start, rp->Range.End), ExprKind::Call,
            ExprValueKind::RValue, nullptr, 
            CallExpr(left, args));
    }

    UnaryOperatorKind Parser::get_unary_operator_from_token(Token* token) {
        switch (token->Kind) {
            case TokenKind::Minus: return UnaryOperatorKind::Negate;
            case TokenKind::Ampersand: return UnaryOperatorKind::AddressOf;
            case TokenKind::Star: return UnaryOperatorKind::Dereference;
            default: return UnaryOperatorKind::Invalid;
        }
    }

    BinaryOperatorKind Parser::get_binary_operator_from_token(Token* token) {
        switch (token->Kind) {
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

        Token* tok = peek();
        if (!tok) {
            m_Context->report_compiler_diagnostic(op.Range.Start, op.Range, "Expected an expression");
            return &error_expr;
        }
        ParseExprFn prefixRule = m_ExprRules[tok->Kind].Prefix;

        if (!prefixRule) {
            sync_local();
            m_Context->report_compiler_diagnostic(tok->Range.Start, tok->Range, "Expected an expression");
            return &error_expr;
        }

        Expr* expr = prefixRule(nullptr);

        if (!expr_ok(expr)) { return &error_expr; }

        return Expr::Create(m_Context, op.Range.Start, SourceRange(op.Range.Start, expr->Range.End), ExprKind::UnaryOperator,
            ExprValueKind::RValue, expr->Type,
            UnaryOperatorExpr(expr, get_unary_operator_from_token(&op)));
    }

    Expr* Parser::parse_binary(Expr* left) {
        ARIA_ASSERT(left, "Parser::parse_binary() expects a left side");

        Token op = consume();
        Expr* right = nullptr;
        
        if (op.Kind == TokenKind::Eq) { right = parse_precedence(PREC_ASSIGNMENT); }
        else { right = parse_precedence(m_ExprRules[op.Kind].Precedence + 1); }

        if (expr_ok(right)) {
            return Expr::Create(m_Context, op.Range.Start, SourceRange(left->Range.Start, right->Range.End), ExprKind::BinaryOperator,
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

            return Expr::Create(m_Context, lb.Range.Start, SourceRange(left->Range.Start, peek(-1)->Range.End), ExprKind::ToSlice,
                ExprValueKind::RValue, nullptr,
                ArraySubscriptExpr(left, len));
        } else {
            Expr* index = parse_expression();
            try_consume(TokenKind::RightBracket, "]");

            return Expr::Create(m_Context, lb.Range.Start, SourceRange(left->Range.Start, peek(-1)->Range.End), ExprKind::ArraySubscript,
                ExprValueKind::LValue, nullptr,
                ArraySubscriptExpr(left, index));
        }
    }

    Expr* Parser::parse_compound_assignment(Expr* left) {
        ARIA_ASSERT(left, "Parser::parse_compound_assignment() expects a left side");

        Token op = consume();
        Expr* right = parse_precedence(PREC_ASSIGNMENT);

        if (expr_ok(right)) {
            return Expr::Create(m_Context, op.Range.Start, SourceRange(left->Range.Start, right->Range.End), ExprKind::CompoundAssign,
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

        return Expr::Create(m_Context, d.Range.Start, SourceRange(left->Range.Start, ident->Range.End), ExprKind::Member,
            ExprValueKind::LValue, nullptr,
            MemberExpr(ident->String, left));
    }

    Expr* Parser::parse_primary(Expr* left) {
        ARIA_ASSERT(left == nullptr, "Parser::parse_primary() should not have a left side");

        Token& t = consume();
        switch (t.Kind) {
            case TokenKind::False: {
                return Expr::Create(m_Context, t.Range.Start, t.Range, ExprKind::BooleanConstant, 
                    ExprValueKind::RValue, &bool_type, 
                    BooleanConstantExpr(false));
            }

            case TokenKind::True: {
                return Expr::Create(m_Context, t.Range.Start, t.Range, ExprKind::BooleanConstant, 
                    ExprValueKind::RValue, &bool_type, 
                    BooleanConstantExpr(true));
            }

            case TokenKind::CharLit: {
                int8_t ch = static_cast<int8_t>(t.Integer);
                return Expr::Create(m_Context, t.Range.Start, t.Range, ExprKind::CharacterConstant, 
                    ExprValueKind::RValue, &char_type, 
                    CharacterConstantExpr(ch));
            }
    
            case TokenKind::IntLit: {
                return Expr::Create(m_Context, t.Range.Start, t.Range, ExprKind::IntegerConstant,
                    ExprValueKind::RValue, &int_type, 
                    IntegerConstantExpr(t.Integer));
            }

            case TokenKind::UIntLit: {
                return Expr::Create(m_Context, t.Range.Start, t.Range, ExprKind::IntegerConstant, 
                    ExprValueKind::RValue, &uint_type, 
                    IntegerConstantExpr(t.Integer));
            }

            case TokenKind::LongLit: {
                return Expr::Create(m_Context, t.Range.Start, t.Range, ExprKind::IntegerConstant,
                    ExprValueKind::RValue, &long_type, 
                    IntegerConstantExpr(t.Integer));
            }

            case TokenKind::ULongLit: {
                return Expr::Create(m_Context, t.Range.Start, t.Range, ExprKind::IntegerConstant, 
                    ExprValueKind::RValue, &ulong_type, 
                    IntegerConstantExpr(t.Integer));
            }
    
            case TokenKind::NumLit: {
                return Expr::Create(m_Context, t.Range.Start, t.Range, ExprKind::FloatingConstant,
                    ExprValueKind::RValue, &double_type, 
                    FloatingConstantExpr(t.Number));
            }
    
            case TokenKind::StrLit: {
                return Expr::Create(m_Context, t.Range.Start, t.Range, ExprKind::StringConstant,
                    ExprValueKind::RValue, &char_slice_type, 
                    StringConstantExpr(t.String));
            }

            case TokenKind::Null: {
                return Expr::Create(m_Context, t.Range.Start, t.Range, ExprKind::Null,
                    ExprValueKind::RValue, &void_ptr_type, ErrorExpr());
            }

            case TokenKind::Identifier: {
                return parse_identifier(t);
            }

            default: return &error_expr;
        }
    }

    Expr* Parser::parse_identifier(Token t) {
        if (match(TokenKind::ColonColon)) {
            Token& col = consume();

            Token* child = try_consume(TokenKind::Identifier, "identifier");
            if (!child) { return &error_expr; }

            Specifier* specifier = Specifier::Create(m_Context, col.Range.Start, SourceRange(t.Range.Start, col.Range.End), SpecifierKind::Scope, ScopeSpecifier(t.String));

            Expr* declRef = Expr::Create(m_Context, child->Range.Start, child->Range, ExprKind::DeclRef,
                                ExprValueKind::LValue, nullptr,
                                DeclRefExpr(child->String, specifier));

            return declRef;
        } else {
            return Expr::Create(m_Context, t.Range.Start, t.Range, ExprKind::DeclRef,
                       ExprValueKind::LValue, nullptr, 
                       DeclRefExpr(t.String, nullptr));
        }

        ARIA_UNREACHABLE();
    }

    Expr* Parser::parse_new(Expr* left) {
        ARIA_ASSERT(left == nullptr, "Parser::parse_new() should not have a left side");

        Token& n = consume(); // consume "new"
        TypeInfo* type = nullptr;
        Expr* initializer = nullptr;

        if (is_primitive_type() || match(TokenKind::Identifier)) {
            type = parse_type();
        } else {
            m_Context->report_compiler_diagnostic(peek()->Range.Start, peek()->Range, "Expected a type");
            type = &error_type;
        }

        type->Base = TypeInfo::Dup(m_Context, type);
        type->Kind = TypeKind::Ptr;

        // parse_type() will handle array types
        // However we do not want this, so we remove the array from the type and add it to the new expression itself
        bool array = type->Base->Kind == TypeKind::Array;

        if (array) {
            initializer = type->Base->Array.Expression;
            type->Base = type->Base->Array.Type;
        } else if (match(TokenKind::LeftParen)) {
            consume();

            if (is_expression()) {
                initializer = parse_expression();
            }

            try_consume(TokenKind::RightParen, ")");
        } else {
            m_Context->report_compiler_diagnostic(peek()->Range.Start, peek()->Range, "Expected '(' or '['");
        }

        return Expr::Create(m_Context, n.Range.Start, SourceRange(n.Range.Start, peek(-1)->Range.End), ExprKind::New,
            ExprValueKind::RValue, type,
            NewExpr(initializer, array));
    }

    Expr* Parser::parse_delete(Expr* left) {
        ARIA_ASSERT(left == nullptr, "Parser::parse_delete() should not have a left side");

        Token& d = consume(); // consume "delete"
        Expr* expr = parse_expression();

        if (!expr) {
            return &error_expr;
        }

        return Expr::Create(m_Context, d.Range.Start, SourceRange(d.Range.Start, peek(-1)->Range.End), ExprKind::Delete,
            ExprValueKind::RValue, &void_type,
            DeleteExpr(expr));
    }

    Expr* Parser::parse_format(Expr* left) {
        ARIA_ASSERT(left == nullptr, "Parser::parse_format() should not have a left side");

        Token& f = consume(); // consume "$format"

        Token* lp = try_consume(TokenKind::LeftParen, "(");
        TinyVector<Expr*> args;

        while (!match(TokenKind::RightParen)) {
            Expr* val = parse_expression();

            if (!expr_ok(val)) {
                sync_local();
                break;
            }

            args.append(m_Context, val);
    
            if (match(TokenKind::Comma)) {
                consume();
                continue;
            }

            break;
        }
    
        Token* rp = try_consume(TokenKind::RightParen, ")");
        if (!rp) { return &error_expr; }
    
        return Expr::Create(m_Context, f.Range.Start, SourceRange(f.Range.Start, rp->Range.End), ExprKind::Format,
            ExprValueKind::RValue, &string_type, 
            FormatExpr(args));
    }

    Expr* Parser::parse_precedence_with_left(Expr* left, size_t precedence) {
        ARIA_ASSERT(left, "Parser::parse_precedence_with_left() expects a left side");

        while (true) {
            Token* tok = peek();
            if (!tok) { break; }
            size_t tokPrecedence = m_ExprRules[tok->Kind].Precedence;

            if (precedence > tokPrecedence) { break; }

            ParseExprFn infixRule = m_ExprRules[tok->Kind].Infix;

            if (!infixRule) {
                sync_local();
                m_Context->report_compiler_diagnostic(tok->Range.Start, tok->Range, fmt::format("'{}' cannot appear here, did you forget something before the operator?", TokenKindToString(tok->Kind)));
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
        ParseExprFn prefixRule = m_ExprRules[tok->Kind].Prefix;

        if (!prefixRule) {
            sync_local();
            m_Context->report_compiler_diagnostic(tok->Range.Start, tok->Range, "Expected an expression");
            return &error_expr;
        }

        Expr* left = prefixRule(nullptr);
        if (!expr_ok(left)) { return &error_expr; }

        return parse_precedence_with_left(left, precedence);
    }

    Expr* Parser::parse_expression() {
        return parse_precedence(PREC_ASSIGNMENT);
    }

    bool Parser::is_expression() {
        if (!peek()) { return false; }
        ParseExprFn prefixRule = m_ExprRules[peek()->Kind].Prefix;

        if (!prefixRule) { return false; }
        return true;
    }

    bool Parser::is_primitive_type() {
        if (!peek()) { return false; }

        Token type = *peek();

        switch (type.Kind) {
            case TokenKind::Void:
            case TokenKind::Bool:
            case TokenKind::Char:
            case TokenKind::UChar:
            case TokenKind::Short:
            case TokenKind::UShort:
            case TokenKind::Int:
            case TokenKind::UInt:
            case TokenKind::Long:
            case TokenKind::ULong:
            case TokenKind::Float:
            case TokenKind::Double:
            case TokenKind::String: return true;
            default: return false;
        }

        return false;
    }

    TypeInfo* Parser::parse_type() {
        ARIA_ASSERT(is_primitive_type() || match(TokenKind::Identifier), "Cannot parse a type out of a non type");

        TypeInfo* type = TypeInfo::Create(m_Context, TypeKind::Error, false);

        switch (consume().Kind) {
            case TokenKind::Void:       type->Kind = TypeKind::Void; break;

            case TokenKind::Bool:       type->Kind = TypeKind::Bool; break;

            case TokenKind::Char:       type->Kind = TypeKind::Char; break;
            case TokenKind::UChar:      type->Kind = TypeKind::UChar; break;
            case TokenKind::Short:      type->Kind = TypeKind::Short; break;
            case TokenKind::UShort:     type->Kind = TypeKind::UShort; break;
            case TokenKind::Int:        type->Kind = TypeKind::Int; break;
            case TokenKind::UInt:       type->Kind = TypeKind::UInt; break;
            case TokenKind::Long:       type->Kind = TypeKind::Long; break;
            case TokenKind::ULong:      type->Kind = TypeKind::ULong; break;

            case TokenKind::Float:      type->Kind = TypeKind::Float; break;
            case TokenKind::Double:     type->Kind = TypeKind::Double; break;

            case TokenKind::String:     type->Kind = TypeKind::String; break;

            case TokenKind::Identifier: {
                Expr* ident = parse_identifier(*peek(-1));
                type->Kind = TypeKind::Unresolved;
                type->Unresolved = UnresolvedType(ident);
                break;
            }

            default: ARIA_UNREACHABLE(); break;
        }

        if (match(TokenKind::LeftBracket)) {
            consume();

            if (is_expression()) {
                type->Array = ArrayDeclaration(TypeInfo::Dup(m_Context, type), parse_expression());
                type->Kind = TypeKind::Array;
            } else {
                type->Base = TypeInfo::Dup(m_Context, type);
                type->Kind = TypeKind::Slice;
            }

            try_consume(TokenKind::RightBracket, "]");
        }

        while (match(TokenKind::Star)) {
            consume();
            type->Base = TypeInfo::Dup(m_Context, type);
            type->Kind = TypeKind::Ptr;
        }

        if (match(TokenKind::Ampersand)) {
            consume();
            type->Reference = true;
        }

        return type;
    }

    Stmt* Parser::parse_block(bool unsafe) {
        TinyVector<Stmt*> stmts;
        Token* l = try_consume(TokenKind::LeftCurly, "'{'");
        if (!l) { return nullptr; }

        while (!match(TokenKind::RightCurly)) {
            Stmt* stmt = parse_statement();

            if (stmt != nullptr) {
                stmts.append(m_Context, stmt);
            }
        }

        Token* r = try_consume(TokenKind::RightCurly, "'}'");
        if (!r) { return nullptr; }

        return Stmt::Create(m_Context, l->Range.Start, SourceRange(l->Range.Start, r->Range.End), StmtKind::Block, BlockStmt(stmts, unsafe));
    }

    Stmt* Parser::parse_block_inline(bool unsafe) {
        if (match(TokenKind::LeftCurly)) {
            return parse_block(unsafe);
        } else {
            Stmt* stmt = parse_statement();
            if (!stmt) { return nullptr; }

            TinyVector<Stmt*> stmts;
            stmts.append(m_Context, stmt);

            return Stmt::Create(m_Context, stmt->Loc, stmt->Range, StmtKind::Block, BlockStmt(stmts, unsafe));
        }
    }

    Stmt* Parser::parse_while() {
        Token w = consume(); // consume "while"

        Expr* condition = parse_expression();
        Stmt* body = parse_block_inline();

        return Stmt::Create(m_Context, w.Range.Start, SourceRange(w.Range.Start, peek(-1)->Range.End), StmtKind::While, WhileStmt(condition, body));
    }

    Stmt* Parser::parse_do_while() {
        Token d = consume(); // consume "do"

        Stmt* body = parse_block_inline();
        try_consume(TokenKind::While, "while");
        Expr* condition = parse_expression();
        
        return Stmt::Create(m_Context, d.Range.Start, SourceRange(d.Range.Start, peek(-1)->Range.End), StmtKind::DoWhile, DoWhileStmt(condition, body));
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

        if (match(TokenKind::Semi)) {
            consume();
        } else {
            prologue = parse_variable_decl(false);
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
            consume();
        } else {
            step = parse_expression();
            if (step) { step->ResultDiscarded = true; }
            try_consume(TokenKind::RightParen, "')'");
        }
        
        body = parse_block_inline();
        
        return Stmt::Create(m_Context, f.Range.Start, SourceRange(f.Range.Start, peek(-1)->Range.End), StmtKind::For, ForStmt(prologue, condition, step, body));
    }

    Stmt* Parser::parse_if() {
        Token i = consume(); // consume "if"
        
        Expr* condition = parse_expression();
        Stmt* body = parse_statement();
        
        return Stmt::Create(m_Context, i.Range.Start, SourceRange(i.Range.Start, peek(-1)->Range.End), StmtKind::If, IfStmt(condition, body, nullptr));
    }

    Stmt* Parser::parse_break() {
        Token& b = consume(); // consume "break"
        try_consume(TokenKind::Semi, ";");
        return Stmt::Create(m_Context, b.Range.Start, b.Range, StmtKind::Break, BreakStmt());
    }

    Stmt* Parser::parse_continue() {
        Token& b = consume(); // consume "continue"
        try_consume(TokenKind::Semi, ";");
        return Stmt::Create(m_Context, b.Range.Start, b.Range, StmtKind::Continue, BreakStmt());
    }

    Stmt* Parser::parse_return() {
        Token r = consume(); // consume "return"
        Expr* value = parse_expression();
        try_consume(TokenKind::Semi, ";");
        
        return Stmt::Create(m_Context, r.Range.Start, SourceRange(r.Range.Start, peek(-1)->Range.End), StmtKind::Return, ReturnStmt(value));
    }

    Stmt* Parser::parse_expression_statement() {
        Expr* expr = parse_expression();
        if (!expr_ok(expr)) { return &error_stmt; }
        try_consume(TokenKind::Semi, ";");
        expr->ResultDiscarded = true;
        
        return Stmt::Create(m_Context, expr->Loc, SourceRange(expr->Range.Start, peek(-1)->Range.End), StmtKind::Expr, expr);
    }

    Stmt* Parser::parse_declaration_statement(bool global) {
        Decl* decl = parse_variable_decl(global);

        if (!decl_ok(decl)) {
            sync_local();
            if (match(TokenKind::Semi)) {
                consume();
            }
        }

        return Stmt::Create(m_Context, decl->Loc, SourceRange(decl->Range.Start, peek(-1)->Range.End), StmtKind::Decl, decl);
    }

    Stmt* Parser::parse_statement() {
        switch (peek()->Kind) {
            case TokenKind::Semi: {
                Token& tok = consume();
                return Stmt::Create(m_Context, tok.Range.Start, tok.Range, StmtKind::Nop, NopStmt());
            }

            case TokenKind::LeftParen:
            case TokenKind::Minus:
            case TokenKind::Ampersand:
            case TokenKind::Star:
            case TokenKind::Bang:
            case TokenKind::Self:
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
            case TokenKind::New:
            case TokenKind::Delete:
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

            case TokenKind::Let:
                return parse_declaration_statement(false);

            case TokenKind::Unsafe:
                consume();
                return parse_block_inline(true);

            case TokenKind::Fn: {
                Token& tok = consume();
                m_Context->report_compiler_diagnostic(tok.Range.Start, tok.Range, fmt::format("Function declaration is not allowed here"));
                return &error_stmt;
            }

            case TokenKind::Struct: {
                Token& tok = consume();
                m_Context->report_compiler_diagnostic(tok.Range.Start, tok.Range, fmt::format("Structure declaration is not allowed here"));
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
            case TokenKind::Less:
            case TokenKind::LessEq:
            case TokenKind::Greater:
            case TokenKind::GreaterEq: {
                Token& tok = consume();
                m_Context->report_compiler_diagnostic(tok.Range.Start, tok.Range, fmt::format("Unexpected binary operator '{}' while looking for statement", TokenKindToString(tok.Kind)));
                return &error_stmt;
            }

            case TokenKind::AtExtern:
            case TokenKind::AtNoMangle:
            case TokenKind::AtUnsafe: {
                Token& tok = consume();
                m_Context->report_compiler_diagnostic(tok.Range.Start, tok.Range, fmt::format("Unexpected attribute '{}' while looking for statement", TokenKindToString(tok.Kind)));
                return &error_stmt;
            }

            case TokenKind::Void:
            case TokenKind::Bool:
            case TokenKind::Char:
            case TokenKind::UChar:
            case TokenKind::Short:
            case TokenKind::UShort:
            case TokenKind::Int:
            case TokenKind::UInt:
            case TokenKind::Long:
            case TokenKind::ULong:
            case TokenKind::Float:
            case TokenKind::Double:
            case TokenKind::String: {
                Token& tok = consume();
                m_Context->report_compiler_diagnostic(tok.Range.Start, tok.Range, fmt::format("Unexpected type '{}', did you mean to declare a variable? (let <name>: <type>)", TokenKindToString(tok.Kind)));
                return &error_stmt;
            }

            case TokenKind::Module:
            case TokenKind::Import:
            case TokenKind::Else:
            case TokenKind::Arrow:
            case TokenKind::RightParen:
            case TokenKind::LeftBracket:
            case TokenKind::RightBracket:
            case TokenKind::RightCurly:
            case TokenKind::Comma:
            case TokenKind::Colon:
            case TokenKind::ColonColon:
            case TokenKind::Dot: {
                Token& tok = consume();
                m_Context->report_compiler_diagnostic(tok.Range.Start, tok.Range, fmt::format("Unexpected token '{}' while looking for statement", TokenKindToString(tok.Kind)));
                return &error_stmt;
            }

            case TokenKind::Last: ARIA_UNREACHABLE();
        }

        ARIA_UNREACHABLE();
        return nullptr;
    }

    Decl* Parser::parse_module_decl() {
        Token& mod = consume(); // consume "module"
        
        Token* ident = try_consume(TokenKind::Identifier, "identifier");
        if (!ident) { return &error_decl; }

        try_consume(TokenKind::Semi, ";");

        if (m_DeclaredModule) {
            m_Context->report_compiler_diagnostic(mod.Range.Start, SourceRange(mod.Range.Start, peek(-1)->Range.End), "Translation unit already declares a module");
            return &error_decl;
        }

        m_DeclaredModule = true;

        Module* module = m_Context->find_or_create_module(fmt::format("{}", ident->String));
        m_Context->ActiveCompUnit->Parent = module;

        return Decl::Create(m_Context, ident->Range.Start, SourceRange(mod.Range.Start, peek(-1)->Range.End), DeclKind::Module, ModuleDecl(ident->String));
    }

    Stmt* Parser::parse_import_decl() {
        Token& imp = consume(); // consume "import"

        Token* ident = try_consume(TokenKind::Identifier, "identifier");
        if (!ident) { return &error_stmt; }

        try_consume(TokenKind::Semi, ";");

        Stmt* import = Stmt::Create(m_Context, ident->Range.Start, SourceRange(ident->Range.Start, peek(-1)->Range.End), StmtKind::Import, ImportStmt(ident->String));
        m_Context->ActiveCompUnit->Imports.push_back(import);
        return import;
    }

    Decl* Parser::parse_variable_decl(bool global) {
        SourceLocation start = peek()->Range.Start;

        try_consume(TokenKind::Let, "let");

        Token* ident = try_consume(TokenKind::Identifier, "identifier");
        if (!ident) { return &error_decl; }
        TypeInfo* type = nullptr;
        Expr* value = nullptr;

        if (match(TokenKind::Colon)) {
            try_consume(TokenKind::Colon, ":");

            if (is_primitive_type() || match(TokenKind::Identifier)) {
                type = parse_type();
            } else {
                m_Context->report_compiler_diagnostic(peek()->Range.Start, peek()->Range, "Expected a type after ':'");
                type = &error_type;
            }
        }

        if (match(TokenKind::Eq)) {
            consume();
            value = parse_expression();
        } else if (!type) {
            m_Context->report_compiler_diagnostic(ident->Range.Start, SourceRange(start, peek()->Range.End), "No initializer provided for type-inffered variable declaration");
            type = &error_type;

            return &error_decl;
        }

        try_consume(TokenKind::Semi, ";");

        Decl* decl = Decl::Create(m_Context, ident->Range.Start, SourceRange(start, peek(-1)->Range.End), DeclKind::Var, VarDecl(ident->String, type, value, global));

        if (global) {
            m_Context->ActiveCompUnit->Globals.push_back(decl);
        }

        return decl;
    }

    Decl* Parser::parse_function_decl() {
        SourceLocation start = peek()->Range.Start;
        Token fn = consume(); // consume "fn"
        TypeInfo* type = &error_type;
        TinyVector<FunctionDecl::Attribute> attrs;

        if (is_primitive_type()) {
            m_Context->report_compiler_diagnostic(peek()->Range.Start, peek()->Range, "Expected an indentifier but got a type (NOTE: function declarations look like: fn name() -> type {...})");
            type = parse_type();
        }

        Token* ident = try_consume(TokenKind::Identifier, "identifier");
        if (!ident) {
            sync_local();
            return &error_decl;
        }

        auto[params, paramTypes] = parse_function_params();

        if (try_consume(TokenKind::Arrow, "->")) {
            if (!(is_primitive_type() || match(TokenKind::Identifier))) {
                m_Context->report_compiler_diagnostic(peek()->Range.Start, peek()->Range, "Expected a type after '->'");
                sync_local();
            } else if (type->is_error()) {
                type = parse_type();
            }

            attrs = parse_function_attrs();
        }
        
        Stmt* body = nullptr;

        if (match(TokenKind::LeftCurly)) {
            body = parse_block();
        } else {
            try_consume(TokenKind::Semi, ";");
        }

        FunctionDeclaration typeDecl;
        typeDecl.ReturnType = type;
        typeDecl.ParamTypes = paramTypes;

        TypeInfo* finalType = TypeInfo::Create(m_Context, TypeKind::Function, false);
        finalType->Function = typeDecl;
        Decl* decl = Decl::Create(m_Context, ident->Range.Start, SourceRange(start, peek(-1)->Range.End), DeclKind::Function, FunctionDecl(ident->String, finalType, params, body, attrs));

        m_Context->ActiveCompUnit->Funcs.push_back(decl);
        return decl;
    }

    std::pair<TinyVector<Decl*>, TinyVector<TypeInfo*>> Parser::parse_function_params() {
        TinyVector<Decl*> params;
        TinyVector<TypeInfo*> paramTypes;

        try_consume(TokenKind::LeftParen, "(");

        while (!match(TokenKind::RightParen)) {
            if (is_primitive_type()) {
                m_Context->report_compiler_diagnostic(peek()->Range.Start, peek()->Range, "Expected an identifier but got a type (<name>: <type>)");
                consume();
                continue;
            }

            Token* paramIdent = try_consume(TokenKind::Identifier, "identifier");
            
            if (!paramIdent) {
                consume();
                continue;
            }

            try_consume(TokenKind::Colon, ":");

            if (!(is_primitive_type() || match(TokenKind::Identifier))) {
                m_Context->report_compiler_diagnostic(peek()->Range.Start, peek()->Range, "Expected a type");
                consume();
                continue;
            }

            TypeInfo* paramType = parse_type();
            
            params.append(m_Context, Decl::Create(m_Context, paramIdent->Range.Start, paramIdent->Range, DeclKind::Param, ParamDecl(paramIdent->String, paramType)));
            paramTypes.append(m_Context, paramType);

            if (match(TokenKind::Comma)) { consume(); continue; }
            if (match(TokenKind::RightParen)) { break; }

            sync_local();
            m_Context->report_compiler_diagnostic(peek()->Range.Start, peek()->Range, "Expected either ',' or ')'");
        }

        try_consume(TokenKind::RightParen, ")");

        return { params, paramTypes };
    }

    Decl* Parser::parse_struct_decl() {
        Token s = consume(); // consume "struct"
        Token* ident = try_consume(TokenKind::Identifier, "indentifier");
        
        TinyVector<Decl*> fields;

        StructDecl::DefinitionData def{};
        
        try_consume(TokenKind::LeftCurly, "{");
        while (!match(TokenKind::RightCurly)) {
            SourceLocation start = peek()->Range.Start;

            if (match(TokenKind::Identifier)) {
                if (peek()->String == ident->String) { // Check for constructor
                    consume();
                    try_consume(TokenKind::LeftParen, "(");
                    try_consume(TokenKind::RightParen, ")");
                    
                    Stmt* body = parse_block();

                    def.HasDefaultCtor = true;
                    def.HasUserDefaultCtor = true;

                    fields.append(m_Context, Decl::Create(m_Context, start, SourceRange(start, peek(-1)->Range.End), DeclKind::Constructor, ConstructorDecl({}, body)));
                } else {
                    Token* fieldName = try_consume(TokenKind::Identifier, "identifier");
                    if (!fieldName) { sync_local(); continue; }

                    try_consume(TokenKind::Colon, ":");

                    TypeInfo* type = parse_type();

                    if (!try_consume(TokenKind::Semi, ";")) {
                        sync_local();
                        if (match(TokenKind::Semi)) { consume(); }
                    }

                    fields.append(m_Context, Decl::Create(m_Context, fieldName->Range.Start, SourceRange(start, peek(-1)->Range.End), DeclKind::Field, FieldDecl(fieldName->String, type)));
                }
            } else if (match(TokenKind::Squigly)) {
                consume();

                Token* dtor = try_consume(TokenKind::Identifier, "identifier");
                if (!dtor) { sync_local(); continue; }

                if (dtor->String != ident->String) { m_Context->report_compiler_diagnostic(dtor->Range.Start, dtor->Range, "Name of destructor must match name of struct"); }

                try_consume(TokenKind::LeftParen, "(");
                try_consume(TokenKind::RightParen, ")");

                Stmt* body = parse_block();

                def.HasUserDtor = true;
                def.TrivialDtor = false;

                fields.append(m_Context, Decl::Create(m_Context, start, SourceRange(start, peek(-1)->Range.End), DeclKind::Destructor, DestructorDecl(body)));
            } else {
                m_Context->report_compiler_diagnostic(start, SourceRange(start, start), "Expected identifier or '~'");
                sync_local();
            }
        }
        try_consume(TokenKind::RightCurly, "}");
        
        Decl* decl = Decl::Create(m_Context, ident->Range.Start, SourceRange(s.Range.Start, peek(-1)->Range.End), DeclKind::Struct, StructDecl(ident->String, def, fields));
        m_Context->ActiveCompUnit->Structs.push_back(decl);
        return decl;
    }

    TinyVector<FunctionDecl::Attribute> Parser::parse_function_attrs() {
        TinyVector<FunctionDecl::Attribute> attrs;
        bool hasExtern = false;
        bool hasNoMangle = false;
        bool hasUnsafe = false;

        while (peek()) {
            if (match(TokenKind::AtExtern)) {
                if (hasExtern) { m_Context->report_compiler_diagnostic(peek()->Range.Start, peek()->Range, "@extern attribute was already added"); }
                if (hasNoMangle) { m_Context->report_compiler_diagnostic(peek()->Range.Start, peek()->Range, "@nomangle and @extern must be mutually exclusive"); }

                consume();
                
                try_consume(TokenKind::LeftParen, "(");
                Token* name = try_consume(TokenKind::StrLit, "string literal");
                try_consume(TokenKind::RightParen, ")");

                FunctionDecl::Attribute attr;
                attr.Kind = FunctionDecl::AttributeKind::Extern;
                if (name) { attr.Arg = name->String; }

                attrs.append(m_Context, attr);
                hasExtern = true;
            } else if (match(TokenKind::AtNoMangle)) {
                if (hasNoMangle) { m_Context->report_compiler_diagnostic(peek()->Range.Start, peek()->Range, "@nomangle attribute was already added"); }
                if (hasExtern) { m_Context->report_compiler_diagnostic(peek()->Range.Start, peek()->Range, "@nomangle and @extern must be mutually exclusive"); }

                consume();
                attrs.append(m_Context, { FunctionDecl::AttributeKind::NoMangle });
                hasNoMangle = true;
            } else if (match(TokenKind::AtUnsafe)) {
                if (hasUnsafe) { m_Context->report_compiler_diagnostic(peek()->Range.Start, peek()->Range, "@unsafe attribute was already added"); }

                consume();
                attrs.append(m_Context, { FunctionDecl::AttributeKind::Unsafe });
                hasUnsafe = true;
            } else {
                break;
            }
        }

        return attrs;
    }

    Stmt* Parser::parse_global() {
        switch (peek()->Kind) {
            case TokenKind::Module: {
                Decl* d = parse_module_decl();
                if (!decl_ok(d)) { sync_global(); return &error_stmt; }

                return Stmt::Create(m_Context, d->Loc, d->Range, StmtKind::Decl, d);
            }

            case TokenKind::Import:
                return parse_import_decl();

            case TokenKind::Let:
                return parse_declaration_statement(true);

            case TokenKind::Fn: {
                Decl* decl = parse_function_decl();
                if (!decl_ok(decl)) { return &error_stmt; }

                return Stmt::Create(m_Context, decl->Loc, decl->Range, StmtKind::Decl, decl);
            }

            case TokenKind::Struct: {
                Decl* decl = parse_struct_decl();
                if (!decl_ok(decl)) { return &error_stmt; }

                return Stmt::Create(m_Context, decl->Loc, decl->Range, StmtKind::Decl, decl);
            }

            case TokenKind::Semi: {
                m_Context->report_compiler_diagnostic(peek()->Range.Start, peek()->Range, fmt::format("Token ';' was unexpected, try removing it")); 
                consume(); 
                return &error_stmt;
            }

            case TokenKind::Void:
            case TokenKind::Bool:
            case TokenKind::Char:
            case TokenKind::UChar:
            case TokenKind::Short:
            case TokenKind::UShort:
            case TokenKind::Int:
            case TokenKind::UInt:
            case TokenKind::Long:
            case TokenKind::ULong:
            case TokenKind::Float:
            case TokenKind::Double:
            case TokenKind::String:
            case TokenKind::Identifier: {
                Token& t = consume();
                m_Context->report_compiler_diagnostic(t.Range.Start, t.Range, fmt::format("Unexpected type '{}', did you mean to declare a global variable? (let <name>: <type>)", TokenKindToString(t.Kind)));
            }

            default: {
                m_Context->report_compiler_diagnostic(peek()->Range.Start, peek()->Range, fmt::format("Expected the start of a global declaration", TokenKindToString(peek()->Kind)));
                sync_global();
                return &error_stmt;
            }
        }
    }

    void Parser::sync_global() {
        while (peek()) {
            TokenKind kind = peek()->Kind;

            if (kind == TokenKind::Fn || kind == TokenKind::Struct || is_primitive_type() || kind == TokenKind::Identifier) {
                return;
            }

            consume();
        }
    }

    void Parser::sync_local() {
        while (peek()) {
            TokenKind type = peek()->Kind;

            if (type == TokenKind::Semi || type == TokenKind::LeftCurly || type == TokenKind::Comma) {
                return;
            }

            consume();
        }
    }

    void Parser::error_expected(const std::string& expect, SourceLocation loc, SourceRange range) {
        if (peek()) {
            m_Context->report_compiler_diagnostic(loc, range, fmt::format("Expected '{}' but got '{}'", expect, TokenKindToString(peek()->Kind)));
        } else {
            m_Context->report_compiler_diagnostic(loc, range, fmt::format("Expected '{}' but got EOF", expect));
        }
    }

    bool Parser::stmt_ok(Stmt* stmt) {
        if (stmt == nullptr) { return true; }
        if (stmt->Kind == StmtKind::Error) { return false; }

        return true;
    }

    bool Parser::expr_ok(Expr* expr) {
        if (expr == nullptr) { return true; }
        if (expr->Kind == ExprKind::Error) { return false; }

        return true;
    }

    bool Parser::decl_ok(Decl* decl) {
        if (decl == nullptr) { return true; }
        if (decl->Kind == DeclKind::Error) { return false; }

        return true;
    }

} // namespace Aria::Internal
