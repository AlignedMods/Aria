#pragma once

#include "aria/internal/compiler/core/vector.hpp"
#include "aria/internal/compiler/types/type_info.hpp"
#include "aria/internal/compiler/core/source_location.hpp"

#include <string_view>

namespace Aria::Internal {

    enum class StmtKind {
        Invalid = 0,

        Error,
        Nop,
        Import,
        Block,
        While,
        DoWhile,
        For,
        If,
        Break,
        Continue,
        Return,
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

    struct ImportStmt {
        ImportStmt(std::string_view name)
            : name(name) {}

        std::string_view name;
        Module* resolved_module = nullptr;
    };

    struct BlockStmt {
        BlockStmt() = default;
        BlockStmt(const TinyVector<Stmt*>& stmts, bool unsafe)
            : stmts(stmts), unsafe(unsafe) {}

        TinyVector<Stmt*> stmts;
        bool unsafe = false;
    };

    struct WhileStmt {
        WhileStmt(Expr* condition, Stmt* body)
            : condition(condition), body(body) {}

        Expr* condition = nullptr;
        Stmt* body = nullptr;
    };
    
    struct DoWhileStmt {
        DoWhileStmt(Expr* condition, Stmt* body)
            : condition(condition), body(body) {}

        Expr* condition = nullptr;
        Stmt* body = nullptr;
    };
    
    struct ForStmt {
        ForStmt(Decl* prologue, Expr* condition, Expr* step, Stmt* body)
            : prologue(prologue), condition(condition), step(step), body(body) {}

        Decl* prologue = nullptr; // int i = 0;
        Expr* condition = nullptr; // i < 5;
        Expr* step = nullptr; // i += 1;
        Stmt* body = nullptr; // { ... }
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

    struct Stmt {
        template <typename T>
        static inline Stmt* Create(CompilationContext* ctx, SourceLocation loc, SourceRange range, StmtKind kind, T t) { return ctx->allocate<Stmt>(kind, loc, range, t); }

        StmtKind kind = StmtKind::Invalid;

        SourceLocation loc;
        SourceRange range;

        union {
            ErrorStmt error;
            ImportStmt import;
            BlockStmt block;
            WhileStmt while_;
            DoWhileStmt do_while;
            ForStmt for_;
            IfStmt if_;
            ReturnStmt return_;
            Expr* expr;
            Decl* decl;
        };

        Stmt(StmtKind kind, SourceLocation loc, SourceRange range, ErrorStmt error)
            : kind(kind), loc(loc), range(range), error(error) {}

        Stmt(StmtKind kind, SourceLocation loc, SourceRange range, ImportStmt import)
            : kind(kind), loc(loc), range(range), import(import) {}

        Stmt(StmtKind kind, SourceLocation loc, SourceRange range, BlockStmt block)
            : kind(kind), loc(loc), range(range), block(block) {}

        Stmt(StmtKind kind, SourceLocation loc, SourceRange range, WhileStmt wh)
            : kind(kind), loc(loc), range(range), while_(wh) {}

        Stmt(StmtKind kind, SourceLocation loc, SourceRange range, DoWhileStmt dowh)
            : kind(kind), loc(loc), range(range), do_while(dowh) {}

        Stmt(StmtKind kind, SourceLocation loc, SourceRange range, ForStmt f)
            : kind(kind), loc(loc), range(range), for_(f) {}

        Stmt(StmtKind kind, SourceLocation loc, SourceRange range, IfStmt i)
            : kind(kind), loc(loc), range(range), if_(i) {}

        Stmt(StmtKind kind, SourceLocation loc, SourceRange range, ReturnStmt ret)
            : kind(kind), loc(loc), range(range), return_(ret) {}

        Stmt(StmtKind kind, SourceLocation loc, SourceRange range, Expr* expr)
            : kind(kind), loc(loc), range(range), expr(expr) {}

        Stmt(StmtKind kind, SourceLocation loc, SourceRange range, Decl* decl)
            : kind(kind), loc(loc), range(range), decl(decl) {}
    };

    inline Stmt error_stmt = Stmt(StmtKind::Error, SourceLocation(), SourceRange(), ErrorStmt());

} // namespace Aria::Internal
