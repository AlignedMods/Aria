#include "ariac/ast/stmt.hpp"
#include "ariac/ast/expr.hpp"
#include "ariac/ast/decl.hpp"

namespace ariac {

    Stmt* Stmt::dup(CompilationContext* ctx, Stmt* s) {
        Stmt* copy = Stmt::Create(ctx, s->loc, s->kind, ErrorStmt());

        switch (s->kind) {
            case StmtKind::Error: break;

            case StmtKind::Block: {
                BlockStmt& b = s->block;

                for (Stmt* st : b.stmts) {
                    copy->block.stmts.append(ctx, Stmt::dup(ctx, st));
                }

                copy->block.unsafe = b.unsafe;
                break;
            }

            case StmtKind::If: {
                IfStmt& i = s->if_;

                copy->if_.condition = Expr::dup(ctx, i.condition);
                copy->if_.body = Stmt::dup(ctx, i.body);
                if (i.else_body) { copy->if_.else_body = Stmt::dup(ctx, i.else_body); }

                break;
            }

            case StmtKind::Return: {
                ReturnStmt& r = s->return_;
                if (r.value) { copy->return_.value = Expr::dup(ctx, r.value); }
                break;
            }

            case StmtKind::Expr: {
                copy->expr = Expr::dup(ctx, s->expr);
                break;
            }

            case StmtKind::Decl: {
                copy->decl = Decl::dup(ctx, s->decl);
                break;
            }

            default: ARIA_UNREACHABLE();
        }

        return copy;
    }

} // namespace ariac