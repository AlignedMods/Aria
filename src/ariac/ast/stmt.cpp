#include "ariac/ast/stmt.hpp"
#include "ariac/ast/expr.hpp"
#include "ariac/ast/decl.hpp"

namespace ariac {

    Stmt* Stmt::dup(Stmt* s) {
        Stmt* copy = Stmt::Create(s->loc, s->kind, ErrorStmt());

        switch (s->kind) {
            case StmtKind::Error: break;

            case StmtKind::Block: {
                BlockStmt& b = s->block;

                for (Stmt* st : b.stmts) {
                    copy->block.stmts.append(Stmt::dup(st));
                }

                break;
            }

            case StmtKind::If: {
                IfStmt& i = s->if_;

                copy->if_.condition = Expr::dup(i.condition);
                copy->if_.body = Stmt::dup(i.body);
                if (i.else_body) { copy->if_.else_body = Stmt::dup(i.else_body); }

                break;
            }

            case StmtKind::Return: {
                ReturnStmt& r = s->return_;
                if (r.value) { copy->return_.value = Expr::dup(r.value); }
                break;
            }

            case StmtKind::Expr: {
                copy->expr = Expr::dup(s->expr);
                break;
            }

            case StmtKind::Decl: {
                copy->decl = Decl::dup(s->decl);
                break;
            }

            default: ARIA_UNREACHABLE("Invalid decl kind");
        }

        return copy;
    }

} // namespace ariac