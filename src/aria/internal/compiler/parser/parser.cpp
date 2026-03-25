#include "aria/internal/compiler/parser/parser.hpp"
#include "aria/internal/compiler/ast/ast.hpp"

#include "fmt/format.h"

#include <string>
#include <charconv>
#include <limits>

namespace Aria::Internal {

    Parser::Parser(CompilationContext* ctx) {
        m_Context = ctx;
        m_Tokens = ctx->GetTokens();

        ParseImpl();
    }

    void Parser::ParseImpl() {
        TinyVector<Stmt*> stmts;
        while (Peek()) {
            Stmt* stmt = ParseToken();
            if (stmt != nullptr) {
                stmts.Append(m_Context, stmt);
            }
        }

        TranslationUnitDecl* root = m_Context->Allocate<TranslationUnitDecl>(m_Context, stmts);
        m_Context->SetRootASTNode(root);
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

    StringBuilder Parser::ParseVariableType() {
        Token type = Consume();

        StringBuilder strType;

        switch (type.Kind) {
            case TokenKind::Void:       strType.Append(m_Context, "void"); break;
            case TokenKind::Bool:       strType.Append(m_Context, "bool"); break;
            case TokenKind::Char:       strType.Append(m_Context, "char"); break;
            case TokenKind::UChar:      strType.Append(m_Context, "uchar"); break;
            case TokenKind::Short:      strType.Append(m_Context, "short"); break;
            case TokenKind::UShort:     strType.Append(m_Context, "ushort"); break;
            case TokenKind::Int:        strType.Append(m_Context, "int"); break;
            case TokenKind::UInt:       strType.Append(m_Context, "uint"); break;
            case TokenKind::Long:       strType.Append(m_Context, "long"); break;
            case TokenKind::ULong:      strType.Append(m_Context, "ulong"); break;
            case TokenKind::Float:      strType.Append(m_Context, "float"); break;
            case TokenKind::Double:     strType.Append(m_Context, "double"); break;
            case TokenKind::String:     strType.Append(m_Context, "string"); break;
            case TokenKind::Identifier: strType.Append(m_Context, type.String); break;
            default:                    strType.Append(m_Context, ""); break;
        }

        if (Match(TokenKind::LeftBracket)) {
            Consume();
            TryConsume(TokenKind::RightBracket, "']'");
            strType.Append(m_Context, "[]");
        }

        if (Match(TokenKind::Ampersand)) {
            Consume();
            strType.Append(m_Context, "&");
        }

        return strType;
    }

    TinyVector<ParamDecl*> Parser::ParseFunctionParameters() {
        TinyVector<ParamDecl*> params;

        while (!Match(TokenKind::RightParen)) {
            StringBuilder type = ParseVariableType();

            Token& ident = Consume();
            
            ParamDecl* param = m_Context->Allocate<ParamDecl>(m_Context, ident.String, StringView(type.Data(), type.Size()));
            
            if (Match(TokenKind::Comma)) {
                Consume();
            }

            params.Append(m_Context, param);
        }

        return params;
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

    bool Parser::IsVariableType() {
        if (IsPrimitiveType()) { return true; }

        if (Peek()->Kind == TokenKind::Identifier) {
            if (m_DeclaredTypes.contains(fmt::format("{}", Peek()->String))) {
                return true;
            }

            return false;
        }

        return false;
    }

    BinaryOperatorKind Parser::ParseOperator() {
        Token op = *Peek();

        switch (op.Kind) {
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
            case TokenKind::Ampersand: return BinaryOperatorKind::And;
            case TokenKind::AmpersandEq: return BinaryOperatorKind::CompoundAnd;
            case TokenKind::DoubleAmpersand: return BinaryOperatorKind::BitAnd;
            case TokenKind::Pipe: return BinaryOperatorKind::Or;
            case TokenKind::PipeEq: return BinaryOperatorKind::CompoundOr;
            case TokenKind::DoublePipe: return BinaryOperatorKind::BitOr;
            case TokenKind::UpArrow: return BinaryOperatorKind::Xor;
            case TokenKind::UpArrowEq: return BinaryOperatorKind::CompoundXor;
            case TokenKind::Less: return BinaryOperatorKind::Less;
            case TokenKind::LessOrEq: return BinaryOperatorKind::LessOrEq;
            case TokenKind::Greater: return BinaryOperatorKind::Greater;
            case TokenKind::GreaterOrEq: return BinaryOperatorKind::GreaterOrEq;
            case TokenKind::Eq: return BinaryOperatorKind::Eq;
            case TokenKind::IsEq: return BinaryOperatorKind::IsEq;
            case TokenKind::IsNotEq: return BinaryOperatorKind::IsNotEq;
            default: return BinaryOperatorKind::Invalid;
        }
    }

    size_t Parser::GetBinaryPrecedence(BinaryOperatorKind kind) {
        switch (kind) {
            case BinaryOperatorKind::Eq:
            case BinaryOperatorKind::CompoundAdd:
            case BinaryOperatorKind::CompoundSub:
            case BinaryOperatorKind::CompoundMul:
            case BinaryOperatorKind::CompoundMod:
            case BinaryOperatorKind::CompoundDiv:
            case BinaryOperatorKind::CompoundAnd:
            case BinaryOperatorKind::CompoundOr:
            case BinaryOperatorKind::CompoundXor:
                return 10;

            case BinaryOperatorKind::Less:
            case BinaryOperatorKind::LessOrEq:
            case BinaryOperatorKind::Greater:
            case BinaryOperatorKind::GreaterOrEq:
            case BinaryOperatorKind::IsEq:
            case BinaryOperatorKind::IsNotEq:
            case BinaryOperatorKind::BitAnd:
            case BinaryOperatorKind::BitOr:
                return 20;

            case BinaryOperatorKind::And:
            case BinaryOperatorKind::Or:
            case BinaryOperatorKind::Xor:
                return 30;

            case BinaryOperatorKind::Add:
            case BinaryOperatorKind::Sub:
                return 40;

            case BinaryOperatorKind::Mod:
            case BinaryOperatorKind::Mul:
            case BinaryOperatorKind::Div:
                return 50;
        }

        ARIA_ASSERT(false, "Unreachable");
        return -1;
    }

    size_t Parser::GetNextPrecedence(BinaryOperatorKind binop) {
        switch (binop) {
            case BinaryOperatorKind::Eq:
            case BinaryOperatorKind::CompoundAdd:
            case BinaryOperatorKind::CompoundSub:
            case BinaryOperatorKind::CompoundMul:
            case BinaryOperatorKind::CompoundDiv:
            case BinaryOperatorKind::CompoundMod:
            case BinaryOperatorKind::CompoundAnd:
            case BinaryOperatorKind::CompoundOr:
            case BinaryOperatorKind::CompoundXor:
                return GetBinaryPrecedence(binop); // Right associative

            default: return GetBinaryPrecedence(binop) + 1; // Left associative
        }
    }

    Expr* Parser::ParseValue() {
        if (!Peek()) { return nullptr; }
        Token& value = *Peek();
    
        Expr* final = nullptr;

        switch (value.Kind) {
            case TokenKind::False: {
                Token& t = Consume();
    
                final = m_Context->Allocate<BooleanConstantExpr>(m_Context, t.Range.Start, t.Range, false);
                break;
            }
    
            case TokenKind::True: {
                Token& t = Consume();
    
                final = m_Context->Allocate<BooleanConstantExpr>(m_Context, t.Range.Start, t.Range, true);
                break;
            }
    
            case TokenKind::CharLit: {
                Token& t = Consume();
    
                int8_t ch = static_cast<int8_t>(value.String.Data()[0]);
                final = m_Context->Allocate<CharacterConstantExpr>(m_Context, t.Range.Start, t.Range, ch);
                break;
            }
    
            case TokenKind::IntLit: {
                Token& t = Consume();

                final = m_Context->Allocate<IntegerConstantExpr>(m_Context, t.Range.Start, t.Range, t.Integer, TypeInfo::Create(m_Context, PrimitiveType::Long, false));
                break;
            }

            case TokenKind::UintLit: {
                Token& t = Consume();

                final = m_Context->Allocate<IntegerConstantExpr>(m_Context, t.Range.Start, t.Range, t.Integer, TypeInfo::Create(m_Context, PrimitiveType::ULong, false));
                break;
            }
    
            case TokenKind::NumLit: {
                Token& t = Consume();
    
                final = m_Context->Allocate<FloatingConstantExpr>(m_Context, t.Range.Start, t.Range, t.Number);
                break;
            }
    
            case TokenKind::StrLit: {
                Token& t = Consume();
    
                final = m_Context->Allocate<StringConstantExpr>(m_Context, t.Range.Start, t.Range, t.String);
                break;
            }
    
            case TokenKind::Minus: {
                Token& m = Consume();
    
                Expr* expr = ParseValue();
                final = m_Context->Allocate<UnaryOperatorExpr>(m_Context, m.Range.Start, SourceRange(m.Range.Start, expr->Range.End), expr, UnaryOperatorKind::Negate);
                break;
            }
    
            case TokenKind::LeftParen: {
                Token p = Consume();
    
                if (IsVariableType()) {
                    StringBuilder type = ParseVariableType();

                    TryConsume(TokenKind::RightParen, "')'");
                    Expr* expr = ParseValue();

                    final = m_Context->Allocate<CastExpr>(m_Context, p.Range.Start, SourceRange(p.Range.Start, expr->Range.End), expr, StringView(type.Data(), type.Size()));
                } else {
                    Expr* expr = ParseExpression();
                    Token* rp = TryConsume(TokenKind::RightParen, "')'");

                    if (!rp) { return nullptr; }
                    final = m_Context->Allocate<ParenExpr>(m_Context, p.Range.Start, SourceRange(p.Range.Start, rp->Range.End), expr);
                }
                
                break;
            }

            case TokenKind::Self: {
                Token s = Consume();

                final = m_Context->Allocate<SelfExpr>(m_Context, s.Range.Start, s.Range);
                break;
            }
    
            case TokenKind::Identifier: {
                Token i = Consume();

                final = m_Context->Allocate<DeclRefExpr>(m_Context, i.Range.Start, i.Range, i.String);

                // Check if this is a function call
                if (Match(TokenKind::LeftParen)) {
                    Token& lp = Consume();
    
                    TinyVector<Expr*> args;

                    while (!Match(TokenKind::RightParen)) {
                        Expr* val = ParseExpression();
    
                        if (Match(TokenKind::Comma)) {
                            Consume();
                        }
    
                        args.Append(m_Context, val);
                    }
    
                    Token* rp = TryConsume(TokenKind::RightParen, "')'");
                    if (!rp) { return nullptr; }
    
                    final = m_Context->Allocate<CallExpr>(m_Context, lp.Range.Start, SourceRange(i.Range.Start, rp->Range.End), GetNode<DeclRefExpr>(final), args);
                }

                break;
            }
        }

        // Handle member access (foo.bar) and member call expressions (foo.add())
        while (Match(TokenKind::Dot)) {
            Token op = Consume();

            if (op.Kind == TokenKind::Dot) {
                Token* member = TryConsume(TokenKind::Identifier, "identifier");
                if (!member) { return nullptr; }

                final = m_Context->Allocate<MemberExpr>(m_Context, op.Range.Start, SourceRange(final->Range.Start, member->Range.End), member->String, final);

                if (Match(TokenKind::LeftParen)) {
                    Token& lp = Consume();
    
                    TinyVector<Expr*> args;
                    while (!Match(TokenKind::RightParen)) {
                        Expr* val = ParseExpression();
    
                        if (Match(TokenKind::Comma)) {
                            Consume();
                        }
    
                        args.Append(m_Context, val);
                    }
    
                    Token* rp = TryConsume(TokenKind::RightParen, "')'");
                    if (!rp) { return nullptr; }

                    final = m_Context->Allocate<MethodCallExpr>(m_Context, lp.Range.Start, SourceRange(final->Range.Start, rp->Range.End), GetNode<MemberExpr>(final), args);
                }
            }
        }

        return final;
    }

    Expr* Parser::ParseExpression(size_t minbp) {
        SourceLocation lhsLoc = Peek()->Range.Start;
        Expr* lhsExpr = ParseValue();

        if (!lhsExpr) { return nullptr; }

        // There are two conditions to this loop
        // A valid operator (the first half of the check) and a valid precedence (the second half)
        while ((Peek() && ParseOperator() != BinaryOperatorKind::Invalid) && 
               (GetBinaryPrecedence(ParseOperator()) >= minbp)) {
            BinaryOperatorKind op = ParseOperator();
            Token o = Consume();

            Expr* rhsExpr = ParseExpression(GetNextPrecedence(op));
            if (!rhsExpr) { return nullptr; }

            if (op == BinaryOperatorKind::CompoundAdd ||
                op == BinaryOperatorKind::CompoundSub ||
                op == BinaryOperatorKind::CompoundMul ||
                op == BinaryOperatorKind::CompoundDiv ||
                op == BinaryOperatorKind::CompoundMod ||
                op == BinaryOperatorKind::CompoundAnd ||
                op == BinaryOperatorKind::CompoundOr  ||
                op == BinaryOperatorKind::CompoundXor) {
                lhsExpr = m_Context->Allocate<CompoundAssignExpr>(m_Context, o.Range.Start, SourceRange(lhsLoc, Peek(-1)->Range.End), lhsExpr, rhsExpr, op);
            } else {
                lhsExpr = m_Context->Allocate<BinaryOperatorExpr>(m_Context, o.Range.Start, SourceRange(lhsLoc, Peek(-1)->Range.End), lhsExpr, rhsExpr, op);
            }

            lhsLoc = Peek()->Range.Start;
        }

        return lhsExpr;
    }

    Stmt* Parser::ParseCompound() {
        TinyVector<Stmt*> stmts;
        Token* l = TryConsume(TokenKind::LeftCurly, "'{'");

        while (!Match(TokenKind::RightCurly)) {
            Stmt* stmt = ParseToken();
            if (stmt != nullptr) {
                stmts.Append(m_Context, stmt);
            }
        }

        TryConsume(TokenKind::RightCurly, "'}'");

        return m_Context->Allocate<CompoundStmt>(m_Context, stmts);
    }

    Stmt* Parser::ParseCompoundInline() {
        if (Match(TokenKind::LeftCurly)) {
            return ParseCompound();
        } else {
            return ParseToken();
        }
    }

    Stmt* Parser::ParseType(bool external) {
        StringBuilder type = ParseVariableType();

        if (Peek(1)) {
            if (Peek(1)->Kind == TokenKind::LeftParen) {
                return ParseFunctionDecl(type, Peek(-1)->Range, external);
            }
        }

        return ParseVariableDecl(type, Peek(-1)->Range);
    }

    Stmt* Parser::ParseVariableDecl(StringBuilder type, SourceRange start) {
        Token* ident = TryConsume(TokenKind::Identifier, "identifier");
        Expr* value = nullptr;

        if (ident) {
            if (Match(TokenKind::Eq)) {
                Consume();
                value = ParseExpression();
            }

            return m_Context->Allocate<VarDecl>(m_Context, ident->String, StringView(type.Data(), type.Size()), value);
        } else {
            StabilizeParser();
            return nullptr;
        }
    }

    Stmt* Parser::ParseFunctionDecl(StringBuilder returnType, SourceRange start, bool external) {
        Token* ident = TryConsume(TokenKind::Identifier, "identifier");

        if (ident) {
            TinyVector<ParamDecl*> params;

            if (Match(TokenKind::LeftParen)) {
                Consume();
                params = ParseFunctionParameters();
                TryConsume(TokenKind::RightParen, "')'");

                Stmt* body = nullptr;

                if (Match(TokenKind::LeftCurly)) {
                    body = ParseCompound();
                    m_NeedsSemi = false;
                }

                return m_Context->Allocate<FunctionDecl>(m_Context, ident->String, StringView(returnType.Data(), returnType.Size()), params, external, GetNode<CompoundStmt>(body));
            } else {
                ErrorExpected("'('", Peek()->Range.End, Peek()->Range);
            }

            return nullptr;
        } else {
            return nullptr;
        }
    }

    Stmt* Parser::ParseExtern() {
        Consume();

        return ParseType(true);
    }

    Stmt* Parser::ParseStructDecl() {
        Token s = Consume(); // Consume "struct"

        Token* ident = TryConsume(TokenKind::Identifier, "indentifier");

        if (!ident) { return nullptr; }
        m_DeclaredTypes[fmt::format("{}", ident->String)] = true;

        TinyVector<Decl*> fields;

        TryConsume(TokenKind::LeftCurly, "'{'");

        while (!Match(TokenKind::RightCurly)) {
            if (IsVariableType()) {
                StringBuilder type = ParseVariableType();

                Token* fieldName = TryConsume(TokenKind::Identifier, "identifier");
                if (!fieldName) { return nullptr; }

                if (Match(TokenKind::LeftParen)) {
                    Consume();

                    TinyVector<ParamDecl*> params = ParseFunctionParameters();
                    TryConsume(TokenKind::RightParen, "')'");

                    Stmt* body = ParseCompound();
                    fields.Append(m_Context, m_Context->Allocate<MethodDecl>(m_Context, fieldName->String, StringView(type.Data(), type.Size()), params, GetNode<CompoundStmt>(body)));
                } else {
                    TryConsume(TokenKind::Semi, "';'");

                    fields.Append(m_Context, m_Context->Allocate<FieldDecl>(m_Context, fieldName->String, StringView(type.Data(), type.Size())));
                }
            } else {
                m_Context->ReportCompilerError(Peek()->Range.Start, Peek()->Range, "expected a type name");
                StabilizeParser();
                TryConsume(TokenKind::Semi, "';'");
            }
        }
        
        TryConsume(TokenKind::RightCurly, "'}'");

        m_NeedsSemi = false;
        return m_Context->Allocate<StructDecl>(m_Context, ident->String, fields);
    }

    Stmt* Parser::ParseWhile() {
        Token w = Consume(); // Consume "while"

        Expr* condition = ParseExpression();
        Stmt* body = ParseCompoundInline();

        m_NeedsSemi = false;

        return m_Context->Allocate<WhileStmt>(m_Context, condition, body);
    }

    Stmt* Parser::ParseDoWhile() {
        Token d = Consume(); // Consume "do"

        Stmt* body = ParseCompoundInline();
        TryConsume(TokenKind::While, "while");
        Expr* condition = ParseExpression();

        return m_Context->Allocate<DoWhileStmt>(m_Context, condition, body);
    }

    Stmt* Parser::ParseFor() {
        Token f = Consume(); // Consume "for"

        TryConsume(TokenKind::LeftParen, "'('");
        
        Stmt* prologue = ParseStatement();
        TryConsume(TokenKind::Semi, "';'");
        Expr* condition = ParseExpression();
        TryConsume(TokenKind::Semi, "';'");
        Expr* epilogue = ParseExpression();
        TryConsume(TokenKind::RightParen, "')'");

        Stmt* body = ParseCompoundInline();
        m_NeedsSemi = false;

        return m_Context->Allocate<ForStmt>(m_Context, prologue, condition, epilogue, body);
    }

    Stmt* Parser::ParseIf() {
        Token i = Consume(); // Consume "if"

        Expr* condition = ParseExpression();
        Stmt* body = ParseStatement();

        m_NeedsSemi = false;
        return m_Context->Allocate<IfStmt>(m_Context, condition, body, nullptr);
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

        ReturnStmt* ret = m_Context->Allocate<ReturnStmt>(m_Context, value);
        return ret;
    }

    Stmt* Parser::ParseStatement() {
        TokenKind t = Peek()->Kind;

        Stmt* node = nullptr;

        if (IsVariableType()) {
            node = ParseType();
        } else if (t == TokenKind::Extern) {
            node = ParseExtern();
        } else if (t == TokenKind::Struct) {
            node = ParseStructDecl();
        } else if (t == TokenKind::LeftCurly) {
            node = ParseCompound();
        } else if (t == TokenKind::While) {
            node = ParseWhile();
        } else if (t == TokenKind::Do) {
            node = ParseDoWhile();
        } else if (t == TokenKind::For) {
            node = ParseFor();
        } else if (t == TokenKind::If) {
            node = ParseIf();
        } else if (t == TokenKind::Break) {
            node = ParseBreak();
        } else if (t == TokenKind::Return) {
            node = ParseReturn();
        }

        return node;
    }

    Stmt* Parser::ParseToken() {
        Stmt* s = nullptr;

        if (Stmt* stmt = ParseStatement()) {
            s = stmt;
        } else if (Expr* expr = ParseExpression()) {
            s = expr;
        }

        if (m_NeedsSemi) {
            TryConsume(TokenKind::Semi, "';'");
        }

        while (Match(TokenKind::Semi)) {
            Consume();
        }

        m_NeedsSemi = true;

        return s;
    }

    void Parser::StabilizeParser() {
        while (Peek()) {
            TokenKind type = Peek()->Kind;

            if (type == TokenKind::Semi || type ==  TokenKind::RightCurly) {
                return;
            }

            Consume();
        }
    }

    void Parser::ErrorExpected(const std::string& expect, SourceLocation loc, SourceRange range) {
        m_Context->ReportCompilerError(loc, range, fmt::format("Expected {} but got \"{}\"", expect, TokenKindToString(Peek()->Kind)));
    }

    void Parser::ErrorTooLarge(const StringView value) {
        // m_Context->ReportCompilerError(Peek(-1)->Line, Peek(-1)->Column, fmt::format("Constant {} is too large", value));
    }

} // namespace Aria::Internal
