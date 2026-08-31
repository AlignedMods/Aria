#pragma once

#include "ariac/core/vector.hpp"
#include "ariac/types/type_info.hpp"
#include "ariac/core/source_location.hpp"
#include "ariac/compilation_context.hpp"
#include "ariac/enums.hpp"

#include <string_view>

namespace ariac {

    struct Expr;
    struct Decl;
    struct Stmt;

    struct Module;

    struct ErrorStmt {
        ErrorStmt() = default;
    };

    struct CompoundStmt {
        CompoundStmt() = default;
        CompoundStmt(TinyVector<Stmt*> stmts)
            : stmts(stmts) {}

        TinyVector<Stmt*> stmts;
        Stmt* cleanup = nullptr;
    };

    struct WhileStmt {
        WhileStmt(Expr* condition, Stmt* body)
            : condition(condition), body(body) {}

        Expr* condition = nullptr;
        Stmt* body = nullptr;
        bool infinite = false;

        struct {
            void* continue_block = nullptr;
            void* end_block = nullptr;
        } backend;
    };
    
    struct DoWhileStmt {
        DoWhileStmt(Expr* condition, Stmt* body)
            : condition(condition), body(body) {}

        Expr* condition = nullptr;
        Stmt* body = nullptr;
        bool infinite = false;

        struct {
            void* continue_block = nullptr;
            void* end_block = nullptr;
        } backend;
    };
    
    struct ForStmt {
        ForStmt(Decl* prologue, Expr* condition, Expr* step, Stmt* body)
            : prologue(prologue), condition(condition), step(step), body(body) {}

        Decl* prologue = nullptr; // let i = 0;
        Expr* condition = nullptr; // i < 5;
        Expr* step = nullptr; // i += 1;
        Stmt* body = nullptr; // { ... }
        bool infinite = false;

        struct {
            void* continue_block = nullptr;
            void* end_block = nullptr;
        } backend;
    };
    
    struct IfStmt {
        IfStmt(Expr* condition, Stmt* body, Stmt* else_body)
            : condition(condition), body(body), else_body(else_body) {}

        Expr* condition = nullptr;
        Stmt* body = nullptr;
        Stmt* else_body = nullptr;
    };

    struct SwitchStmt {
        SwitchStmt(Expr* e, TinyVector<Stmt*> cases)
            : expression(e), cases(cases) {}

        Expr* expression;
        TinyVector<Stmt*> cases;

        struct {
            void* end_block = nullptr;
        } backend;
    };

    struct CaseStmt {
        CaseStmt(Expr* cond, Stmt* body)
            : condition(cond), body(body) {}

        Expr* condition;
        Stmt* body;

        struct {
            void* entry_block = nullptr;
            void* body_block = nullptr;
            void* fail_block = nullptr;
        } backend;
    };

    struct BreakStmt {
        Stmt* target = nullptr;
        Stmt* cleanup = nullptr;
    };

    struct ContinueStmt {
        Stmt* target = nullptr;
        Stmt* cleanup = nullptr;
    };
    
    struct NextcaseStmt {
        Stmt* target = nullptr;
        Stmt* cleanup = nullptr;
    };

    struct ReturnStmt {
        ReturnStmt(Expr* value)
            : value(value) {}

        Expr* value = nullptr;
        Stmt* cleanup = nullptr;
    };

    struct DeferStmt {
        DeferStmt(Stmt* stmt)
            : statement(stmt) {}

        Stmt* statement = nullptr;
    };

    struct AssertStmt {
        AssertStmt(Expr* cond, TinyVector<Expr*> args)
            : condition(cond), arguments(args) {}

        Expr* condition;
        TinyVector<Expr*> arguments;
    };

    struct UnreachableStmt {
        UnreachableStmt(TinyVector<Expr*> args)
            : arguments(args) {}

        TinyVector<Expr*> arguments;
    };

    struct Stmt {
        template <typename T>
        static inline Stmt* Create(SourceLoc loc, StmtKind kind, T t) { return context.allocate<Stmt>(kind, loc, t); }
        static Stmt* dup(Stmt* s);

        StmtKind kind = StmtKind::Invalid;

        SourceLoc loc;

        Stmt* next = nullptr;
        bool reached = true;

        union {
            ErrorStmt error;
            CompoundStmt compound;
            WhileStmt while_;
            DoWhileStmt do_while;
            ForStmt for_;
            IfStmt if_;
            SwitchStmt switch_;
            CaseStmt case_;
            BreakStmt break_;
            ContinueStmt continue_;
            NextcaseStmt nextcase;
            ReturnStmt return_;
            DeferStmt defer;
            AssertStmt assert_;
            UnreachableStmt unreachable;
            Expr* expr;
            Decl* decl;
        };

        Stmt(StmtKind kind, SourceLoc loc, ErrorStmt error)
            : kind(kind), loc(loc), error(error) {}

        Stmt(StmtKind kind, SourceLoc loc, CompoundStmt compound)
            : kind(kind), loc(loc), compound(compound) {}

        Stmt(StmtKind kind, SourceLoc loc, WhileStmt wh)
            : kind(kind), loc(loc), while_(wh) {}

        Stmt(StmtKind kind, SourceLoc loc, DoWhileStmt dowh)
            : kind(kind), loc(loc), do_while(dowh) {}

        Stmt(StmtKind kind, SourceLoc loc, ForStmt f)
            : kind(kind), loc(loc), for_(f) {}

        Stmt(StmtKind kind, SourceLoc loc, IfStmt i)
            : kind(kind), loc(loc), if_(i) {}

        Stmt(StmtKind kind, SourceLoc loc, SwitchStmt s)
            : kind(kind), loc(loc), switch_(s) {}

        Stmt(StmtKind kind, SourceLoc loc, CaseStmt c)
            : kind(kind), loc(loc), case_(c) {}

        Stmt(StmtKind kind, SourceLoc loc, BreakStmt b)
            : kind(kind), loc(loc), break_(b) {}

        Stmt(StmtKind kind, SourceLoc loc, ContinueStmt c)
            : kind(kind), loc(loc), continue_(c) {}

        Stmt(StmtKind kind, SourceLoc loc, NextcaseStmt n)
            : kind(kind), loc(loc), nextcase(n) {}

        Stmt(StmtKind kind, SourceLoc loc, ReturnStmt ret)
            : kind(kind), loc(loc), return_(ret) {}

        Stmt(StmtKind kind, SourceLoc loc, DeferStmt d)
            : kind(kind), loc(loc), defer(d) {}

        Stmt(StmtKind kind, SourceLoc loc, AssertStmt a)
            : kind(kind), loc(loc), assert_(a) {}

        Stmt(StmtKind kind, SourceLoc loc, UnreachableStmt u)
            : kind(kind), loc(loc), unreachable(u) {}

        Stmt(StmtKind kind, SourceLoc loc, Expr* expr)
            : kind(kind), loc(loc), expr(expr) {}

        Stmt(StmtKind kind, SourceLoc loc, Decl* decl)
            : kind(kind), loc(loc), decl(decl) {}
    };

    inline Stmt error_stmt = Stmt(StmtKind::Error, SourceLoc(), ErrorStmt());

} // namespace ariac
