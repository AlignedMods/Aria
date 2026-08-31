#include "ariac/semantic_analyzer/semantic_analyzer.hpp"

namespace ariac {

    void SemanticAnalyzer::resolve_compound_stmt(Stmt* stmt) {
        CompoundStmt& compound = stmt->compound;

        for (Stmt* s : compound.stmts) {
            resolve_stmt(s);
        }

        if (m_functions.back().scopes.back().reaches_end) {
            auto& scope = m_functions.back().scopes.back();

            for (auto it = scope.defers.rbegin(); it != scope.defers.rend(); it++) {
                Stmt* d = *it;
                d->next = compound.cleanup;
                compound.cleanup = d;
            }
        }
    }

    void SemanticAnalyzer::resolve_while_stmt(Stmt* stmt) {
        WhileStmt& wh = stmt->while_;

        resolve_expr(wh.condition);
        require_rvalue(wh.condition);
        insert_expr_with_cleanups(wh.condition);

        if (!wh.condition->type->is_boolean()) {
            report_diag(wh.condition->loc, fmt::format("Expression must be of type 'bool' but is '{}'", type_info_to_string(wh.condition->type)));
        } else if (is_const_expr(wh.condition)) {
            wh.condition = eval_const_expr(wh.condition);

            if (wh.condition->const_.boolean && wh.body->compound.stmts.size == 0) {
                m_functions.back().scopes.back().reaches_end = false;
                wh.infinite = true;
            }
        }

        auto prev = set_break_targets(stmt, stmt);
        push_scope();
        resolve_compound_stmt(wh.body);
        pop_scope();
        restore_break_targets(prev);
    }

    void SemanticAnalyzer::resolve_do_while_stmt(Stmt* stmt) {
        DoWhileStmt& wh = stmt->do_while;

        resolve_expr(wh.condition);
        require_rvalue(wh.condition);
        insert_expr_with_cleanups(wh.condition);

        if (!wh.condition->type->is_boolean()) {
            report_diag(wh.condition->loc, fmt::format("Expression must be of type 'bool' but is'{}'", type_info_to_string(wh.condition->type)));
        } else if (is_const_expr(wh.condition)) {
            wh.condition = eval_const_expr(wh.condition);

            if (wh.condition->const_.boolean && wh.body->compound.stmts.size == 0) {
                m_functions.back().scopes.back().reaches_end = false;
                wh.infinite = true;
            }
        }

        auto prev = set_break_targets(stmt, stmt);
        push_scope();
        resolve_compound_stmt(wh.body);
        pop_scope();
        restore_break_targets(prev);
    }

    void SemanticAnalyzer::resolve_for_stmt(Stmt* stmt) {
        ForStmt& fs = stmt->for_;

        auto prev = set_break_targets(stmt, stmt);
        push_scope();
        if (fs.prologue) { resolve_decl(fs.prologue); }

        if (fs.condition) {
            resolve_expr(fs.condition);
            require_rvalue(fs.condition);
            insert_expr_with_cleanups(fs.condition);

            if (!fs.condition->type->is_boolean() && !fs.condition->type->is_error()) {
                report_diag(fs.condition->loc, fmt::format("For loop condition must be of a boolean type but is '{}'", type_info_to_string(fs.condition->type)));
            } else if (is_const_expr(fs.condition)) {
                fs.condition = eval_const_expr(fs.condition);

                if (fs.condition->const_.boolean && fs.body->compound.stmts.size == 0) {
                    m_functions.back().scopes.at(m_functions.back().scopes.size() - 2).reaches_end = false;
                    fs.infinite = true;
                }
            }
        } else {
            if (fs.body->compound.stmts.size == 0) {
                m_functions.back().scopes.at(m_functions.back().scopes.size() - 2).reaches_end = false;
                fs.infinite = true;
            }
        }

        if (fs.step) { resolve_expr(fs.step); }
        resolve_compound_stmt(fs.body);
        pop_scope();
        restore_break_targets(prev);
    }

    void SemanticAnalyzer::resolve_if_stmt(Stmt* stmt) {
        IfStmt& ifs = stmt->if_;

        resolve_expr(ifs.condition);
        require_rvalue(ifs.condition);
        insert_expr_with_cleanups(ifs.condition);

        if (!ifs.condition->type->is_boolean()) {
            report_diag(ifs.condition->loc, fmt::format("Expression must be of type 'bool' but is '{}'", type_info_to_string(ifs.condition->type)));
        }

        bool main_reaches_end = true;
        bool else_reaches_end = true;
        push_scope();
        resolve_compound_stmt(ifs.body);
        main_reaches_end = m_functions.back().scopes.back().reaches_end;
        pop_scope();

        if (ifs.else_body) {
            push_scope();
            resolve_compound_stmt(ifs.else_body);
            else_reaches_end = m_functions.back().scopes.back().reaches_end;
            pop_scope();
        }

        if (!main_reaches_end && !else_reaches_end) {
            m_functions.back().scopes.back().reaches_end = false;
        }
    }

    void SemanticAnalyzer::resolve_switch_stmt(Stmt* stmt) {
        SwitchStmt& s = stmt->switch_;

        resolve_expr(s.expression);
        require_rvalue(s.expression);
        insert_expr_with_cleanups(s.expression);

        if (!s.expression->type->is_integral() && !s.expression->type->is_typeid() && !s.expression->type->is_enum()) {
            report_diag(s.expression->loc, fmt::format("Expression must be of an integral type but is '{}'", type_info_to_string(s.expression->type)));
        }

        for (size_t i = 0; i < s.cases.size; i++) {
            Stmt* case_ = s.cases.items[i];
            ARIA_ASSERT(case_->kind == StmtKind::Case, "Invalid case stmt");
            CaseStmt& c = case_->case_;
            auto prev = set_nextcase_target(i + 1 < s.cases.size ? s.cases.items[i + 1] : nullptr);

            resolve_expr(c.condition);
            try_insert_implicit_cast(s.expression->type, c.condition);
            require_rvalue(c.condition);

            if (!is_const_expr(c.condition)) {
                report_diag(c.condition->loc, "Expression must be a compile time constant");
                c.condition->kind = ExprKind::Error;
            }

            c.condition = eval_const_expr(c.condition);

            push_scope();
            resolve_compound_stmt(c.body);
            pop_scope();
            restore_nextcase_target(prev);
        }
    }

    void SemanticAnalyzer::resolve_break_stmt(Stmt* stmt) {
        if (!m_break_target.target) {
            report_diag(stmt->loc, "Cannot use 'break' here");
        } else {
            m_functions.back().scopes.back().reaches_end = false;
            stmt->break_.target = m_break_target.target;

            for (size_t i = m_functions.back().scopes.size(); i > m_break_target.scope_idx; i--) {
                auto& scope = m_functions.back().scopes[i - 1];

                for (auto it2 = scope.defers.rbegin(); it2 != scope.defers.rend(); it2++) {
                    Stmt* d = *it2;
                    d->next = stmt->break_.cleanup;
                    stmt->break_.cleanup = d;
                }
            }
        }
    }

    void SemanticAnalyzer::resolve_continue_stmt(Stmt* stmt) {
        if (!m_continue_target.target) {
            report_diag(stmt->loc, "Cannot use 'continue' here");
        } else {
            m_functions.back().scopes.back().reaches_end = false;
            stmt->continue_.target = m_continue_target.target;

            for (size_t i = m_functions.back().scopes.size(); i > m_continue_target.scope_idx; i--) {
                auto& scope = m_functions.back().scopes[i - 1];

                for (auto it2 = scope.defers.rbegin(); it2 != scope.defers.rend(); it2++) {
                    Stmt* d = *it2;
                    d->next = stmt->continue_.cleanup;
                    stmt->continue_.cleanup = d;
                }
            }
        }
    }

    void SemanticAnalyzer::resolve_nextcase_stmt(Stmt* stmt) {
        if (!m_nextcase_target.target) {
            report_diag(stmt->loc, "Cannot use 'nextcase' here");
        } else {
            m_functions.back().scopes.back().reaches_end = false;
            stmt->nextcase.target = m_nextcase_target.target;

            for (size_t i = m_functions.back().scopes.size(); i > m_nextcase_target.scope_idx; i--) {
                auto& scope = m_functions.back().scopes[i - 1];

                for (auto it2 = scope.defers.rbegin(); it2 != scope.defers.rend(); it2++) {
                    Stmt* d = *it2;
                    d->next = stmt->nextcase.cleanup;
                    stmt->nextcase.cleanup = d;
                }
            }
        }
    }

    void SemanticAnalyzer::resolve_return_stmt(Stmt* stmt) {
        ReturnStmt& ret = stmt->return_;

        if (m_functions.back().return_type->is_never()) {
            report_diag(stmt->loc, "Function with return type '!' should not have any return statement");
        }
        
        m_functions.back().scopes.back().reaches_end = false;

        for (auto it = m_functions.back().scopes.rbegin(); it != m_functions.back().scopes.rend(); it++) {
            auto& scope = *it;

            for (auto it2 = scope.defers.rbegin(); it2 != scope.defers.rend(); it2++) {
                Stmt* d = *it2;
                d->next = ret.cleanup;
                ret.cleanup = d;
            }
        }

        if (ret.value) {
            resolve_expr(ret.value);
            try_insert_implicit_cast(m_functions.back().return_type, ret.value, "return type");
            require_rvalue(ret.value);
            insert_expr_with_cleanups(ret.value);
        } else {
            if (!m_functions.back().return_type->is_void() && !m_functions.back().return_type->is_never()) {
                report_diag(stmt->loc, "Missing value for return statement");
            }
        }
    }

    void SemanticAnalyzer::resolve_defer_stmt(Stmt* stmt) {
        DeferStmt& defer = stmt->defer;
        resolve_stmt(defer.statement);
        m_functions.back().scopes.back().defers.push_back(defer.statement);
    }

    void SemanticAnalyzer::resolve_compile_if_stmt(Stmt* stmt) {
        IfStmt& i = stmt->if_;

        resolve_expr(i.condition);
        require_rvalue(i.condition);
        insert_expr_with_cleanups(i.condition);

        if (i.condition->type->is_dependent()) {
            return;
        }

        if (!i.condition->type->is_boolean()) {
            report_diag(i.condition->loc, fmt::format("Expression must be of type 'bool' but is '{}'", type_info_to_string(i.condition->type)));
            stmt->kind = StmtKind::Error;
            return;
        }

        if (!is_const_expr(i.condition)) {
            report_error(i.condition->loc, "Expression must be a compile time constant");
            stmt->kind = StmtKind::Error;
            return;
        }

        i.condition = eval_const_expr(i.condition);
        ARIA_ASSERT(i.condition->const_.kind == ConstExprKind::Boolean, "Invalid const expr");
        
        if (i.condition->const_.boolean) {
            push_scope();
            resolve_compound_stmt(i.body);
            bool reaches_end = m_functions.back().scopes.back().reaches_end;
            pop_scope();

            replace_stmt(stmt, i.body);

            m_functions.back().scopes.back().reaches_end = reaches_end;
        } else {
            if (i.else_body) {
                push_scope();
                resolve_compound_stmt(i.else_body);
                bool reaches_end = m_functions.back().scopes.back().reaches_end;
                pop_scope();

                replace_stmt(stmt, i.else_body);

                m_functions.back().scopes.back().reaches_end = reaches_end;
            } else {
                replace_stmt(stmt, Stmt::Create(stmt->loc, StmtKind::Nop, ErrorStmt()));
            }
        }
    }

    void SemanticAnalyzer::resolve_assert_stmt(Stmt* stmt) {
        AssertStmt& a = stmt->assert_;

        bool temp = m_sema_context.temporary;
        m_sema_context.temporary = true;
        resolve_expr(a.condition);
        require_rvalue(a.condition);
        m_sema_context.temporary = temp;

        for (Expr* arg : a.arguments) {
            bool temp = m_sema_context.temporary;
            m_sema_context.temporary = true;
            resolve_expr(arg);
            require_rvalue(arg);
            m_sema_context.temporary = temp;
        }

        if (a.condition->type->is_error()) { return; }

        if (!a.condition->type->is_boolean()) {
            report_error(a.condition->loc, fmt::format("Expression must be of type 'bool' but is '{}'", a.condition->type->to_string()));
            return;
        }

        if (is_const_expr(a.condition)) {
            a.condition = eval_const_expr(a.condition);

            if (!a.condition->const_.boolean) {
                report_error(stmt->loc, "This assertion will always fail");
                return;
            }
        }

        if (a.arguments.size > 0) {
            if (a.arguments[0]->kind != ExprKind::StringLiteral) {
                report_error(a.condition->loc, "Expression must be a string literal");
                return;
            }
        }
    }

    void SemanticAnalyzer::resolve_expr_stmt(Stmt* stmt) {
        m_sema_context.temporary = true;
        resolve_expr(stmt->expr);
        m_sema_context.temporary = false;
        insert_expr_with_cleanups(stmt->expr);
    }

    void SemanticAnalyzer::resolve_decl_stmt(Stmt* stmt) {
        resolve_decl(stmt->decl);
    }

    void SemanticAnalyzer::resolve_stmt(Stmt* stmt) {
        if (!m_functions.back().scopes.back().reaches_end) {
            report_diag(stmt->loc, "This statement is never reached", CompilerDiagKind::Warning);
            stmt->reached = false;
        }

        switch (stmt->kind) {
            case StmtKind::Error:
            case StmtKind::Nop: return;

            case StmtKind::Compound: {
                push_scope();
                resolve_compound_stmt(stmt);

                if (!m_functions.back().scopes.back().reaches_end) {
                    m_functions.back().scopes[m_functions.back().scopes.size() - 2].reaches_end = false;
                }

                pop_scope(); 
                return;
            }
            case StmtKind::While: return resolve_while_stmt(stmt);
            case StmtKind::DoWhile: return resolve_do_while_stmt(stmt);
            case StmtKind::For: return resolve_for_stmt(stmt);
            case StmtKind::If: return resolve_if_stmt(stmt);
            case StmtKind::Switch: return resolve_switch_stmt(stmt);
            case StmtKind::Break: return resolve_break_stmt(stmt);
            case StmtKind::Continue: return resolve_continue_stmt(stmt);
            case StmtKind::Nextcase: return resolve_nextcase_stmt(stmt);
            case StmtKind::Return: return resolve_return_stmt(stmt);
            case StmtKind::Defer: return resolve_defer_stmt(stmt);
            case StmtKind::CompileIf: return resolve_compile_if_stmt(stmt);
            case StmtKind::Assert: return resolve_assert_stmt(stmt);
            case StmtKind::Expr: return resolve_expr_stmt(stmt);
            case StmtKind::Decl: return resolve_decl_stmt(stmt);

            default: ARIA_UNREACHABLE("Invalid stmt kind");
        }
    }

} // namespace ariac