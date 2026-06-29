#pragma once

#include "ariac/core/vector.hpp"
#include "ariac/types/type_info.hpp"
#include "ariac/core/source_location.hpp"

#include <string_view>

namespace ariac {

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

    struct ImportStmt {
        ImportStmt(std::string_view name, std::string_view alias)
            : name(name), alias(alias) {}

        ImportStmt(std::string_view name, Module* mod)
            : name(name), resolved_module(mod) {}

        std::string_view name;
        std::string_view alias;
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

        Decl* prologue = nullptr; // let i = 0;
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

    struct DeferStmt {
        DeferStmt(Stmt* stmt)
            : statement(stmt) {}

        Stmt* statement = nullptr;
    };

    struct Stmt {
        template <typename T>
        static inline Stmt* Create(CompilationContext* ctx, SourceLoc loc, StmtKind kind, T t) { return ctx->allocate<Stmt>(kind, loc, t); }
        static Stmt* dup(CompilationContext* ctx, Stmt* s);

        StmtKind kind = StmtKind::Invalid;

        SourceLoc loc;

        bool reached = true; // A flag to see if this statement can ever be reached in a function body

        union {
            ErrorStmt error;
            ImportStmt import;
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

        Stmt(StmtKind kind, SourceLoc loc, ImportStmt import)
            : kind(kind), loc(loc), import(import) {}

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
