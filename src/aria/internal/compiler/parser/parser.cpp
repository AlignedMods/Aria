#include "aria/internal/compiler/parser/parser.hpp"

#include "fmt/format.h"

#include <string>
#include <charconv>
#include <limits>

#define BIND_PARSE_RULE(fn) [this](Expr* expr) -> Expr* { return fn(expr); }

#define ASSIGN_OR_RET(varName, expr, what) do { \
        varName = expr; \
        if (!varName) { return what; } \
    } while (false)

#define CONSUME_OR_RET(kind, what) do { \
        if (!TryConsume(TokenKind::kind, TokenKindToString(TokenKind::kind))) { return what; } \
    } while (false)

static constexpr size_t PREC_NONE = 0;
static constexpr size_t PREC_ASSIGNMENT = 10;
static constexpr size_t PREC_RELATIONAL = 20;
static constexpr size_t PREC_BIT = 30;
static constexpr size_t PREC_ADDITIVE = 40;
static constexpr size_t PREC_MULTIPLICATIVE = 50;
static constexpr size_t PREC_CALL = 60;

namespace Aria::Internal {

    Parser::Parser(CompilationContext* ctx) {
        m_Context = ctx;
        m_Tokens = ctx->GetTokens();

        // Setup error nodes
        m_ErrorExpr = Expr::Create(m_Context, SourceLocation(), SourceRange(), ExprKind::Error, ExprValueKind::RValue, &ErrorType, ErrorExpr());
        m_ErrorDecl = Decl::Create(m_Context, SourceLocation(), SourceRange(), DeclKind::Error, ErrorDecl());
        m_ErrorStmt = Stmt::Create(m_Context, SourceLocation(), SourceRange(), StmtKind::Error, ErrorStmt());

        AddExprRules();
        ParseImpl();
    }

    void Parser::AddExprRules() {
        m_ExprRules.reserve(static_cast<size_t>(TokenKind::Last));

        m_ExprRules[TokenKind::LeftParen] =       { BIND_PARSE_RULE(ParseGrouping), BIND_PARSE_RULE(ParseCall), PREC_CALL };
        m_ExprRules[TokenKind::Plus] =            { nullptr, BIND_PARSE_RULE(ParseBinary), PREC_ADDITIVE };
        m_ExprRules[TokenKind::Minus] =           { BIND_PARSE_RULE(ParseUnary), BIND_PARSE_RULE(ParseBinary), PREC_ADDITIVE };
        m_ExprRules[TokenKind::Star] =            { nullptr, BIND_PARSE_RULE(ParseBinary), PREC_MULTIPLICATIVE };
        m_ExprRules[TokenKind::Slash] =           { nullptr, BIND_PARSE_RULE(ParseBinary), PREC_MULTIPLICATIVE };
        m_ExprRules[TokenKind::Percent] =         { nullptr, BIND_PARSE_RULE(ParseBinary), PREC_MULTIPLICATIVE };
        m_ExprRules[TokenKind::Ampersand] =       { nullptr, BIND_PARSE_RULE(ParseBinary), PREC_BIT };
        m_ExprRules[TokenKind::DoubleAmpersand] = { nullptr, BIND_PARSE_RULE(ParseBinary), PREC_RELATIONAL };
        m_ExprRules[TokenKind::Pipe] =            { nullptr, BIND_PARSE_RULE(ParseBinary), PREC_BIT };
        m_ExprRules[TokenKind::DoublePipe] =      { nullptr, BIND_PARSE_RULE(ParseBinary), PREC_RELATIONAL };
        m_ExprRules[TokenKind::UpArrow] =         { nullptr, BIND_PARSE_RULE(ParseBinary), PREC_BIT };
        m_ExprRules[TokenKind::EqEq] =            { nullptr, BIND_PARSE_RULE(ParseBinary), PREC_RELATIONAL };
        m_ExprRules[TokenKind::BangEq] =          { nullptr, BIND_PARSE_RULE(ParseBinary), PREC_RELATIONAL };
        m_ExprRules[TokenKind::Less] =            { nullptr, BIND_PARSE_RULE(ParseBinary), PREC_RELATIONAL };
        m_ExprRules[TokenKind::LessEq] =          { nullptr, BIND_PARSE_RULE(ParseBinary), PREC_RELATIONAL };
        m_ExprRules[TokenKind::Greater] =         { nullptr, BIND_PARSE_RULE(ParseBinary), PREC_RELATIONAL };
        m_ExprRules[TokenKind::GreaterEq] =       { nullptr, BIND_PARSE_RULE(ParseBinary), PREC_RELATIONAL };

        m_ExprRules[TokenKind::Eq] =              { nullptr, BIND_PARSE_RULE(ParseBinary), PREC_ASSIGNMENT };
        m_ExprRules[TokenKind::PlusEq] =          { nullptr, BIND_PARSE_RULE(ParseCompoundAssignment), PREC_ASSIGNMENT };
        m_ExprRules[TokenKind::MinusEq] =         { nullptr, BIND_PARSE_RULE(ParseCompoundAssignment), PREC_ASSIGNMENT };
        m_ExprRules[TokenKind::StarEq] =          { nullptr, BIND_PARSE_RULE(ParseCompoundAssignment), PREC_ASSIGNMENT };
        m_ExprRules[TokenKind::SlashEq] =         { nullptr, BIND_PARSE_RULE(ParseCompoundAssignment), PREC_ASSIGNMENT };
        m_ExprRules[TokenKind::PercentEq] =       { nullptr, BIND_PARSE_RULE(ParseCompoundAssignment), PREC_ASSIGNMENT };
        m_ExprRules[TokenKind::AmpersandEq] =     { nullptr, BIND_PARSE_RULE(ParseCompoundAssignment), PREC_ASSIGNMENT };
        m_ExprRules[TokenKind::PipeEq] =          { nullptr, BIND_PARSE_RULE(ParseCompoundAssignment), PREC_ASSIGNMENT };
        m_ExprRules[TokenKind::UpArrowEq] =       { nullptr, BIND_PARSE_RULE(ParseCompoundAssignment), PREC_ASSIGNMENT };

        m_ExprRules[TokenKind::Dot] =             { nullptr, BIND_PARSE_RULE(ParseMember), PREC_CALL };

        m_ExprRules[TokenKind::Self] =            { BIND_PARSE_RULE(ParsePrimary), nullptr, PREC_NONE };
        m_ExprRules[TokenKind::True] =            { BIND_PARSE_RULE(ParsePrimary), nullptr, PREC_NONE };
        m_ExprRules[TokenKind::False] =           { BIND_PARSE_RULE(ParsePrimary), nullptr, PREC_NONE };
        m_ExprRules[TokenKind::CharLit] =         { BIND_PARSE_RULE(ParsePrimary), nullptr, PREC_NONE };
        m_ExprRules[TokenKind::IntLit] =          { BIND_PARSE_RULE(ParsePrimary), nullptr, PREC_NONE };
        m_ExprRules[TokenKind::UintLit] =         { BIND_PARSE_RULE(ParsePrimary), nullptr, PREC_NONE };
        m_ExprRules[TokenKind::NumLit] =          { BIND_PARSE_RULE(ParsePrimary), nullptr, PREC_NONE };
        m_ExprRules[TokenKind::StrLit] =          { BIND_PARSE_RULE(ParsePrimary), nullptr, PREC_NONE };
        m_ExprRules[TokenKind::Identifier] =      { BIND_PARSE_RULE(ParsePrimary), nullptr, PREC_NONE };
    }

    void Parser::ParseImpl() {
        TinyVector<Stmt*> stmts;
        while (Peek()) {
            Stmt* stmt = ParseGlobal();
            if (stmt != nullptr) {
                stmts.Append(m_Context, stmt);
            }
        }

        TranslationUnitDecl root(stmts);
        Decl* decl = Decl::Create(m_Context, SourceLocation(), SourceRange(), DeclKind::TranslationUnit, root);
        m_Context->SetRootASTNode(Stmt::Create(m_Context, SourceLocation(), SourceRange(), StmtKind::Decl, decl));
    }

    Token* Parser::Peek(size_t count) {
        // While the count is a size_t, you are still allowed to use -1
        // Even if you pass -1 the actual data underneath is the same
        if (m_Index + count < m_Tokens.size()) {
            return &m_Tokens.at(m_Index + count);
        } else {
            return nullptr;
        }
    }

    Token& Parser::Consume() {
        ARIA_ASSERT(m_Index < m_Tokens.size(), "Consume out of bounds!");

        return m_Tokens.at(m_Index++);
    }

    Token* Parser::TryConsume(TokenKind kind, const std::string& expect) {
        if (Match(kind)) {
            Token& t = Consume();
            return &t;
        }

        Token* prev = Peek(-1);
        ErrorExpected(expect, prev->Range.End, prev->Range);
        return nullptr;
    }

    bool Parser::Match(TokenKind kind) {
        if (!Peek()) {
            return false;
        }

        if (Peek()->Kind == kind) {
            return true;
        } else {
            return false;
        }
    }

    Expr* Parser::ParseGrouping(Expr* left) {
        ARIA_ASSERT(left == nullptr, "Parser::ParseGrouping() should not have a left side");

        Token* lp = TryConsume(TokenKind::LeftParen, "(");
        if (IsType()) { // Cast
            TypeInfo* type = ParseType();
            if (!TryConsume(TokenKind::RightParen, ")")) {
                StabilizeParser();
                return m_ErrorExpr;
            }

            Expr* child = ParseExpression();

            return Expr::Create(m_Context, lp->Range.Start, SourceRange(lp->Range.Start, child->Range.End), ExprKind::Cast, 
                ExprValueKind::RValue, type,
                CastExpr(child, type));
        } else { // Grouping of expression
            Expr* child = ParseExpression();
            Token* rp = TryConsume(TokenKind::RightParen, ")");

            if (!rp) {
                StabilizeParser();
                return m_ErrorExpr;
            }

            return Expr::Create(m_Context, lp->Range.Start, SourceRange(lp->Range.Start, rp->Range.End), ExprKind::Paren, 
                child->ValueKind, child->Type,
                ParenExpr(child));
        }
    }

    Expr* Parser::ParseCall(Expr* left) {
        ARIA_ASSERT(left, "Parser::ParseCall() expects a left side");

        Token* lp = TryConsume(TokenKind::LeftParen, "(");
        TinyVector<Expr*> args;

        while (!Match(TokenKind::RightParen)) {
            Expr* val = ParseExpression();
    
            if (Match(TokenKind::Comma)) {
                Consume();
            }
    
            args.Append(m_Context, val);
        }
    
        Token* rp = TryConsume(TokenKind::RightParen, ")");
        if (!rp) { return m_ErrorExpr; }
    
        return Expr::Create(m_Context, lp->Range.Start, SourceRange(left->Range.Start, rp->Range.End), ExprKind::Call,
            ExprValueKind::RValue, nullptr, 
            CallExpr(left, args));
    }

    UnaryOperatorKind Parser::GetUnaryOperatorFromToken(Token* token) {
        switch (token->Kind) {
            case TokenKind::Minus: return UnaryOperatorKind::Negate;
            default: return UnaryOperatorKind::Invalid;
        }
    }

    BinaryOperatorKind Parser::GetBinaryOperatorFromToken(Token* token) {
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
            case TokenKind::AmpersandEq: return BinaryOperatorKind::CompoundAnd;
            case TokenKind::DoubleAmpersand: return BinaryOperatorKind::LogAnd;
            case TokenKind::Pipe: return BinaryOperatorKind::BitOr;
            case TokenKind::PipeEq: return BinaryOperatorKind::CompoundOr;
            case TokenKind::DoublePipe: return BinaryOperatorKind::LogOr;
            case TokenKind::UpArrow: return BinaryOperatorKind::BitXor;
            case TokenKind::UpArrowEq: return BinaryOperatorKind::CompoundXor;
            case TokenKind::Less: return BinaryOperatorKind::Less;
            case TokenKind::LessEq: return BinaryOperatorKind::LessOrEq;
            case TokenKind::Greater: return BinaryOperatorKind::Greater;
            case TokenKind::GreaterEq: return BinaryOperatorKind::GreaterOrEq;
            case TokenKind::Eq: return BinaryOperatorKind::Eq;
            case TokenKind::EqEq: return BinaryOperatorKind::IsEq;
            case TokenKind::BangEq: return BinaryOperatorKind::IsNotEq;
            default: return BinaryOperatorKind::Invalid;
        }
    }

    Expr* Parser::ParseUnary(Expr* left) {
        ARIA_ASSERT(left == nullptr, "Parser::ParseUnary() should not have a left side");

        Token op = Consume();
        Expr* expr = ParseExpression();

        if (!ExprOk(expr)) { return m_ErrorExpr; }

        return Expr::Create(m_Context, op.Range.Start, SourceRange(op.Range.Start, expr->Range.End), ExprKind::UnaryOperator,
            ExprValueKind::RValue, expr->Type,
            UnaryOperatorExpr(expr, GetUnaryOperatorFromToken(&op)));
    }

    Expr* Parser::ParseBinary(Expr* left) {
        ARIA_ASSERT(left, "Parser::ParseBinary() expects a left side");

        Token op = Consume();
        Expr* right = nullptr;
        
        if (op.Kind == TokenKind::Eq) { right = ParsePrecedence(PREC_ASSIGNMENT); }
        else { right = ParsePrecedence(m_ExprRules[op.Kind].Precedence + 1); }

        if (ExprOk(right)) {
            return Expr::Create(m_Context, op.Range.Start, SourceRange(left->Range.Start, right->Range.End), ExprKind::BinaryOperator,
                ExprValueKind::RValue, nullptr,
                BinaryOperatorExpr(left, right, GetBinaryOperatorFromToken(&op)));
        }
        
        return m_ErrorExpr;
    }

    Expr* Parser::ParseCompoundAssignment(Expr* left) {
        ARIA_ASSERT(left, "Parser::ParseCompoundAssignment() expects a left side");

        Token op = Consume();
        Expr* right = ParsePrecedence(PREC_ASSIGNMENT);

        if (ExprOk(right)) {
            return Expr::Create(m_Context, op.Range.Start, SourceRange(left->Range.Start, right->Range.End), ExprKind::CompoundAssign,
                ExprValueKind::RValue, nullptr,
                BinaryOperatorExpr(left, right, GetBinaryOperatorFromToken(&op)));
        }
        
        return m_ErrorExpr;
    }

    Expr* Parser::ParseMember(Expr* left) { ARIA_ASSERT(false, "todo!"); }

    Expr* Parser::ParsePrimary(Expr* left) {
        ARIA_ASSERT(left == nullptr, "Parser::ParsePrimary() should not have a left side");

        Token& t = Consume();
        switch (t.Kind) {
            case TokenKind::False: {
                return Expr::Create(m_Context, t.Range.Start, t.Range, ExprKind::BooleanConstant, 
                    ExprValueKind::RValue, &BoolType, 
                    BooleanConstantExpr(false));
            }

            case TokenKind::True: {
                return Expr::Create(m_Context, t.Range.Start, t.Range, ExprKind::BooleanConstant, 
                    ExprValueKind::RValue, &BoolType, 
                    BooleanConstantExpr(true));
            }

            case TokenKind::CharLit: {
                int8_t ch = static_cast<int8_t>(t.Integer);
                return Expr::Create(m_Context, t.Range.Start, t.Range, ExprKind::CharacterConstant, 
                    ExprValueKind::RValue, &CharType, 
                    CharacterConstantExpr(ch));
            }
    
            case TokenKind::IntLit: {
                return Expr::Create(m_Context, t.Range.Start, t.Range, ExprKind::IntegerConstant,
                    ExprValueKind::RValue, &LongType, 
                    IntegerConstantExpr(t.Integer));
            }

            case TokenKind::UintLit: {
                return Expr::Create(m_Context, t.Range.Start, t.Range, ExprKind::IntegerConstant, 
                    ExprValueKind::RValue, &ULongType, 
                    IntegerConstantExpr(t.Integer));
            }
    
            case TokenKind::NumLit: {
                return Expr::Create(m_Context, t.Range.Start, t.Range, ExprKind::FloatingConstant,
                    ExprValueKind::RValue, &DoubleType, 
                    FloatingConstantExpr(t.Number));
            }
    
            case TokenKind::StrLit: {
                return Expr::Create(m_Context, t.Range.Start, t.Range, ExprKind::StringConstant,
                    ExprValueKind::RValue, &StringType, 
                    IntegerConstantExpr(t.Integer));
            }

            case TokenKind::Identifier: {
                return Expr::Create(m_Context, t.Range.Start, t.Range, ExprKind::DeclRef,
                    ExprValueKind::LValue, nullptr, 
                    DeclRefExpr(t.String));
            }

            default: return m_ErrorExpr;
        }
    }

    Expr* Parser::ParsePrecedenceWithLeft(Expr* left, size_t precedence) {
        ARIA_ASSERT(left, "Parser::ParsePrecedenceWithLeft() expects a left side");

        while (true) {
            Token* tok = Peek();
            if (!tok) { break; }
            size_t tokPrecedence = m_ExprRules[tok->Kind].Precedence;

            if (precedence > tokPrecedence) { break; }

            ParseExprFn infixRule = m_ExprRules[tok->Kind].Infix;

            if (!infixRule) {
                StabilizeParser();
                m_Context->ReportCompilerError(tok->Range.Start, tok->Range, fmt::format("'{}' cannot appear here, did you forget something before the operator?", TokenKindToString(tok->Kind)));
                return m_ErrorExpr;
            }

            left = infixRule(left);
        }

        return left;
    }

    Expr* Parser::ParsePrecedence(size_t precedence) {
        Token* tok = Peek();
        ParseExprFn prefixRule = m_ExprRules[tok->Kind].Prefix;

        if (!prefixRule) {
            StabilizeParser();
            m_Context->ReportCompilerError(tok->Range.Start, tok->Range, "Expected an expression");
            return m_ErrorExpr;
        }

        Expr* left = prefixRule(nullptr);
        if (!ExprOk(left)) { return m_ErrorExpr; }

        return ParsePrecedenceWithLeft(left, precedence);
    }

    Expr* Parser::ParseExpression() {
        return ParsePrecedence(PREC_ASSIGNMENT);
    }

    bool Parser::IsPrimitiveType() {
        if (!Peek()) { return false; }

        Token type = *Peek();

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

    bool Parser::IsType() {
        if (IsPrimitiveType()) { return true; }

        if (Peek()->Kind == TokenKind::Identifier) {
            if (m_DeclaredTypes.contains(fmt::format("{}", Peek()->String))) {
                return true;
            }

            return false;
        }

        return false;
    }

    TypeInfo* Parser::ParseType() {
        ARIA_ASSERT(IsType(), "Cannot parse a type out of a non type");

        TypeInfo* type = TypeInfo::Create(m_Context, PrimitiveType::Error, false);

        switch (Consume().Kind) {
            case TokenKind::Void:       type->Type = PrimitiveType::Void; break;

            case TokenKind::Bool:       type->Type = PrimitiveType::Bool; break;

            case TokenKind::Char:       type->Type = PrimitiveType::Char; break;
            case TokenKind::UChar:      type->Type = PrimitiveType::UChar; break;
            case TokenKind::Short:      type->Type = PrimitiveType::Short; break;
            case TokenKind::UShort:     type->Type = PrimitiveType::UShort; break;
            case TokenKind::Int:        type->Type = PrimitiveType::Int; break;
            case TokenKind::UInt:       type->Type = PrimitiveType::UInt; break;
            case TokenKind::Long:       type->Type = PrimitiveType::Long; break;
            case TokenKind::ULong:      type->Type = PrimitiveType::ULong; break;

            case TokenKind::Float:      type->Type = PrimitiveType::Float; break;
            case TokenKind::Double:     type->Type = PrimitiveType::Double; break;

            case TokenKind::String:     type->Type = PrimitiveType::String; break;

            case TokenKind::Identifier: {
                StringView ident = Peek(-1)->String;
                type->Type = PrimitiveType::Structure;
                type->Data = StructDeclaration(ident, m_DeclaredTypes.at(fmt::format("{}", ident)));
                break;
            }

            default: ARIA_UNREACHABLE(); break;
        }

        if (Match(TokenKind::Ampersand)) {
            Consume();
            type->Reference = true;
        }

        return type;
    }

    Stmt* Parser::ParseBlock() {
        TinyVector<Stmt*> stmts;
        Token* l = TryConsume(TokenKind::LeftCurly, "'{'");
        if (!l) { return nullptr; }

        while (!Match(TokenKind::RightCurly)) {
            Stmt* stmt = ParseStatement();

            if (stmt != nullptr) {
                stmts.Append(m_Context, stmt);
            }
        }

        Token* r = TryConsume(TokenKind::RightCurly, "'}'");
        if (!r) { return nullptr; }

        return Stmt::Create(m_Context, l->Range.Start, SourceRange(l->Range.Start, r->Range.End), StmtKind::Block, BlockStmt(stmts));
    }

    Stmt* Parser::ParseBlockInline() {
        if (Match(TokenKind::LeftCurly)) {
            return ParseBlock();
        } else {
            Stmt* stmt = ParseStatement();
            if (!stmt) { return nullptr; }

            TinyVector<Stmt*> stmts;
            stmts.Append(m_Context, stmt);

            return Stmt::Create(m_Context, stmt->Loc, stmt->Range, StmtKind::Block, BlockStmt(stmts));
        }
    }

    Stmt* Parser::ParseWhile() {
        Token w = Consume(); // Consume "while"

        Expr* condition = ParseExpression();
        Stmt* body = ParseBlockInline();

        return Stmt::Create(m_Context, w.Range.Start, SourceRange(w.Range.Start, Peek(-1)->Range.End), StmtKind::While, WhileStmt(condition, body));
    }

    Stmt* Parser::ParseDoWhile() {
        Token d = Consume(); // Consume "do"

        Stmt* body = ParseBlockInline();
        TryConsume(TokenKind::While, "while");
        Expr* condition = ParseExpression();
        
        return Stmt::Create(m_Context, d.Range.Start, SourceRange(d.Range.Start, Peek(-1)->Range.End), StmtKind::DoWhile, DoWhileStmt(condition, body));
    }

    Stmt* Parser::ParseFor() {
        ARIA_ASSERT(false, "todo!");
        // Token f = Consume(); // Consume "for"
        // 
        // // We still try to keep parsing if we are missing a '('
        // // It could be quite likely that the user just forgot it
        // TryConsume(TokenKind::LeftParen, "(");
        // 
        // Decl* prologue = ParseVariableDecl();
        // Expr* condition = ParseExpression();
        // 
        // // Check for commas as delimiters,
        // // This is an easy to detect error that is relatively common
        // if (Match(TokenKind::Comma)) {
        //     Token& comma = Consume();
        //     m_Context->ReportCompilerError(comma.Range.Start, comma.Range, "Expected ';' but got a ',' (Did you mean to use ';'?)");
        // } else {
        //     TryConsume(TokenKind::Semi, "';'");
        // }
        // Expr* epilogue = ParseExpression();
        // 
        // TryConsume(TokenKind::RightParen, "')'");
        // Stmt* body = ParseBlockInline();
        // 
        // return Stmt::Create(m_Context, f.Range.Start, SourceRange(f.Range.Start, Peek(-1)->Range.End), StmtKind::For, ForStmt(prologue, condition, epilogue, body));
    }

    Stmt* Parser::ParseIf() {
        Token i = Consume(); // Consume "if"
        
        Expr* condition = ParseExpression();
        Stmt* body = ParseStatement();
        
        return Stmt::Create(m_Context, i.Range.Start, SourceRange(i.Range.Start, Peek(-1)->Range.End), StmtKind::If, IfStmt(condition, body, nullptr));
    }

    Stmt* Parser::ParseBreak() {
        ARIA_ASSERT(false, "todo: add break parsing");
        // Token b = Consume(); // Consume "break"

        // return Allocate<NodeStmt>(nullptr, b.Loc);
    }

    Stmt* Parser::ParseContinue() {
        ARIA_ASSERT(false, "todo: add continue parsing");
        // Token c = Consume(); // Consume "continue"

        // return Allocate<NodeStmt>(nullptr, c.Loc);
    }

    Stmt* Parser::ParseReturn() {
        Token r = Consume(); // Consume "return"
        Expr* value = ParseExpression();
        
        return Stmt::Create(m_Context, r.Range.Start, SourceRange(r.Range.Start, Peek(-1)->Range.End), StmtKind::Return, ReturnStmt(value));
    }

    Stmt* Parser::ParseExpressionStatement() {
        Expr* expr = nullptr;
        ASSIGN_OR_RET(expr, ParseExpression(), m_ErrorStmt);
        CONSUME_OR_RET(Semi, m_ErrorStmt);
        
        return Stmt::Create(m_Context, expr->Loc, SourceRange(expr->Range.Start, Peek(-1)->Range.End), StmtKind::Expr, expr);
    }

    Stmt* Parser::ParseDeclarationStatement() {
        Decl* decl = ParseVariableDecl();

        if (!DeclOk(decl)) {
            StabilizeParser();
            if (Match(TokenKind::Semi)) {
                Consume();
            }
        }

        return Stmt::Create(m_Context, decl->Loc, SourceRange(decl->Range.Start, Peek(-1)->Range.End), StmtKind::Decl, decl);
    }

    Stmt* Parser::ParseDeclarationOrExpression() {
        ARIA_ASSERT(Peek()->Kind == TokenKind::Identifier, "Invalid current token for Parser::ParseDeclarationOrExpression()");

        if (m_DeclaredTypes.contains(fmt::format("{}", Peek()->String))) {
            Decl* decl = nullptr;
            ASSIGN_OR_RET(decl, ParseVariableDecl(), m_ErrorStmt);
            CONSUME_OR_RET(Semi, m_ErrorStmt);

            return Stmt::Create(m_Context, decl->Loc, SourceRange(decl->Range.Start, Peek(-1)->Range.End), StmtKind::Decl, decl);
        }

        return ParseExpressionStatement();
    }

    Stmt* Parser::ParseStatement() {
        switch (Peek()->Kind) {
            case TokenKind::Semi: {
                Token& tok = Consume();
                return Stmt::Create(m_Context, tok.Range.Start, tok.Range, StmtKind::Nop, NopStmt());
            }

            case TokenKind::LeftParen:
            case TokenKind::Minus:
            case TokenKind::Bang:
            case TokenKind::Self:
            case TokenKind::True:
            case TokenKind::False:
            case TokenKind::CharLit:
            case TokenKind::IntLit:
            case TokenKind::UintLit:
            case TokenKind::NumLit:
            case TokenKind::StrLit:
                return ParseExpressionStatement();

            case TokenKind::Identifier: {
                if (IsType()) {
                    return ParseDeclarationStatement();
                } else {
                    return ParseExpressionStatement();
                }
            }

            case TokenKind::LeftCurly:
                return ParseBlock();

            case TokenKind::If:
                return ParseIf();

            case TokenKind::While:
                return ParseWhile();

            case TokenKind::Do:
                return ParseDoWhile();

            case TokenKind::For:
                return ParseFor();

            case TokenKind::Return:
                return ParseReturn();

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
                return ParseDeclarationStatement();

            case TokenKind::Fn: {
                Token& tok = Consume();
                m_Context->ReportCompilerError(tok.Range.Start, tok.Range, fmt::format("Function declaration is not allowed here"));
                return m_ErrorStmt;
            }

            case TokenKind::Struct: {
                Token& tok = Consume();
                m_Context->ReportCompilerError(tok.Range.Start, tok.Range, fmt::format("Structure declaration is not allowed here"));
                return m_ErrorStmt;
            }

            case TokenKind::Plus:
            case TokenKind::PlusEq:
            case TokenKind::MinusEq:
            case TokenKind::Star:
            case TokenKind::StarEq:
            case TokenKind::Slash:
            case TokenKind::SlashEq:
            case TokenKind::Percent:
            case TokenKind::PercentEq:
            case TokenKind::Ampersand:
            case TokenKind::AmpersandEq:
            case TokenKind::DoubleAmpersand:
            case TokenKind::Pipe:
            case TokenKind::PipeEq:
            case TokenKind::DoublePipe:
            case TokenKind::UpArrow:
            case TokenKind::UpArrowEq:
            case TokenKind::Eq:
            case TokenKind::EqEq:
            case TokenKind::BangEq:
            case TokenKind::Less:
            case TokenKind::LessEq:
            case TokenKind::Greater:
            case TokenKind::GreaterEq: {
                Token& tok = Consume();
                m_Context->ReportCompilerError(tok.Range.Start, tok.Range, fmt::format("Unexpected binary operator '{}' while looking for statement", TokenKindToString(tok.Kind)));
                return m_ErrorStmt;
            }

            case TokenKind::RightParen:
            case TokenKind::LeftBracket:
            case TokenKind::RightBracket:
            case TokenKind::RightCurly:
            case TokenKind::Comma:
            case TokenKind::Colon:
            case TokenKind::Dot: {
                Token& tok = Consume();
                m_Context->ReportCompilerError(tok.Range.Start, tok.Range, fmt::format("Unexpected token '{}' while looking for statement", TokenKindToString(tok.Kind)));
                return m_ErrorStmt;
            }

            case TokenKind::Break:
                Consume(); ARIA_ASSERT(false, "todo!"); return nullptr;

            case TokenKind::Last: ARIA_UNREACHABLE();
        }

        ARIA_UNREACHABLE();
        return nullptr;
    }

    Decl* Parser::ParseVariableDecl() {
        ARIA_ASSERT(IsType(), "Parser::ParseVariableDecl() expects the current token to be a type");

        SourceLocation start = Peek()->Range.Start;
        TypeInfo* type = ParseType();
        Token* ident = nullptr;
        
        ASSIGN_OR_RET(ident, TryConsume(TokenKind::Identifier, "identifier"), m_ErrorDecl);
        Expr* value = nullptr;

        if (Match(TokenKind::Eq)) {
            Consume();
            value = ParseExpression();
        }

        TryConsume(TokenKind::Semi, ";");

        return Decl::Create(m_Context, ident->Range.Start, SourceRange(start, Peek(-1)->Range.End), DeclKind::Var, VarDecl(ident->String, type, value));
    }

    Decl* Parser::ParseFunctionDecl() {
        SourceLocation start = Peek()->Range.Start;
        Token fn = Consume(); // Consume "fn"

        if (!IsType()) {
            m_Context->ReportCompilerError(Peek()->Range.Start, Peek()->Range, "Expected a type");
            StabilizeParser();
            return nullptr;
        }

        TypeInfo* type = ParseType();
        Token* ident = nullptr;
        ASSIGN_OR_RET(ident, TryConsume(TokenKind::Identifier, "identifier"), m_ErrorDecl);

        CONSUME_OR_RET(LeftParen, m_ErrorDecl);

        TinyVector<Decl*> params;
        TinyVector<TypeInfo*> paramTypes;

        while (!Match(TokenKind::RightParen)) {
            if (!IsType()) {
                m_Context->ReportCompilerError(Peek()->Range.Start, Peek()->Range, "Expected a type");
                StabilizeParser();
                continue;
            }

            TypeInfo* paramType = ParseType();
            Token* paramIdent = TryConsume(TokenKind::Identifier, "identifier");
            
            if (!paramIdent) {
                StabilizeParser();
                continue;
            }

            params.Append(m_Context, Decl::Create(m_Context, paramIdent->Range.Start, paramIdent->Range, DeclKind::Param, ParamDecl(paramIdent->String, paramType)));
            paramTypes.Append(m_Context, paramType);

            if (Match(TokenKind::Comma)) { Consume(); continue; }
            if (Match(TokenKind::RightParen)) { break; }

            StabilizeParser();
            m_Context->ReportCompilerError(Peek()->Range.Start, Peek()->Range, "expected either ',' or ')'");
        }

        CONSUME_OR_RET(RightParen, m_ErrorDecl);

        Stmt* body = nullptr;

        if (Match(TokenKind::LeftCurly)) {
            body = ParseBlock();
        } else {
            CONSUME_OR_RET(Semi, m_ErrorDecl);
        }

        FunctionDeclaration typeDecl;
        typeDecl.ReturnType = type;
        typeDecl.ParamTypes = paramTypes;

        TypeInfo* finalType = TypeInfo::Create(m_Context, PrimitiveType::Function, false, typeDecl);

        return Decl::Create(m_Context, ident->Range.Start, SourceRange(start, Peek(-1)->Range.End), DeclKind::Function, FunctionDecl(ident->String, finalType, params, body));
    }

    Decl* Parser::ParseStructDecl() {
        Token s = Consume(); // Consume "struct"

        // Token* ident = TryConsume(TokenKind::Identifier, "indentifier");
        // 
        // if (!ident) { return nullptr; }
        // m_DeclaredTypes[fmt::format("{}", ident->String)] = true;
        // 
        // TinyVector<Decl*> fields;
        // 
        // TryConsume(TokenKind::LeftCurly, "'{'");
        // 
        // while (!Match(TokenKind::RightCurly)) {
        //     if (IsVariableType()) {
        //         StringBuilder type = ParseVariableType();
        // 
        //         Token* fieldName = TryConsume(TokenKind::Identifier, "identifier");
        //         if (!fieldName) { return nullptr; }
        // 
        //         if (Match(TokenKind::LeftParen)) {
        //             Consume();
        // 
        //             TinyVector<ParamDecl*> params = ParseFunctionParameters();
        //             TryConsume(TokenKind::RightParen, "')'");
        // 
        //             Stmt* body = ParseCompound();
        //             fields.Append(m_Context, m_Context->Allocate<MethodDecl>(m_Context, fieldName->String, StringView(type.Data(), type.Size()), params, GetNode<CompoundStmt>(body)));
        //         } else {
        //             TryConsume(TokenKind::Semi, "';'");
        // 
        //             fields.Append(m_Context, m_Context->Allocate<FieldDecl>(m_Context, fieldName->String, StringView(type.Data(), type.Size())));
        //         }
        //     } else {
        //         m_Context->ReportCompilerError(Peek()->Range.Start, Peek()->Range, "expected a type name");
        //         StabilizeParser();
        //         TryConsume(TokenKind::Semi, "';'");
        //     }
        // }
        // 
        // TryConsume(TokenKind::RightCurly, "'}'");
        // 
        // m_NeedsSemi = false;
        // return m_Context->Allocate<StructDecl>(m_Context, ident->String, fields);
        ARIA_ASSERT(false, "todo!");
    }

    Stmt* Parser::ParseGlobal() {
        switch (Peek()->Kind) {
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
                return ParseDeclarationStatement();

            case TokenKind::Identifier: {
                if (m_DeclaredTypes.contains(fmt::format("{}", Peek()->String))) {
                    return ParseDeclarationStatement();
                }

                m_Context->ReportCompilerError(Peek()->Range.Start, Peek()->Range, fmt::format("Expected the start of a global declaration", TokenKindToString(Peek()->Kind)));
                StabilizeParser();
                return m_ErrorStmt;
            }

            case TokenKind::Fn: {
                Decl* decl = nullptr;
                ASSIGN_OR_RET(decl, ParseFunctionDecl(), m_ErrorStmt);

                return Stmt::Create(m_Context, decl->Loc, decl->Range, StmtKind::Decl, decl);
            }

            case TokenKind::Struct: {
                Decl* decl = nullptr;
                ASSIGN_OR_RET(decl, ParseStructDecl(), m_ErrorStmt);

                return Stmt::Create(m_Context, decl->Loc, decl->Range, StmtKind::Decl, decl);
            }

            case TokenKind::Semi: {
                m_Context->ReportCompilerError(Peek()->Range.Start, Peek()->Range, fmt::format("Token ';' was unexpected, try removing it")); 
                Consume(); 
                return m_ErrorStmt;
            }

            default: {
                m_Context->ReportCompilerError(Peek()->Range.Start, Peek()->Range, fmt::format("Expected the start of a global declaration", TokenKindToString(Peek()->Kind)));
                StabilizeParser();
                return m_ErrorStmt;
            }
        }
    }

    void Parser::StabilizeParser() {
        while (Peek()) {
            TokenKind type = Peek()->Kind;

            if (type == TokenKind::Semi || type == TokenKind::LeftCurly || type == TokenKind::RightCurly || type == TokenKind::Comma) {
                return;
            }

            Consume();
        }
    }

    void Parser::ErrorExpected(const std::string& expect, SourceLocation loc, SourceRange range) {
        if (Peek()) {
            m_Context->ReportCompilerError(loc, range, fmt::format("Expected '{}' but got '{}'", expect, TokenKindToString(Peek()->Kind)));
        } else {
            m_Context->ReportCompilerError(loc, range, fmt::format("Expected '{}' but got EOF", expect));
        }
    }

    bool Parser::ExprOk(Expr* expr) {
        if (expr == nullptr) { return true; }
        if (expr->Kind == ExprKind::Error) { return false; }

        return true;
    }

    bool Parser::DeclOk(Decl* decl) {
        if (decl == nullptr) { return true; }
        if (decl->Kind == DeclKind::Error) { return false; }

        return true;
    }

} // namespace Aria::Internal
