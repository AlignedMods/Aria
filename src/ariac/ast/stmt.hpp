#pragma once

#include "ariac/core/vector.hpp"
#include "ariac/types/type_info.hpp"
#include "ariac/core/source_location.hpp"
#include "ariac/compilation_context.hpp"

#include <string_view>

namespace ariac {

    enum class StmtKind {
        Invalid = 0,

        Error,
        Nop,
        Block,
        While,
        DoWhile,
        For,
        If,
        Break,
        Continue,
        Return,
        Defer,
        Expr,
        Decl
    };

    struct Expr;
    struct Decl;
    struct Stmt;

    struct Module;

    struct ErrorStmt {
        ErrorStmt() = default;
    };

    struct BlockStmt {
        BlockStmt() = default;
        BlockStmt(TinyVector<Stmt*> stmts)
            : stmts(stmts) {}

        TinyVector<Stmt*> stmts;
        bool reaches_end = true;
    };

    struct WhileStmt {
        WhileStmt(Expr* condition, Stmt* body)
            : condition(condition), body(body) {}

        Expr* condition = nullptr;
        Stmt* body = nullptr;
        bool infinite = false;
    };
    
    struct DoWhileStmt {
        DoWhileStmt(Expr* condition, Stmt* body)
            : condition(condition), body(body) {}

        Expr* condition = nullptr;
        Stmt* body = nullptr;
        bool infinite = false;
    };
    
    struct ForStmt {
        ForStmt(Decl* prologue, Expr* condition, Expr* step, Stmt* body)
            : prologue(prologue), condition(condition), step(step), body(body) {}

        Decl* prologue = nullptr; // let i = 0;
        Expr* condition = nullptr; // i < 5;
        Expr* step = nullptr; // i += 1;
        Stmt* body = nullptr; // { ... }
        bool infinite = false;
    };
    
    struct IfStmt {
        IfStmt(Expr* condition, Stmt* body, Stmt* else_body)
            : condition(condition), body(body), else_body(else_body) {}

        Expr* condition = nullptr;
        Stmt* body = nullptr;
        Stmt* else_body = nullptr;
    };
    
    struct ReturnStmt {
        ReturnStmt(Expr* value)
            : value(value) {}

        Expr* value = nullptr;
    };

    struct DeferStmt {
        DeferStmt(Stmt* stmt)
            : statement(stmt) {}

        Stmt* statement = nullptr;
    };

    struct Stmt {
        template <typename T>
        static inline Stmt* Create(SourceLoc loc, StmtKind kind, T t) { return context.allocate<Stmt>(kind, loc, t); }
        static Stmt* dup(Stmt* s);

        StmtKind kind = StmtKind::Invalid;

        SourceLoc loc;

        bool reached = true; // A flag to see if this statement can ever be reached in a function body

        union {
            ErrorStmt error;
            BlockStmt block;
            WhileStmt while_;
            DoWhileStmt do_while;
            ForStmt for_;
            IfStmt if_;
            ReturnStmt return_;
            DeferStmt defer;
            Expr* expr;
            Decl* decl;
        };

        Stmt(StmtKind kind, SourceLoc loc, ErrorStmt error)
            : kind(kind), loc(loc), error(error) {}

        Stmt(StmtKind kind, SourceLoc loc, BlockStmt block)
            : kind(kind), loc(loc), block(block) {}

        Stmt(StmtKind kind, SourceLoc loc, WhileStmt wh)
            : kind(kind), loc(loc), while_(wh) {}

        Stmt(StmtKind kind, SourceLoc loc, DoWhileStmt dowh)
            : kind(kind), loc(loc), do_while(dowh) {}

        Stmt(StmtKind kind, SourceLoc loc, ForStmt f)
            : kind(kind), loc(loc), for_(f) {}

        Stmt(StmtKind kind, SourceLoc loc, IfStmt i)
            : kind(kind), loc(loc), if_(i) {}

        Stmt(StmtKind kind, SourceLoc loc, ReturnStmt ret)
            : kind(kind), loc(loc), return_(ret) {}

        Stmt(StmtKind kind, SourceLoc loc, DeferStmt d)
            : kind(kind), loc(loc), defer(d) {}

        Stmt(StmtKind kind, SourceLoc loc, Expr* expr)
            : kind(kind), loc(loc), expr(expr) {}

        Stmt(StmtKind kind, SourceLoc loc, Decl* decl)
            : kind(kind), loc(loc), decl(decl) {}
    };

    inline Stmt error_stmt = Stmt(StmtKind::Error, SourceLoc(), ErrorStmt());

} // namespace ariac
