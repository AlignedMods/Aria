#include "aria/internal/compiler/semantic_analyzer/semantic_analyzer.hpp"

namespace Aria::Internal {

    void SemanticAnalyzer::resolve_Nop_stmt(Stmt* stmt) {}
    void SemanticAnalyzer::resolve_Import_stmt(Stmt* stmt) {}

    void SemanticAnalyzer::resolve_Block_stmt(Stmt* stmt) {
        BlockStmt block = stmt->Block;

        bool wasUnsafe = m_UnsafeContext;
        if (!m_UnsafeContext) { m_UnsafeContext = block.Unsafe; }

        for (Stmt* s : block.Stmts) {
            resolve_stmt(s);
        }

        m_UnsafeContext = wasUnsafe;
    }

    void SemanticAnalyzer::resolve_While_stmt(Stmt* stmt) {
        WhileStmt wh = stmt->While;

        resolve_expr(wh.Condition);
        require_rvalue(wh.Condition);

        if (!wh.Condition->Type->is_boolean()) {
            ARIA_ASSERT(false, "todo: add error");
        }

        // Check for loops such as "while true"
        if (is_const_expr(wh.Condition) && eval_expr_bool(wh.Condition)) {
            m_Scopes.back().ReachesEnd = false;
        }

        push_scope(true, true);
        resolve_Block_stmt(wh.Body);
        pop_scope();
    }

    void SemanticAnalyzer::resolve_DoWhile_stmt(Stmt* stmt) {
        DoWhileStmt wh = stmt->DoWhile;

        resolve_expr(wh.Condition);
        require_rvalue(wh.Condition);

        if (!wh.Condition->Type->is_boolean()) {
            ARIA_ASSERT(false, "todo: add error");
        }

        // Check for loops such as "do {} while true"
        if (is_const_expr(wh.Condition) && eval_expr_bool(wh.Condition)) {
            m_Scopes.back().ReachesEnd = false;
        }

        push_scope(true, true);
        resolve_Block_stmt(wh.Body);
        pop_scope();
    }

    void SemanticAnalyzer::resolve_For_stmt(Stmt* stmt) {
        ForStmt fs = stmt->For;

        if (fs.Condition) {
            resolve_expr(fs.Condition);
            require_rvalue(fs.Condition);

            if (!fs.Condition->Type->is_boolean() && !fs.Condition->Type->is_error()) {
                m_Context->report_compiler_diagnostic(fs.Condition->Loc, fs.Condition->Range, fmt::format("For loop condition must be of a boolean type but is '{}'", type_info_to_string(fs.Condition->Type)));
            }

            if (is_const_expr(fs.Condition) && eval_expr_bool(fs.Condition)) {
                m_Scopes.back().ReachesEnd = false;
            }
        } else {
            m_Scopes.back().ReachesEnd = false;
        }

        push_scope(true, true);
        if (fs.Prologue) { resolve_decl(fs.Prologue); }
        if (fs.Step) { resolve_expr(fs.Step); }
        resolve_Block_stmt(fs.Body);
        pop_scope();
    }

    void SemanticAnalyzer::resolve_If_stmt(Stmt* stmt) {
        IfStmt ifs = stmt->If;
        push_scope();

        resolve_expr(ifs.Condition);
        require_rvalue(ifs.Condition);

        if (!ifs.Condition->Type->is_boolean()) {
            ARIA_ASSERT(false, "todo: add error");
        }

        resolve_Block_stmt(ifs.Body);

        pop_scope();
    }

    void SemanticAnalyzer::resolve_Break_stmt(Stmt* stmt) {
        if (!m_Scopes.back().AllowBreakStmt) {
            m_Context->report_compiler_diagnostic(stmt->Loc, stmt->Range, "Cannot use 'break' here");
        } else {
            m_Scopes.back().ReachesEnd = false;
        }
    }

    void SemanticAnalyzer::resolve_Continue_stmt(Stmt* stmt) {
        if (!m_Scopes.back().AllowContinueStmt) {
            m_Context->report_compiler_diagnostic(stmt->Loc, stmt->Range, "Cannot use 'continue' here");
        } else {
            m_Scopes.back().ReachesEnd = false;
        }
    }

    void SemanticAnalyzer::resolve_Return_stmt(Stmt* stmt) {
        ReturnStmt& ret = stmt->Return;
        
        if (m_ActiveReturnType == nullptr) {
            m_Context->report_compiler_diagnostic(stmt->Loc, stmt->Range, "'return' statement out of function body is not allowed");
            ret.Value->Type = &error_type;
            return;
        }
        
        resolve_expr(ret.Value);
        require_rvalue(ret.Value);

        ConversionCost cost = get_conversion_cost(m_ActiveReturnType, ret.Value->Type);
        if (cost.CastNeeded) {
            if (cost.ImplicitCastPossible) {
                insert_implicit_cast(m_ActiveReturnType, ret.Value->Type, ret.Value, cost.Kind);
            } else {
                m_Context->report_compiler_diagnostic(ret.Value->Loc, ret.Value->Range, fmt::format("Cannot implicitly convert '{}' to return type '{}'", type_info_to_string(ret.Value->Type), type_info_to_string(m_ActiveReturnType)));
            }
        }

        m_Scopes.back().ReachesEnd = false;
    }

    void SemanticAnalyzer::resolve_Expr_stmt(Stmt* stmt) {
        resolve_expr(stmt->ExprStmt);
    }

    void SemanticAnalyzer::resolve_Decl_stmt(Stmt* stmt) {
        resolve_decl(stmt->DeclStmt);
    }

    void SemanticAnalyzer::resolve_stmt(Stmt* stmt) {
        #define STMT_CASE(kind) resolve_##kind##_stmt(stmt)
        #include "aria/internal/compiler/ast/stmt_switch.hpp"
        #undef STMT_CASE
    }

} // namespace Aria::Internal