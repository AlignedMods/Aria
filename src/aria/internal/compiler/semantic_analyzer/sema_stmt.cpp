#include "aria/internal/compiler/semantic_analyzer/semantic_analyzer.hpp"

namespace Aria::Internal {

    void SemanticAnalyzer::ResolveImportStmt(Stmt* stmt) {}

    void SemanticAnalyzer::ResolveBlockStmt(Stmt* stmt) {
        BlockStmt block = stmt->Block;

        for (Stmt* s : block.Stmts) {
            ResolveStmt(s);
        }
    }

    void SemanticAnalyzer::ResolveWhileStmt(Stmt* stmt) {
        WhileStmt wh = stmt->While;

        PushScope(true, true);

        ResolveExpr(wh.Condition);
        RequireRValue(wh.Condition);

        if (!wh.Condition->Type->IsBoolean()) {
            ARIA_ASSERT(false, "todo: add error");
        }

        ResolveBlockStmt(wh.Body);

        PopScope();
    }

    void SemanticAnalyzer::ResolveDoWhileStmt(Stmt* stmt) {
        DoWhileStmt wh = stmt->DoWhile;

        PushScope(true, true);

        ResolveExpr(wh.Condition);
        RequireRValue(wh.Condition);

        if (!wh.Condition->Type->IsBoolean()) {
            ARIA_ASSERT(false, "todo: add error");
        }

        ResolveBlockStmt(wh.Body);

        PopScope();
    }

    void SemanticAnalyzer::ResolveForStmt(Stmt* stmt) {
        ForStmt fs = stmt->For;

        PushScope(true, true);
        
        if (fs.Prologue) { ResolveDecl(fs.Prologue); }

        if (fs.Condition) {
            ResolveExpr(fs.Condition);
            RequireRValue(fs.Condition);

            if (!fs.Condition->Type->IsBoolean() && !fs.Condition->Type->IsError()) {
                m_Context->ReportCompilerDiagnostic(fs.Condition->Loc, fs.Condition->Range, fmt::format("For loop condition must be of a boolean type but is '{}'", TypeInfoToString(fs.Condition->Type)));
            }
        }

        if (fs.Step) { ResolveExpr(fs.Step); }
        ResolveBlockStmt(fs.Body);
        
        PopScope();
    }

    void SemanticAnalyzer::ResolveIfStmt(Stmt* stmt) {
        IfStmt ifs = stmt->If;
        PushScope();

        ResolveExpr(ifs.Condition);
        RequireRValue(ifs.Condition);

        if (!ifs.Condition->Type->IsBoolean()) {
            ARIA_ASSERT(false, "todo: add error");
        }

        ResolveBlockStmt(ifs.Body);

        PopScope();
    }

    void SemanticAnalyzer::ResolveBreakStmt(Stmt* stmt) {
        if (!m_Scopes.back().AllowBreakStmt) {
            m_Context->ReportCompilerDiagnostic(stmt->Loc, stmt->Range, "Cannot use 'break' here");
        }
    }

    void SemanticAnalyzer::ResolveContinueStmt(Stmt* stmt) {
        if (!m_Scopes.back().AllowContinueStmt) {
            m_Context->ReportCompilerDiagnostic(stmt->Loc, stmt->Range, "Cannot use 'continue' here");
        }
    }

    void SemanticAnalyzer::ResolveReturnStmt(Stmt* stmt) {
        ReturnStmt& ret = stmt->Return;
        
        if (m_ActiveReturnType == nullptr) {
            m_Context->ReportCompilerDiagnostic(stmt->Loc, stmt->Range, "'return' statement out of function body is not allowed");
            ret.Value->Type = &ErrorType;
            return;
        }
        
        ResolveInitializer(&ret.Value, m_ActiveReturnType, false);

        if (m_Scopes.size() == 1) {
            m_CanReachEndOfFunction = false;
        }
    }

    void SemanticAnalyzer::ResolveStmt(Stmt* stmt) {
        if (stmt->Kind == StmtKind::Error) { return; }
        else if (stmt->Kind == StmtKind::Nop) { return; }
        else if (stmt->Kind == StmtKind::Import) {
            return ResolveImportStmt(stmt);
        } else if (stmt->Kind == StmtKind::Block) {
            return ResolveBlockStmt(stmt);
        } else if (stmt->Kind == StmtKind::While) {
            return ResolveWhileStmt(stmt);
        } else if (stmt->Kind == StmtKind::DoWhile) {
            return ResolveDoWhileStmt(stmt);
        } else if (stmt->Kind == StmtKind::For) {
            return ResolveForStmt(stmt);
        } else if (stmt->Kind == StmtKind::If) {
            return ResolveIfStmt(stmt);
        } else if (stmt->Kind == StmtKind::Break) {
            return ResolveBreakStmt(stmt);
        } else if (stmt->Kind == StmtKind::Continue) {
            return ResolveContinueStmt(stmt);
        } else if (stmt->Kind == StmtKind::Return) {
            return ResolveReturnStmt(stmt);
        } else if (stmt->Kind == StmtKind::Expr) {
            return ResolveExpr(stmt->ExprStmt);
        } else if (stmt->Kind == StmtKind::Decl) {
            return ResolveDecl(stmt->DeclStmt);
        }

        ARIA_UNREACHABLE();
    }

} // namespace Aria::Internal