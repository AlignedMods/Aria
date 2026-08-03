#include "ariac/semantic_analyzer/semantic_analyzer.hpp"

namespace ariac {

    void SemanticAnalyzer::resolve_block_stmt(Stmt* stmt) {
        BlockStmt& block = stmt->block;

        for (Stmt* s : block.stmts) {
            resolve_stmt(s);
        }

        if (m_scopes.back().reaches_end) {
            auto& scope = m_scopes.back();

            for (auto it = scope.defers.rbegin(); it != scope.defers.rend(); it++) {
                Stmt* d = *it;
                d->next = block.cleanup;
                block.cleanup = d;
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

            if (wh.condition->const_.boolean && wh.body->block.stmts.size == 0) {
                m_scopes.back().reaches_end = false;
                wh.infinite = true;
            }
        }

        auto prev = set_break_targets(stmt, stmt);
        push_scope();
        resolve_block_stmt(wh.body);
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

            if (wh.condition->const_.boolean && wh.body->block.stmts.size == 0) {
                m_scopes.back().reaches_end = false;
                wh.infinite = true;
            }
        }

        auto prev = set_break_targets(stmt, stmt);
        push_scope();
        resolve_block_stmt(wh.body);
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

                if (fs.condition->const_.boolean && fs.body->block.stmts.size == 0) {
                    m_scopes.at(m_scopes.size() - 2).reaches_end = false;
                    fs.infinite = true;
                }
            }
        } else {
            if (fs.body->block.stmts.size == 0) {
                m_scopes.at(m_scopes.size() - 2).reaches_end = false;
                fs.infinite = true;
            }
        }

        if (fs.step) { resolve_expr(fs.step); }
        resolve_block_stmt(fs.body);
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

        push_scope();
        resolve_block_stmt(ifs.body);
        pop_scope();

        if (ifs.else_body) {
            push_scope();
            resolve_block_stmt(ifs.else_body);
            pop_scope();
        }
    }

    void SemanticAnalyzer::resolve_switch_stmt(Stmt* stmt) {
        SwitchStmt& s = stmt->switch_;

        resolve_expr(s.expression);
        require_rvalue(s.expression);
        insert_expr_with_cleanups(s.expression);

        if (!s.expression->type->is_integral() && !s.expression->type->is_typeid()) {
            report_diag(s.expression->loc, fmt::format("Expression must be of an integral type but is '{}'", type_info_to_string(s.expression->type)));
        }

        for (Stmt* case_ : s.cases) {
            ARIA_ASSERT(case_->kind == StmtKind::Case, "Invalid case stmt");
            CaseStmt& c = case_->case_;

            resolve_expr(c.condition);
            try_insert_implicit_cast(s.expression->type, c.condition);
            require_rvalue(c.condition);

            if (!is_const_expr(c.condition)) {
                report_diag(c.condition->loc, "Expression must be a compile time constant");
                c.condition->kind = ExprKind::Error;
            }

            c.condition = eval_const_expr(c.condition);

            push_scope();
            resolve_block_stmt(c.body);
            pop_scope();
        }
    }

    void SemanticAnalyzer::resolve_break_stmt(Stmt* stmt) {
        if (!m_break_target.target) {
            report_diag(stmt->loc, "Cannot use 'break' here");
        } else {
            m_scopes.back().reaches_end = false;
            stmt->break_.target = m_break_target.target;

            for (auto it = m_scopes.rbegin(); it != m_break_target.scope; it++) {
                auto& scope = *it;

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
            m_scopes.back().reaches_end = false;
            stmt->continue_.target = m_continue_target.target;

            for (auto it = m_scopes.rbegin(); it != m_continue_target.scope; it++) {
                auto& scope = *it;

                for (auto it2 = scope.defers.rbegin(); it2 != scope.defers.rend(); it2++) {
                    Stmt* d = *it2;
                    d->next = stmt->continue_.cleanup;
                    stmt->continue_.cleanup = d;
                }
            }
        }
    }

    void SemanticAnalyzer::resolve_return_stmt(Stmt* stmt) {
        ReturnStmt& ret = stmt->return_;
        
        if (m_active_return_type == nullptr) {
            report_diag(stmt->loc, "'return' statement out of function body is not allowed");
            if (ret.value) { ret.value->type = TypeInfo::get_error(); }
            return;
        }

        if (m_active_return_type->is_never()) {
            report_diag(stmt->loc, "Function with return type '!' should not have any return statement");
        }
        
        m_scopes.back().reaches_end = false;

        for (auto it = m_scopes.rbegin(); it != m_scopes.rend(); it++) {
            auto& scope = *it;

            for (auto it2 = scope.defers.rbegin(); it2 != scope.defers.rend(); it2++) {
                Stmt* d = *it2;
                d->next = ret.cleanup;
                ret.cleanup = d;
            }
        }

        if (ret.value) {
            resolve_expr(ret.value);
            try_insert_implicit_cast(m_active_return_type, ret.value);
            require_rvalue(ret.value);
            insert_expr_with_cleanups(ret.value);
        } else {
            if (!m_active_return_type->is_void() && !m_active_return_type->is_never()) {
                report_diag(stmt->loc, "Missing value for return statement");
            }
        }
    }

    void SemanticAnalyzer::resolve_defer_stmt(Stmt* stmt) {
        DeferStmt& defer = stmt->defer;
        resolve_stmt(defer.statement);
        m_scopes.back().defers.push_back(defer.statement);
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
        if (m_scopes.size() > 0 && !m_scopes.back().reaches_end) {
            report_diag(stmt->loc, "This statement is never reached", CompilerDiagKind::Warning);
            stmt->reached = false;
        }

        switch (stmt->kind) {
            case StmtKind::Error:
            case StmtKind::Nop: return;

            case StmtKind::Block: {
                push_scope();
                resolve_block_stmt(stmt);

                if (!m_scopes.back().reaches_end) {
                    m_scopes[m_scopes.size() - 2].reaches_end = false;
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
            case StmtKind::Return: return resolve_return_stmt(stmt);
            case StmtKind::Defer: return resolve_defer_stmt(stmt);
            case StmtKind::Expr: return resolve_expr_stmt(stmt);
            case StmtKind::Decl: return resolve_decl_stmt(stmt);

            default: ARIA_UNREACHABLE("Invalid stmt kind");
        }
    }

} // namespace ariac