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
    
                final = Expr::Create(m_Context, t.Range.Start, t.Range, ExprKind::BooleanConstant, 
                    ExprValueKind::RValue, TypeInfo::Create(m_Context, PrimitiveType::Bool, false), 
                    BooleanConstantExpr(false));
                break;
            }
    
            case TokenKind::True: {
                Token& t = Consume();
    
                final = Expr::Create(m_Context, t.Range.Start, t.Range, ExprKind::BooleanConstant, 
                    ExprValueKind::RValue, TypeInfo::Create(m_Context, PrimitiveType::Bool, false), 
                    BooleanConstantExpr(true));
                break;
            }
    
            case TokenKind::CharLit: {
                Token& t = Consume();
    
                int8_t ch = static_cast<int8_t>(value.String.Data()[0]);
                final = Expr::Create(m_Context, t.Range.Start, t.Range, ExprKind::CharacterConstant, 
                    ExprValueKind::RValue, TypeInfo::Create(m_Context, PrimitiveType::Char, false), 
                    CharacterConstantExpr(ch));
                break;
            }
    
            case TokenKind::IntLit: {
                Token& t = Consume();

                final = Expr::Create(m_Context, t.Range.Start, t.Range, ExprKind::IntegerConstant,
                    ExprValueKind::RValue, TypeInfo::Create(m_Context, PrimitiveType::Long, false), 
                    IntegerConstantExpr(t.Integer));
                break;
            }

            case TokenKind::UintLit: {
                Token& t = Consume();

                final = Expr::Create(m_Context, t.Range.Start, t.Range, ExprKind::IntegerConstant, 
                    ExprValueKind::RValue, TypeInfo::Create(m_Context, PrimitiveType::ULong, false), 
                    IntegerConstantExpr(t.Integer));
                break;
            }
    
            case TokenKind::NumLit: {
                Token& t = Consume();
    
                final = Expr::Create(m_Context, t.Range.Start, t.Range, ExprKind::FloatingConstant,
                    ExprValueKind::RValue, TypeInfo::Create(m_Context, PrimitiveType::Double, false), 
                    FloatingConstantExpr(t.Number));
                break;
            }
    
            case TokenKind::StrLit: {
                Token& t = Consume();
    
                final = Expr::Create(m_Context, t.Range.Start, t.Range, ExprKind::StringConstant,
                    ExprValueKind::RValue, TypeInfo::Create(m_Context, PrimitiveType::String, false), 
                    IntegerConstantExpr(t.Integer));
                break;
            }
    
            case TokenKind::Minus: {
                Token& m = Consume();
                Expr* subExpr = ParseValue();

                final = Expr::Create(m_Context, m.Range.Start, m.Range, ExprKind::UnaryOperator,
                    ExprValueKind::RValue, nullptr,
                    UnaryOperatorExpr(subExpr, UnaryOperatorKind::Negate));
                break;
            }
    
            case TokenKind::LeftParen: {
                Token p = Consume();
    
                if (IsType()) {
                    TypeInfo* type = ParseType();

                    TryConsume(TokenKind::RightParen, "')'");
                    Expr* subExpr = ParseValue();

                    final = Expr::Create(m_Context, p.Range.Start, SourceRange(p.Range.Start, subExpr->Range.End), ExprKind::Cast,
                        subExpr->ValueKind, subExpr->Type, 
                        CastExpr(subExpr, type));
                } else {
                    Expr* subExpr = ParseExpression();
                    Token* rp = TryConsume(TokenKind::RightParen, "')'");

                    if (!rp) { return nullptr; }

                    final = Expr::Create(m_Context, p.Range.Start, SourceRange(p.Range.Start, rp->Range.End), ExprKind::Paren,
                        subExpr->ValueKind, subExpr->Type, 
                        ParenExpr(subExpr));
                }
                
                break;
            }

            case TokenKind::Self: {
                Token s = Consume();

                final = Expr::Create(m_Context, s.Range.Start, s.Range, ExprKind::Self,
                    ExprValueKind::LValue, nullptr, 
                    SelfExpr());
                break;
            }
    
            case TokenKind::Identifier: {
                Token i = Consume();

                final = Expr::Create(m_Context, i.Range.Start, i.Range, ExprKind::DeclRef,
                    ExprValueKind::LValue, nullptr, 
                    DeclRefExpr(i.String));

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
    
                    final = Expr::Create(m_Context, lp.Range.Start, SourceRange(i.Range.Start, rp->Range.End), ExprKind::Call,
                        ExprValueKind::RValue, nullptr, 
                        CallExpr(final, args));
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

                final = Expr::Create(m_Context, op.Range.Start, SourceRange(final->Range.Start, member->Range.End), ExprKind::Member,
                    ExprValueKind::LValue, nullptr, 
                    MemberExpr(member->String, final));

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

                    final = Expr::Create(m_Context, lp.Range.Start, SourceRange(final->Range.Start, rp->Range.End), ExprKind::MethodCall,
                        ExprValueKind::LValue, nullptr, 
                        MethodCallExpr(final, args));
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
                lhsExpr = Expr::Create(m_Context, o.Range.Start, SourceRange(lhsLoc, Peek(-1)->Range.End), ExprKind::CompoundAssign, 
                    ExprValueKind::LValue, nullptr, 
                    CompoundAssignExpr(lhsExpr, rhsExpr, op));
            } else {
                lhsExpr = Expr::Create(m_Context, o.Range.Start, SourceRange(lhsLoc, Peek(-1)->Range.End), ExprKind::BinaryOperator, 
                    ExprValueKind::RValue, nullptr, 
                    BinaryOperatorExpr(lhsExpr, rhsExpr, op));
            }

            lhsLoc = Peek()->Range.Start;
        }

        return lhsExpr;
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

        TypeInfo* type = TypeInfo::Create(m_Context, PrimitiveType::Invalid, false);

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

    Decl* Parser::ParseTypeDecl() {
        SourceLocation start = Peek()->Range.Start;
        TypeInfo* type = ParseType();

        if (Peek(1) && Peek(1)->Kind == TokenKind::LeftParen) {
            return ParseFunctionDecl(type, start);
        } else {
            return ParseVariableDecl(type, start);
        }
    }

    Decl* Parser::ParseVariableDecl(TypeInfo* type, SourceLocation start) {
        Token* ident = TryConsume(TokenKind::Identifier, "identifier");
        Expr* value = nullptr;

        if (ident) {
            if (Match(TokenKind::Eq)) {
                Consume();
                value = ParseExpression();
            }

            return Decl::Create(m_Context, ident->Range.Start, SourceRange(start, Peek(-1)->Range.End), DeclKind::Var, VarDecl(ident->String, type, value));
        } else {
            StabilizeParser();
            return nullptr;
        }
    }

    Decl* Parser::ParseFunctionDecl(TypeInfo* type, SourceLocation start) {
        Token* ident = TryConsume(TokenKind::Identifier, "identifier");

        if (ident) {
            TinyVector<ParamDecl> params;

            if (Match(TokenKind::LeftParen)) {
                Consume();

                while (!Match(TokenKind::RightParen)) {
                    if (!IsType()) {
                        StabilizeParser();
                        m_Context->ReportCompilerError(Peek(-1)->Range.Start, Peek(-1)->Range, "expected a type");
                        continue;
                    }

                    TypeInfo* type = ParseType();
                    Token* ident = TryConsume(TokenKind::Identifier, "identifier");
                    if (!ident) {
                        StabilizeParser();
                        continue;
                    }

                    params.Append(m_Context, ParamDecl(ident->String, type));

                    if (Match(TokenKind::Comma)) { continue; }
                    if (Match(TokenKind::RightParen)) { break; }

                    StabilizeParser();
                    m_Context->ReportCompilerError(Peek()->Range.Start, Peek()->Range, "expected either ',' or ')'");
                }

                TryConsume(TokenKind::RightParen, "')'");

                BlockStmt body;

                if (Match(TokenKind::LeftCurly)) {
                    body = ParseBlock()->Block;
                } else {
                    Token* semi = TryConsume(TokenKind::Semi, "';'");

                    if (!semi) {
                        StabilizeParser();
                    }
                }

                return Decl::Create(m_Context, ident->Range.Start, SourceRange(start, Peek(-1)->Range.End), DeclKind::Function, FunctionDecl(ident->String, type, params, body));
            } else {
                StabilizeParser();
                ErrorExpected("'('", Peek()->Range.End, Peek()->Range);
            }

            return nullptr;
        } else {
            StabilizeParser();
            return nullptr;
        }
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

    Stmt* Parser::ParseBlock() {
        TinyVector<Stmt*> stmts;
        Token* l = TryConsume(TokenKind::LeftCurly, "'{'");
        if (!l) { return nullptr; }

        while (!Match(TokenKind::RightCurly)) {
            Stmt* stmt = ParseToken();

            Token* semi = TryConsume(TokenKind::Semi, "';'");

            if (stmt != nullptr && semi != nullptr) {
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
            Stmt* stmt = ParseToken();
            if (!stmt) { return nullptr; }

            Token* semi = TryConsume(TokenKind::Semi, "';'");
            if (!semi) { return nullptr; }

            TinyVector<Stmt*> stmts;
            stmts.Append(m_Context, stmt);

            return Stmt::Create(m_Context, stmt->Loc, stmt->Range, StmtKind::Block, BlockStmt(stmts));
        }
    }

    Stmt* Parser::ParseWhile() {
        Token w = Consume(); // Consume "while"

        Expr* condition = ParseExpression();
        BlockStmt body = ParseBlockInline()->Block;

        return Stmt::Create(m_Context, w.Range.Start, SourceRange(w.Range.Start, Peek(-1)->Range.End), StmtKind::While, WhileStmt(condition, body));
    }

    Stmt* Parser::ParseDoWhile() {
        Token d = Consume(); // Consume "do"

        // Stmt* body = ParseCompoundInline();
        // TryConsume(TokenKind::While, "while");
        // Expr* condition = ParseExpression();
        // 
        // return Stmt::Create(m_Context, w.Range.Start, SourceRange(w.Range.Start, Peek(-1)->Range.End), StmtKind::DoWhile, DoWhileStmt(condition, body));
        ARIA_ASSERT(false, "todo!");
    }

    Stmt* Parser::ParseFor() {
        Token f = Consume(); // Consume "for"

        // TryConsume(TokenKind::LeftParen, "'('");
        // 
        // Stmt* prologue = ParseStatement();
        // TryConsume(TokenKind::Semi, "';'");
        // Expr* condition = ParseExpression();
        // TryConsume(TokenKind::Semi, "';'");
        // Expr* epilogue = ParseExpression();
        // TryConsume(TokenKind::RightParen, "')'");
        // 
        // Stmt* body = ParseCompoundInline();
        // m_NeedsSemi = false;
        // 
        // return m_Context->Allocate<ForStmt>(m_Context, prologue, condition, epilogue, body);
        ARIA_ASSERT(false, "todo!");
    }

    Stmt* Parser::ParseIf() {
        // Token i = Consume(); // Consume "if"
        // 
        // Expr* condition = ParseExpression();
        // Stmt* body = ParseStatement();
        // 
        // m_NeedsSemi = false;
        // return m_Context->Allocate<IfStmt>(m_Context, condition, body, nullptr);
        ARIA_ASSERT(false, "todo!");
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
        // Token r = Consume(); // Consume "return"
        // 
        // Expr* value = ParseExpression();
        // 
        // ReturnStmt* ret = m_Context->Allocate<ReturnStmt>(m_Context, value);
        // return ret;
        ARIA_ASSERT(false, "todo!");
    }

    Stmt* Parser::ParseStatement() {
        TokenKind t = Peek()->Kind;

        Stmt* node = nullptr;

        if (IsType()) {
            Decl* decl = ParseTypeDecl();
            node = Stmt::Create(m_Context, decl->Loc, decl->Range, StmtKind::Decl, decl);
        } else if (t == TokenKind::Struct) {
            Decl* decl = ParseStructDecl();
            node = Stmt::Create(m_Context, decl->Loc, decl->Range, StmtKind::Decl, decl);
        } else if (t == TokenKind::LeftCurly) {
            node = ParseBlock();
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
            s = Stmt::Create(m_Context, expr->Loc, expr->Range, StmtKind::Expr, expr);
        }

        while (Match(TokenKind::Semi)) {
            Consume();
        }

        return s;
    }

    void Parser::StabilizeParser() {
        while (Peek()) {
            TokenKind type = Peek()->Kind;

            if (type == TokenKind::Semi || type == TokenKind::RightCurly || type == TokenKind::Comma) {
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
