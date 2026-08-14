#include "ariac/ast/stmt.hpp"
#include "ariac/ast/expr.hpp"
#include "ariac/ast/decl.hpp"

namespace ariac {

    Stmt* Stmt::dup(Stmt* s) {
        Stmt* copy = Stmt::Create(s->loc, s->kind, ErrorStmt());

        switch (s->kind) {
            case StmtKind::Error:
            case StmtKind::Nop: break;

            case StmtKind::Block: {
                BlockStmt& b = s->block;

                for (Stmt* st : b.stmts) {
                    copy->block.stmts.append(Stmt::dup(st));
                }

                break;
            }

            case StmtKind::If:
            case StmtKind::CompileIf: {
                IfStmt& i = s->if_;

                copy->if_.condition = Expr::dup(i.condition);
                copy->if_.body = Stmt::dup(i.body);
                if (i.else_body) { copy->if_.else_body = Stmt::dup(i.else_body); }

                break;
            }

            case StmtKind::Switch: {
                SwitchStmt& sw = s->switch_;
                copy->switch_.expression = Expr::dup(sw.expression);
                
                for (Stmt* c : sw.cases) {
                    copy->switch_.cases.append(Stmt::dup(c));
                }

                break;
            }

            case StmtKind::Case: {
                CaseStmt& c = s->case_;
                copy->case_.condition = Expr::dup(c.condition);
                copy->case_.body = Stmt::dup(c.body);
                break;
            }

            case StmtKind::Break: {
                BreakStmt& b = s->break_;
                copy->break_.target = b.target;
                break;
            }

            case StmtKind::Continue: {
                ContinueStmt& c = s->continue_;
                copy->continue_.target = c.target;
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

            default: ARIA_UNREACHABLE("Invalid stmt kind");
        }

        return copy;
    }

} // namespace ariac