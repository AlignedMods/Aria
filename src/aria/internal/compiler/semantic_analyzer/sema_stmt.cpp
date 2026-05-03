#include "aria/internal/compiler/semantic_analyzer/semantic_analyzer.hpp"

namespace Aria::Internal {

    void SemanticAnalyzer::resolve_block_stmt(Stmt* stmt) {
        BlockStmt block = stmt->block;

        bool wasUnsafe = m_unsafe_context;
        if (!m_unsafe_context) { m_unsafe_context = block.unsafe; }

        for (Stmt* s : block.stmts) {
            resolve_stmt(s);
        }

        m_unsafe_context = wasUnsafe;
    }

    void SemanticAnalyzer::resolve_while_stmt(Stmt* stmt) {
        WhileStmt wh = stmt->while_;

        resolve_expr(wh.condition);
        require_rvalue(wh.condition);

        if (!wh.condition->type->is_boolean()) {
            ARIA_ASSERT(false, "todo: add error");
        }

        // Check for loops such as "while true"
        if (is_const_expr(wh.condition) && eval_expr_bool(wh.condition)) {
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
        }

        // Check for loops such as "do {} while true"
        if (is_const_expr(wh.condition) && eval_expr_bool(wh.condition)) {
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
                m_context->report_compiler_diagnostic(fs.condition->loc, fs.condition->range, fmt::format("For loop condition must be of a boolean type but is '{}'", type_info_to_string(fs.condition->type)));
            }

            if (is_const_expr(fs.condition) && eval_expr_bool(fs.condition)) {
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
        push_scope();

        resolve_expr(ifs.condition);
        require_rvalue(ifs.condition);

        if (!ifs.condition->type->is_boolean()) {
            ARIA_ASSERT(false, "todo: add error");
        }

        resolve_block_stmt(ifs.body);

        pop_scope();
    }

    void SemanticAnalyzer::resolve_break_stmt(Stmt* stmt) {
        if (!m_scopes.back().allow_break_stmt) {
            m_context->report_compiler_diagnostic(stmt->loc, stmt->range, "Cannot use 'break' here");
        } else {
            m_scopes.back().reaches_end = false;
        }
    }

    void SemanticAnalyzer::resolve_continue_stmt(Stmt* stmt) {
        if (!m_scopes.back().allow_break_stmt) {
            m_context->report_compiler_diagnostic(stmt->loc, stmt->range, "Cannot use 'continue' here");
        } else {
            m_scopes.back().reaches_end = false;
        }
    }

    void SemanticAnalyzer::resolve_return_stmt(Stmt* stmt) {
        ReturnStmt& ret = stmt->return_;
        
        if (m_active_return_type == nullptr) {
            m_context->report_compiler_diagnostic(stmt->loc, stmt->range, "'return' statement out of function body is not allowed");
            ret.value->type = &error_type;
            return;
        }
        
        resolve_expr(ret.value);
        require_rvalue(ret.value);

        ConversionCost cost = get_conversion_cost(m_active_return_type, ret.value->type);
        if (cost.cast_needed) {
            if (cost.implicit_cast_possible) {
                insert_implicit_cast(m_active_return_type, ret.value->type, ret.value, cost.kind);
            } else {
                m_context->report_compiler_diagnostic(ret.value->loc, ret.value->range, fmt::format("Cannot implicitly convert '{}' to return type '{}'", type_info_to_string(ret.value->type), type_info_to_string(m_active_return_type)));
            }
        }

        m_scopes.back().reaches_end = false;
    }

    void SemanticAnalyzer::resolve_expr_stmt(Stmt* stmt) {
        resolve_expr(stmt->expr);
    }

    void SemanticAnalyzer::resolve_decl_stmt(Stmt* stmt) {
        resolve_decl(stmt->decl);
    }

    void SemanticAnalyzer::resolve_stmt(Stmt* stmt) {
        switch (stmt->kind) {
            case StmtKind::Error:
            case StmtKind::Import:
            case StmtKind::Nop: return;

            case StmtKind::Block: push_scope(); resolve_block_stmt(stmt); pop_scope(); return;
            case StmtKind::While: return resolve_while_stmt(stmt);
            case StmtKind::DoWhile: return resolve_do_while_stmt(stmt);
            case StmtKind::For: return resolve_for_stmt(stmt);
            case StmtKind::If: return resolve_if_stmt(stmt);
            case StmtKind::Break: return resolve_break_stmt(stmt);
            case StmtKind::Continue: return resolve_continue_stmt(stmt);
            case StmtKind::Return: return resolve_return_stmt(stmt);
            case StmtKind::Expr: return resolve_expr_stmt(stmt);
            case StmtKind::Decl: return resolve_decl_stmt(stmt);

            default: ARIA_UNREACHABLE();
        }
    }

} // namespace Aria::Internal