#include "aria/internal/compiler/semantic_analyzer/semantic_analyzer.hpp"

namespace Aria::Internal {

    void SemanticAnalyzer::ResolveNopStmt(Stmt* stmt) {}
    void SemanticAnalyzer::ResolveImportStmt(Stmt* stmt) {}

    void SemanticAnalyzer::ResolveBlockStmt(Stmt* stmt) {
        BlockStmt block = stmt->Block;
        PushScope();

        bool wasUnsafe = m_UnsafeContext;
        if (!m_UnsafeContext) { m_UnsafeContext = block.Unsafe; }

        for (Stmt* s : block.Stmts) {
            ResolveStmt(s);
        }

        m_UnsafeContext = wasUnsafe;
        PopScope();
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
        
        ResolveExpr(ret.Value);
        RequireRValue(ret.Value);

        ConversionCost cost = GetConversionCost(m_ActiveReturnType, ret.Value->Type);
        if (cost.CastNeeded) {
            if (cost.ImplicitCastPossible) {
                InsertImplicitCast(m_ActiveReturnType, ret.Value->Type, ret.Value, cost.Kind);
            } else {
                m_Context->ReportCompilerDiagnostic(ret.Value->Loc, ret.Value->Range, fmt::format("Cannot implicitly convert '{}' to return type '{}'", TypeInfoToString(ret.Value->Type), TypeInfoToString(m_ActiveReturnType)));
            }
        }

        if (m_Scopes.size() == 1) {
            m_CanReachEndOfFunction = false;
        }
    }

    void SemanticAnalyzer::ResolveExprStmt(Stmt* stmt) {
        ResolveExpr(stmt->ExprStmt);
    }

    void SemanticAnalyzer::ResolveDeclStmt(Stmt* stmt) {
        ResolveDecl(stmt->DeclStmt);
    }

    void SemanticAnalyzer::ResolveStmt(Stmt* stmt) {
        #define STMT_CASE(kind) Resolve##kind##Stmt(stmt)
        #include "aria/internal/compiler/ast/stmt_switch.hpp"
        #undef STMT_CASE
    }

} // namespace Aria::Internal