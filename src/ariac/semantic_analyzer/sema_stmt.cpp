#include "ariac/semantic_analyzer/semantic_analyzer.hpp"

namespace ariac {

    void SemanticAnalyzer::resolve_block_stmt(Stmt* stmt) {
        BlockStmt block = stmt->block;

        for (Stmt* s : block.stmts) {
            resolve_stmt(s);
        }
    }

    void SemanticAnalyzer::resolve_while_stmt(Stmt* stmt) {
        WhileStmt wh = stmt->while_;

        resolve_expr(wh.condition);
        require_rvalue(wh.condition);

        if (!wh.condition->type->is_boolean()) {
            m_context->report_compiler_diagnostic(wh.condition->loc, fmt::format("Expression must be of type 'bool' but is '{}'", type_info_to_string(wh.condition->type)));
        } else if (is_const_expr(wh.condition) && eval_const_expr(wh.condition)->const_.boolean) {
            m_scopes.back().reaches_end = false;
        }

        push_scope(true, true);
        resolve_block_stmt(wh.body);
        pop_scope();
    }

    void SemanticAnalyzer::resolve_do_while_stmt(Stmt* stmt) {
        DoWhileStmt wh = stmt->do_while;

        resolve_expr(wh.condition);
        require_rvalue(wh.condition);

        if (!wh.condition->type->is_boolean()) {
            ARIA_ASSERT(false, "todo: add error");
        } else if (is_const_expr(wh.condition) && eval_const_expr(wh.condition)->const_.boolean) {
            m_scopes.back().reaches_end = false;
        }

        push_scope(true, true);
        resolve_block_stmt(wh.body);
        pop_scope();
    }

    void SemanticAnalyzer::resolve_for_stmt(Stmt* stmt) {
        ForStmt fs = stmt->for_;

        push_scope(true, true);
        if (fs.prologue) { resolve_decl(fs.prologue); }

        if (fs.condition) {
            resolve_expr(fs.condition);
            require_rvalue(fs.condition);

            if (!fs.condition->type->is_boolean() && !fs.condition->type->is_error()) {
                m_context->report_compiler_diagnostic(fs.condition->loc, fmt::format("For loop condition must be of a boolean type but is '{}'", type_info_to_string(fs.condition->type)));
            } else if (is_const_expr(fs.condition) && eval_const_expr(fs.condition)->const_.boolean) {
                m_scopes.back().reaches_end = false;
            }
        } else {
            m_scopes.back().reaches_end = false;
        }

        if (fs.step) { resolve_expr(fs.step); }
        resolve_block_stmt(fs.body);
        pop_scope();
    }

    void SemanticAnalyzer::resolve_if_stmt(Stmt* stmt) {
        IfStmt ifs = stmt->if_;

        resolve_expr(ifs.condition);
        require_rvalue(ifs.condition);

        if (!ifs.condition->type->is_boolean()) {
            m_context->report_compiler_diagnostic(ifs.condition->loc, fmt::format("Expression must be of type 'bool' but is '{}'", type_info_to_string(ifs.condition->type)));
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

    void SemanticAnalyzer::resolve_break_stmt(Stmt* stmt) {
        if (!m_scopes.back().allow_break_stmt) {
            m_context->report_compiler_diagnostic(stmt->loc, "Cannot use 'break' here");
        } else {
            m_scopes.back().reaches_end = false;
        }
    }

    void SemanticAnalyzer::resolve_continue_stmt(Stmt* stmt) {
        if (!m_scopes.back().allow_break_stmt) {
            m_context->report_compiler_diagnostic(stmt->loc, "Cannot use 'continue' here");
        } else {
            m_scopes.back().reaches_end = false;
        }
    }

    void SemanticAnalyzer::resolve_return_stmt(Stmt* stmt) {
        ReturnStmt& ret = stmt->return_;
        
        if (m_active_return_type == nullptr) {
            m_context->report_compiler_diagnostic(stmt->loc, "'return' statement out of function body is not allowed");
            ret.value->type = TypeInfo::get_error(m_context);
            return;
        }
        
        m_scopes.back().reaches_end = false;

        if (ret.value) {
            resolve_expr(ret.value);
            require_rvalue(ret.value);

            if (ret.value->type->is_error() || m_active_return_type->is_error()) {
                return;
            }

            ConversionCost cost = get_conversion_cost(m_active_return_type, ret.value->type);
            if (cost.cast_needed) {
                if (cost.implicit_cast_possible) {
                    insert_implicit_cast(m_active_return_type, ret.value->type, ret.value, cost.kind);
                } else {
                    m_context->report_compiler_diagnostic(ret.value->loc, fmt::format("Cannot implicitly convert '{}' to return type '{}'", type_info_to_string(ret.value->type), type_info_to_string(m_active_return_type)));
                }
            }
        }
    }

    void SemanticAnalyzer::resolve_defer_stmt(Stmt* stmt) {
        DeferStmt& defer = stmt->defer;
        resolve_stmt(defer.statement);
    }

    void SemanticAnalyzer::resolve_expr_stmt(Stmt* stmt) {
        resolve_expr(stmt->expr);
    }

    void SemanticAnalyzer::resolve_decl_stmt(Stmt* stmt) {
        resolve_decl(stmt->decl);
    }

    void SemanticAnalyzer::resolve_stmt(Stmt* stmt) {
        if (m_scopes.size() > 0 && !m_scopes.back().reaches_end) { stmt->reached = false; }

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
            case StmtKind::Break: return resolve_break_stmt(stmt);
            case StmtKind::Continue: return resolve_continue_stmt(stmt);
            case StmtKind::Return: return resolve_return_stmt(stmt);
            case StmtKind::Defer: return resolve_defer_stmt(stmt);
            case StmtKind::Expr: return resolve_expr_stmt(stmt);
            case StmtKind::Decl: return resolve_decl_stmt(stmt);

            default: ARIA_UNREACHABLE();
        }
    }

} // namespace ariac