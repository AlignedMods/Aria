#include "black_lua/internal/compiler/parser/parser.hpp"
#include "black_lua/internal/compiler/ast/ast.hpp"

#include "fmt/format.h"

#include <string>
#include <charconv>
#include <limits>

namespace BlackLua::Internal {

    Parser::Parser(CompilationContext* ctx) {
        m_Context = ctx;
        m_Tokens = ctx->GetTokens();

        ParseImpl();
    }

    void Parser::ParseImpl() {
        TinyVector<Stmt*> stmts;
        while (Peek()) {
            Stmt* stmt = ParseToken();
            stmts.Append(m_Context, stmt);
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
        BLUA_ASSERT(m_Index < m_Tokens.size(), "Consume out of bounds!");

        return m_Tokens.at(m_Index++);
    }

    Token* Parser::TryConsume(TokenType type, const StringView error) {
        if (Match(type)) {
            Token& t = Consume();
            return &t;
        }

        ErrorExpected(error);

        return nullptr;
    }

    bool Parser::Match(TokenType type) {
        if (!Peek()) {
            return false;
        }

        if (Peek()->Type == type) {
            return true;
        } else {
            return false;
        }
    }

    StringBuilder Parser::ParseVariableType() {
        Token type = Consume();

        StringBuilder strType;

        switch (type.Type) {
            case TokenType::Void:       strType.Append(m_Context, "void"); break;
            case TokenType::Bool:       strType.Append(m_Context, "bool"); break;
            case TokenType::Char:       strType.Append(m_Context, "char"); break;
            case TokenType::UChar:      strType.Append(m_Context, "uchar"); break;
            case TokenType::Short:      strType.Append(m_Context, "short"); break;
            case TokenType::UShort:     strType.Append(m_Context, "ushort"); break;
            case TokenType::Int:        strType.Append(m_Context, "int"); break;
            case TokenType::UInt:       strType.Append(m_Context, "uint"); break;
            case TokenType::Long:       strType.Append(m_Context, "long"); break;
            case TokenType::ULong:      strType.Append(m_Context, "ulong"); break;
            case TokenType::Float:      strType.Append(m_Context, "float"); break;
            case TokenType::Double:     strType.Append(m_Context, "double"); break;
            case TokenType::String:     strType.Append(m_Context, "string"); break;
            case TokenType::Identifier: strType.Append(m_Context, type.Data); break;
            default:                    strType.Append(m_Context, ""); break;
        }

        if (Match(TokenType::LeftBracket)) {
            Consume();
            TryConsume(TokenType::RightBracket, "']'");
            strType.Append(m_Context, "[]");
        }

        return strType;
    }

    TinyVector<ParamDecl*> Parser::ParseFunctionParameters() {
        TinyVector<ParamDecl*> params;

        while (!Match(TokenType::RightParen)) {
            StringBuilder type = ParseVariableType();

            Token& ident = Consume();
            
            ParamDecl* param = m_Context->Allocate<ParamDecl>(m_Context, ident.Data, StringView(type.Data(), type.Size()));
            
            if (Match(TokenType::Comma)) {
                Consume();
            }

            params.Append(m_Context, param);
        }

        if (!Match(TokenType::RightParen)) {
            BLUA_ASSERT(false, "todo: add error");
        }

        return params;
    }

    bool Parser::IsPrimitiveType() {
        if (!Peek()) { return false; }

        Token type = *Peek();

        switch (type.Type) {
            case TokenType::Void:
            case TokenType::Bool:
            case TokenType::Char:
            case TokenType::UChar:
            case TokenType::Short:
            case TokenType::UShort:
            case TokenType::Int:
            case TokenType::UInt:
            case TokenType::Long:
            case TokenType::ULong:
            case TokenType::Float:
            case TokenType::Double:
            case TokenType::String: return true;
            default: return false;
        }

        return false;
    }

    bool Parser::IsVariableType() {
        if (IsPrimitiveType()) { return true; }

        if (Peek()->Type == TokenType::Identifier) {
            if (m_DeclaredTypes.contains(fmt::format("{}", Peek()->Data))) {
                return true;
            }

            return false;
        }

        return false;
    }

    BinaryOperatorType Parser::ParseOperator() {
        Token op = *Peek();

        switch (op.Type) {
            case TokenType::Plus: return BinaryOperatorType::Add;
            case TokenType::PlusEq: return BinaryOperatorType::AddInPlace;
            case TokenType::Minus: return BinaryOperatorType::Sub;
            case TokenType::MinusEq: return BinaryOperatorType::SubInPlace;
            case TokenType::Star: return BinaryOperatorType::Mul;
            case TokenType::StarEq: return BinaryOperatorType::MulInPlace;
            case TokenType::Slash: return BinaryOperatorType::Div;
            case TokenType::SlashEq: return BinaryOperatorType::DivInPlace;
            case TokenType::Percent: return BinaryOperatorType::Mod;
            case TokenType::PercentEq: return BinaryOperatorType::ModInPlace;
            case TokenType::Ampersand: return BinaryOperatorType::And;
            case TokenType::AmpersandEq: return BinaryOperatorType::AndInPlace;
            case TokenType::DoubleAmpersand: return BinaryOperatorType::BitAnd;
            case TokenType::Pipe: return BinaryOperatorType::Or;
            case TokenType::PipeEq: return BinaryOperatorType::OrInPlace;
            case TokenType::DoublePipe: return BinaryOperatorType::BitOr;
            case TokenType::UpArrow: return BinaryOperatorType::Xor;
            case TokenType::UpArrowEq: return BinaryOperatorType::XorInPlace;
            case TokenType::Less: return BinaryOperatorType::Less;
            case TokenType::LessOrEq: return BinaryOperatorType::LessOrEq;
            case TokenType::Greater: return BinaryOperatorType::Greater;
            case TokenType::GreaterOrEq: return BinaryOperatorType::GreaterOrEq;
            case TokenType::Eq: return BinaryOperatorType::Eq;
            case TokenType::IsEq: return BinaryOperatorType::IsEq;
            case TokenType::IsNotEq: return BinaryOperatorType::IsNotEq;
            default: return BinaryOperatorType::Invalid;
        }
    }

    size_t Parser::GetBinaryPrecedence(BinaryOperatorType type) {
        switch (type) {
            case BinaryOperatorType::Eq:
            case BinaryOperatorType::AddInPlace:
            case BinaryOperatorType::SubInPlace:
            case BinaryOperatorType::MulInPlace:
            case BinaryOperatorType::ModInPlace:
            case BinaryOperatorType::DivInPlace:
            case BinaryOperatorType::AndInPlace:
            case BinaryOperatorType::OrInPlace:
            case BinaryOperatorType::XorInPlace:
                return 10;

            case BinaryOperatorType::Less:
            case BinaryOperatorType::LessOrEq:
            case BinaryOperatorType::Greater:
            case BinaryOperatorType::GreaterOrEq:
            case BinaryOperatorType::IsEq:
            case BinaryOperatorType::IsNotEq:
            case BinaryOperatorType::BitAnd:
            case BinaryOperatorType::BitOr:
                return 20;

            case BinaryOperatorType::And:
            case BinaryOperatorType::Or:
            case BinaryOperatorType::Xor:
                return 30;

            case BinaryOperatorType::Add:
            case BinaryOperatorType::Sub:
                return 40;

            case BinaryOperatorType::Mod:
            case BinaryOperatorType::Mul:
            case BinaryOperatorType::Div:
                return 50;
        }

        BLUA_ASSERT(false, "Unreachable");
        return -1;
    }

    Expr* Parser::ParseValue() {
        if (!Peek()) { return nullptr; }
        Token& value = *Peek();
    
        Expr* final = nullptr;

        switch (value.Type) {
            case TokenType::False: {
                Consume();
    
                final = m_Context->Allocate<BooleanConstantExpr>(m_Context, false);
                break;
            }
    
            case TokenType::True: {
                Consume();
    
                final = m_Context->Allocate<BooleanConstantExpr>(m_Context, false);
                break;
            }
    
            case TokenType::CharLit: {
                Consume();
    
                int8_t ch = static_cast<int8_t>(value.Data.Data()[0]);
                final = m_Context->Allocate<CharacterConstantExpr>(m_Context, ch);
                break;
            }
    
            case TokenType::IntLit: {
                Consume();
    
                int32_t num = 0;
                auto [ptr, ec] = std::from_chars(value.Data.Data(), value.Data.Data() + value.Data.Size(), num);
    
                if (ec != std::errc()) {
                    ErrorTooLarge(value.Data);
                }
                final = m_Context->Allocate<IntegerConstantExpr>(m_Context, num, TypeInfo::Create(m_Context, PrimitiveType::Int, true));
                break;
            }
    
            case TokenType::FloatLit: {
                Consume();
    
                float num = 0.0f;
                auto [ptr, ec] = std::from_chars(value.Data.Data(), value.Data.Data() + value.Data.Size(), num);
    
                if (ec != std::errc()) {
                    ErrorTooLarge(value.Data);
                }
                final = m_Context->Allocate<FloatingConstantExpr>(m_Context, num, TypeInfo::Create(m_Context, PrimitiveType::Float));
                break;
            }
    
            case TokenType::StrLit: {
                Consume();
    
                StringView str = value.Data;
                final = m_Context->Allocate<StringConstantExpr>(m_Context, str);
                break;
            }
    
            case TokenType::Minus: {
                Token m = Consume();
    
                Expr* expr = ParseValue();
                final = m_Context->Allocate<UnaryOperatorExpr>(m_Context, expr, UnaryOperatorType::Negate);
                break;
            }
    
            case TokenType::LeftParen: {
                Token p = Consume();
    
                if (IsVariableType()) {
                    StringBuilder type = ParseVariableType();

                    TryConsume(TokenType::RightParen, "')'");
                    Expr* expr = ParseValue();

                    final = m_Context->Allocate<CastExpr>(m_Context, expr, StringView(type.Data(), type.Size()));
                } else {
                    Expr* expr = ParseExpression();
                    TryConsume(TokenType::RightParen, "')'");
                    
                    final = m_Context->Allocate<ParenExpr>(m_Context, expr);
                }
                
                break;
            }

            case TokenType::Self: {
                Token s = Consume();

                final = m_Context->Allocate<SelfExpr>(m_Context);
                break;
            }
    
            case TokenType::Identifier: {
                Token i = Consume();

                // Check if this is a function call
                if (Match(TokenType::LeftParen)) {
                    Consume();
    
                    TinyVector<Expr*> args;

                    while (!Match(TokenType::RightParen)) {
                        Expr* val = ParseExpression();
    
                        if (Match(TokenType::Comma)) {
                            Consume();
                        }
    
                        args.Append(m_Context, val);
                    }
    
                    TryConsume(TokenType::RightParen, "')'");
    
                    final = m_Context->Allocate<CallExpr>(m_Context, i.Data, args);
                } else {
                    final = m_Context->Allocate<VarRefExpr>(m_Context, i.Data);
                }

                break;
            }
        }

        // Handle member access (foo.bar) and array access (foo[5])
        // NOTE: This is currently not avalible, AST refactoring in progress
        // while (Match(TokenType::Dot) || Match(TokenType::LeftBracket)) {
        //     Token op = Consume();

        //     if (op.Type == TokenType::Dot) {
        //         Token* member = TryConsume(TokenType::Identifier, "identifier");
        //         if (!member) { return nullptr; }

        //         if (Match(TokenType::LeftParen)) {
        //             Consume();
    
        //             ExprMethodCall* methodExpr = Allocate<ExprMethodCall>();
        //             methodExpr->Member = member->Data;
        //             methodExpr->Parent = final;
    
        //             while (!Match(TokenType::RightParen)) {
        //                 NodeExpr* val = ParseExpression();
    
        //                 if (Match(TokenType::Comma)) {
        //                     Consume();
        //                 }
    
        //                 methodExpr->Arguments.Append(m_Context, Allocate<Node>(val));
        //             }
    
        //             TryConsume(TokenType::RightParen, "')'");
    
        //             final = Allocate<NodeExpr>(methodExpr, SourceRange(final->Range.Start, Peek(-1)->Loc.End), op.Loc.Start);
        //         } else {
        //             ExprMember* memExpr = Allocate<ExprMember>();

        //             memExpr->Member = member->Data;
        //             memExpr->Parent = final;
        //             final = Allocate<NodeExpr>(memExpr, SourceRange(final->Range.Start, Peek(-1)->Loc.End), op.Loc.Start);
        //         }
        //     } else if (op.Type == TokenType::LeftBracket) {
        //         ExprArrayAccess* arrExpr = Allocate<ExprArrayAccess>();

        //         arrExpr->Index = ParseExpression();
        //         arrExpr->Parent = final;
        //         TryConsume(TokenType::RightBracket, "']'");

        //         final = Allocate<NodeExpr>(arrExpr, SourceRange(final->Range.Start, Peek(-1)->Loc.End), op.Loc.Start);
        //     }
        // }

        return final;
    }

    Expr* Parser::ParseExpression(size_t minbp) {
        Expr* lhsExpr = ParseValue();

        // There are two conditions to this loop
        // A valid operator (the first half of the check) and a valid precedence (the second half)
        while ((Peek() && ParseOperator() != BinaryOperatorType::Invalid) && 
               (GetBinaryPrecedence(ParseOperator()) >= minbp)) {
            BinaryOperatorType op = ParseOperator();
            Token o = Consume();

            Expr* rhsExpr = ParseExpression(GetBinaryPrecedence(op) + 1);
            lhsExpr = m_Context->Allocate<BinaryOperatorExpr>(m_Context, lhsExpr, rhsExpr, op);
        }

        return lhsExpr;
    }

    Stmt* Parser::ParseCompound() {
        TinyVector<Stmt*> stmts;
        Token* l = TryConsume(TokenType::LeftCurly, "'{'");

        while (!Match(TokenType::RightCurly)) {
            Stmt* stmt = ParseToken();
            stmts.Append(m_Context, stmt);
        }

        TryConsume(TokenType::RightCurly, "'}'");

        return m_Context->Allocate<CompoundStmt>(m_Context, stmts);
    }

    Stmt* Parser::ParseCompoundInline() {
        if (Match(TokenType::LeftCurly)) {
            return ParseCompound();
        } else {
            return ParseToken();
        }
    }

    Stmt* Parser::ParseType(bool external) {
        StringBuilder type = ParseVariableType();

        if (Peek(1)) {
            if (Peek(1)->Type == TokenType::LeftParen) {
                return ParseFunctionDecl(type, Peek(-1)->Loc, external);
            }
        }

        return ParseVariableDecl(type, Peek(-1)->Loc);
    }

    Stmt* Parser::ParseVariableDecl(StringBuilder type, SourceRange start) {
        Token* ident = TryConsume(TokenType::Identifier, "identifier");
        Expr* value = nullptr;

        if (ident) {
            if (Match(TokenType::Eq)) {
                Consume();
                value = ParseExpression();
            }

            return m_Context->Allocate<VarDecl>(m_Context, ident->Data, StringView(type.Data(), type.Size()), value);
        } else {
            return nullptr;
        }
    }

    Stmt* Parser::ParseFunctionDecl(StringBuilder returnType, SourceRange start, bool external) {
        Token* ident = TryConsume(TokenType::Identifier, "identifier");

        if (ident) {
            TinyVector<ParamDecl*> params;

            if (Match(TokenType::LeftParen)) {
                Consume();
                params = ParseFunctionParameters();
                TryConsume(TokenType::RightParen, "')'");

                Stmt* body = nullptr;

                if (Match(TokenType::LeftCurly)) {
                    body = ParseCompound();
                    m_NeedsSemi = false;
                }

                return m_Context->Allocate<FunctionDecl>(m_Context, ident->Data, StringView(returnType.Data(), returnType.Size()), params, external, GetNode<CompoundStmt>(body));
            } else {
                ErrorExpected("'('");
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
        BLUA_ASSERT(false, "todo: add struct parsing");
        // Token s = Consume(); // Consume "struct"

        // Token* ident = TryConsume(TokenType::Identifier, "indentifier");

        // if (!ident) { return nullptr; }
        // m_DeclaredTypes[fmt::format("{}", ident->Data)] = true;

        // StmtStructDecl* node = Allocate<StmtStructDecl>();
        // node->Identifier = ident->Data;

        // TryConsume(TokenType::LeftCurly, "'{'");

        // while (!Match(TokenType::RightCurly)) {
        //     if (IsVariableType()) {
        //         Node* field = nullptr;
        //         StringBuilder type = ParseVariableType();

        //         Token* fieldName = TryConsume(TokenType::Identifier, "identifier");
        //         if (!fieldName) { return nullptr; }

        //         if (Match(TokenType::LeftParen)) {
        //             Consume();

        //             StmtMethodDecl* decl = Allocate<StmtMethodDecl>();
        //             decl->Name = fieldName->Data;
        //             decl->ReturnType = type;

        //             NodeList params = ParseFunctionParameters();
        //             TryConsume(TokenType::RightParen, "')'");

        //             decl->Parameters = params;
        //             decl->Body = ParseCompound();

        //             node->Fields.Append(m_Context, Allocate<Node>(Allocate<NodeStmt>(decl, fieldName->Loc)));
        //         } else {
        //             StmtFieldDecl* decl = Allocate<StmtFieldDecl>();
        //             decl->Identifier = fieldName->Data;
        //             decl->Type = type;

        //             TryConsume(TokenType::Semi, "';'");

        //             node->Fields.Append(m_Context, Allocate<Node>(Allocate<NodeStmt>(decl, fieldName->Loc)));
        //         }
        //     }
        // }

        // TryConsume(TokenType::RightCurly, "'}'");

        // m_NeedsSemi = false;
        // return Allocate<NodeStmt>(node, SourceRange(s.Loc.Start, Peek(-1)->Loc.End), ident->Loc.Start);
    }

    Stmt* Parser::ParseWhile() {
        Token w = Consume(); // Consume "while"

        TryConsume(TokenType::LeftParen, "'('");
        Expr* condition = ParseExpression();
        TryConsume(TokenType::RightParen, "')'");
        Stmt* body = ParseCompoundInline();

        m_NeedsSemi = false;

        return m_Context->Allocate<WhileStmt>(m_Context, condition, body);
    }

    Stmt* Parser::ParseDoWhile() {
        BLUA_ASSERT(false, "todo: add do while parsing");
        // Token d = Consume(); // Consume "do"

        // StmtDoWhile* node = Allocate<StmtDoWhile>();
        // node->Body = ParseCompoundInline();

        // TryConsume(TokenType::While, "while");
        // TryConsume(TokenType::LeftParen, "'('");
        // node->Condition = ParseExpression();
        // TryConsume(TokenType::RightParen, "')'");

        // m_NeedsSemi = false;

        // return Allocate<NodeStmt>(node, d.Loc);
    }

    Stmt* Parser::ParseFor() {
        BLUA_ASSERT(false, "todo: add for parsing");
        // Token f = Consume(); // Consume "for"

        // StmtFor* node = Allocate<StmtFor>();
        // TryConsume(TokenType::LeftParen, "'('");

        // node->Prologue = ParseStatement();

        // node->Condition = ParseExpression();
        // TryConsume(TokenType::Semi, "';'");

        // node->Epilogue = ParseExpression();
        // TryConsume(TokenType::RightParen, "')'");

        // SourceLocation end = Peek(-1)->Loc.End;
        // node->Body = ParseCompoundInline();

        // m_NeedsSemi = false;

        // return Allocate<NodeStmt>(node, SourceRange(f.Loc.Start, end), f.Loc.Start);
    }

    Stmt* Parser::ParseIf() {
        BLUA_ASSERT(false, "todo: add if parsing");
        // Token i = Consume(); // Consume "if"

        // StmtIf* node = Allocate<StmtIf>();
        // TryConsume(TokenType::LeftParen, "'('");

        // node->Condition = ParseExpression();

        // TryConsume(TokenType::RightParen, "')'");
        // SourceLocation end = Peek(-1)->Loc.End;
        // node->Body = ParseCompoundInline();

        // if (Match(TokenType::Else)) {
        //     Consume();

        //     node->ElseBody = ParseCompoundInline();
        // }

        // m_NeedsSemi = false;

        // return Allocate<NodeStmt>(node, SourceRange(i.Loc.Start, end), i.Loc.Start);
    }

    Stmt* Parser::ParseBreak() {
        BLUA_ASSERT(false, "todo: add break parsing");
        // Token b = Consume(); // Consume "break"

        // return Allocate<NodeStmt>(nullptr, b.Loc);
    }

    Stmt* Parser::ParseContinue() {
        BLUA_ASSERT(false, "todo: add continue parsing");
        // Token c = Consume(); // Consume "continue"

        // return Allocate<NodeStmt>(nullptr, c.Loc);
    }

    Stmt* Parser::ParseReturn() {
        BLUA_ASSERT(false, "todo: add return parsing");
        // Token r = Consume(); // Consume "return"

        // StmtReturn* node = Allocate<StmtReturn>();
        // node->Value = ParseExpression();

        // return Allocate<NodeStmt>(node, SourceRange(r.Loc.Start, Peek(-1)->Loc.End), r.Loc.Start);
    }

    Stmt* Parser::ParseStatement() {
        TokenType t = Peek()->Type;

        Stmt* node = nullptr;

        if (IsVariableType()) {
            node = ParseType();
        } else if (t == TokenType::Extern) {
            node = ParseExtern();
        } else if (t == TokenType::Struct) {
            node = ParseStructDecl();
        } else if (t == TokenType::LeftCurly) {
            node = ParseCompound();
        } else if (t == TokenType::While) {
            node = ParseWhile();
        } else if (t == TokenType::Do) {
            node = ParseDoWhile();
        } else if (t == TokenType::For) {
            node = ParseFor();
        } else if (t == TokenType::If) {
            node = ParseIf();
        } else if (t == TokenType::Break) {
            node = ParseBreak();
        } else if (t == TokenType::Return) {
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
            TryConsume(TokenType::Semi, "';'");
        }

        m_NeedsSemi = true;

        return s;
    }

    void Parser::ErrorExpected(const StringView msg) {
        // if (Peek(-1)) {
        //     m_Context->ReportCompilerError(Peek(-1)->Line, Peek(-1)->Column, fmt::format("Expected {} after token \"{}\"", msg, TokenTypeToString(Peek(-1)->Type)));
        // } else {
        //     m_Context->ReportCompilerError(Peek()->Line, Peek()->Column, fmt::format("Expected {} after token \"{}\"", msg, TokenTypeToString(Peek()->Type)));
        // }
    }

    void Parser::ErrorTooLarge(const StringView value) {
        // m_Context->ReportCompilerError(Peek(-1)->Line, Peek(-1)->Column, fmt::format("Constant {} is too large", value));
    }

} // namespace BlackLua::Internal
