#pragma once

#include "aria/internal/compiler/core/vector.hpp"
#include "aria/internal/compiler/core/string_view.hpp"
#include "aria/internal/compiler/core/string_builder.hpp"
#include "aria/internal/compiler/types/type_info.hpp"
#include "aria/internal/compiler/core/source_location.hpp"

namespace Aria::Internal {

    enum class StmtKind {
        Invalid = 0,

        Error,
        Nop,
        Block,
        While,
        DoWhile,
        For,
        If,
        Return,
        Expr,
        Decl
    };

    struct Expr;
    struct Decl;
    struct Stmt;

    struct ErrorStmt {
        ErrorStmt() = default;
    };

    struct NopStmt {
        NopStmt() = default;
    };

    struct BlockStmt {
        BlockStmt() = default;
        BlockStmt(const TinyVector<Stmt*>& stmts)
            : Stmts(stmts) {}

        TinyVector<Stmt*> Stmts;
    };

    struct WhileStmt {
        WhileStmt(Expr* condition, Stmt* body)
            : Condition(condition), Body(body) {}

        Expr* Condition = nullptr;
        Stmt* Body = nullptr;
    };
    
    struct DoWhileStmt {
        DoWhileStmt(Expr* condition, Stmt* body)
            : Condition(condition), Body(body) {}

        Expr* Condition = nullptr;
        Stmt* Body = nullptr;
    };
    
    struct ForStmt {
        ForStmt(Decl* prologue, Expr* condition, Expr* step, Stmt* body)
            : Prologue(prologue), Condition(condition), Step(step), Body(body) {}

        Decl* Prologue = nullptr; // int i = 0;
        Expr* Condition = nullptr; // i < 5;
        Expr* Step = nullptr; // i += 1;
        Stmt* Body = nullptr; // { ... }
    };
    
    struct IfStmt {
        IfStmt(Expr* condition, Stmt* body, Stmt* elseBody)
            : Condition(condition), Body(body), ElseBody(elseBody) {}

        Expr* Condition = nullptr;
        Stmt* Body = nullptr;
        Stmt* ElseBody = nullptr;
    };
    
    struct ReturnStmt {
        ReturnStmt(Expr* value)
            : Value(value) {}

        Expr* Value = nullptr;
    };

    struct Stmt {
        template <typename T>
        static inline Stmt* Create(CompilationContext* ctx, SourceLocation loc, SourceRange range, StmtKind kind, T t) { return ctx->Allocate<Stmt>(kind, loc, range, t); }

        StmtKind Kind = StmtKind::Invalid;

        SourceLocation Loc;
        SourceRange Range;

        union {
            ErrorStmt Error;
            NopStmt Nop;
            BlockStmt Block;
            WhileStmt While;
            DoWhileStmt DoWhile;
            ForStmt For;
            IfStmt If;
            ReturnStmt Return;
            Expr* ExprStmt;
            Decl* DeclStmt;
        };

        Stmt(StmtKind kind, SourceLocation loc, SourceRange range, ErrorStmt error)
            : Kind(kind), Loc(loc), Range(range), Error(error) {}

        Stmt(StmtKind kind, SourceLocation loc, SourceRange range, NopStmt nop)
            : Kind(kind), Loc(loc), Range(range), Nop(nop) {}

        Stmt(StmtKind kind, SourceLocation loc, SourceRange range, BlockStmt block)
            : Kind(kind), Loc(loc), Range(range), Block(block) {}

        Stmt(StmtKind kind, SourceLocation loc, SourceRange range, WhileStmt wh)
            : Kind(kind), Loc(loc), Range(range), While(wh) {}

        Stmt(StmtKind kind, SourceLocation loc, SourceRange range, DoWhileStmt dowh)
            : Kind(kind), Loc(loc), Range(range), DoWhile(dowh) {}

        Stmt(StmtKind kind, SourceLocation loc, SourceRange range, ForStmt f)
            : Kind(kind), Loc(loc), Range(range), For(f) {}

        Stmt(StmtKind kind, SourceLocation loc, SourceRange range, IfStmt i)
            : Kind(kind), Loc(loc), Range(range), If(i) {}

        Stmt(StmtKind kind, SourceLocation loc, SourceRange range, ReturnStmt ret)
            : Kind(kind), Loc(loc), Range(range), Return(ret) {}

        Stmt(StmtKind kind, SourceLocation loc, SourceRange range, Expr* expr)
            : Kind(kind), Loc(loc), Range(range), ExprStmt(expr) {}

        Stmt(StmtKind kind, SourceLocation loc, SourceRange range, Decl* decl)
            : Kind(kind), Loc(loc), Range(range), DeclStmt(decl) {}
    };

} // namespace Aria::Internal
