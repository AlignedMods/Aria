#include "aria/internal/compiler/semantic_analyzer/semantic_analyzer.hpp"

namespace Aria::Internal {

    SemanticAnalyzer::SemanticAnalyzer(CompilationContext* ctx) : m_TypeChecker(ctx) {
        m_Context = ctx;
        m_RootASTNode = ctx->GetRootASTNode();

        m_TypeChecker.CheckImpl();
        AnalyzeImpl();
    }

    void SemanticAnalyzer::AnalyzeImpl() {
        HandleStmt(m_RootASTNode);
    }

    void SemanticAnalyzer::HandleBooleanConstantExpr(Expr* expr)   {}
    void SemanticAnalyzer::HandleCharacterConstantExpr(Expr* expr) {}
    void SemanticAnalyzer::HandleIntegerConstantExpr(Expr* expr)   {}
    void SemanticAnalyzer::HandleFloatingConstantExpr(Expr* expr)  {}
    void SemanticAnalyzer::HandleStringConstantExpr(Expr* expr)    {}
    void SemanticAnalyzer::HandleDeclRefExpr(Expr* expr)           {}
    void SemanticAnalyzer::HandleMemberExpr(Expr* expr)            {}
    void SemanticAnalyzer::HandleCallExpr(Expr* expr)              {}
    void SemanticAnalyzer::HandleMethodCallExpr(Expr* expr)        {}
    void SemanticAnalyzer::HandleParenExpr(Expr* expr)             {}
    void SemanticAnalyzer::HandleCastExpr(Expr* expr)              {}
    void SemanticAnalyzer::HandleImplicitCastExpr(Expr* expr)      {}
    void SemanticAnalyzer::HandleUnaryOperatorExpr(Expr* expr)     {}
    void SemanticAnalyzer::HandleBinaryOperatorExpr(Expr* expr)    {}
    void SemanticAnalyzer::HandleCompoundAssignExpr(Expr* expr)    {}

    void SemanticAnalyzer::HandleExpr(Expr* expr) {
        if (expr->Kind == ExprKind::BooleanConstant) {
            return HandleBooleanConstantExpr(expr);
        } else if (expr->Kind == ExprKind::CharacterConstant) {
            return HandleCharacterConstantExpr(expr);
        } else if (expr->Kind == ExprKind::IntegerConstant) {
            return HandleIntegerConstantExpr(expr);
        } else if (expr->Kind == ExprKind::FloatingConstant) {
            return HandleFloatingConstantExpr(expr);
        } else if (expr->Kind == ExprKind::StringConstant) {
            return HandleStringConstantExpr(expr);
        } else if (expr->Kind == ExprKind::DeclRef) {
            return HandleDeclRefExpr(expr);
        } else if (expr->Kind == ExprKind::Member) {
            return HandleMemberExpr(expr);
        } else if (expr->Kind == ExprKind::Call) {
            return HandleCallExpr(expr);
        } else if (expr->Kind == ExprKind::MethodCall) {
            return HandleMethodCallExpr(expr);
        } else if (expr->Kind == ExprKind::Paren) {
            return HandleParenExpr(expr);
        } else if (expr->Kind == ExprKind::Cast) {
            return HandleCastExpr(expr);
        } else if (expr->Kind == ExprKind::ImplicitCast) {
            return HandleImplicitCastExpr(expr);
        } else if (expr->Kind == ExprKind::UnaryOperator) {
            return HandleUnaryOperatorExpr(expr);
        } else if (expr->Kind == ExprKind::BinaryOperator) {
            return HandleBinaryOperatorExpr(expr);
        } else if (expr->Kind == ExprKind::CompoundAssign) {
            return HandleCompoundAssignExpr(expr);
        }

        ARIA_UNREACHABLE();
    }

    void SemanticAnalyzer::HandleTranslationUnitDecl(Decl* decl) {
        TranslationUnitDecl tu = decl->TranslationUnit;

        for (Stmt* stmt : tu.Stmts) {
            HandleStmt(stmt);
        }
    }

    void SemanticAnalyzer::HandleVarDecl(Decl* decl) {}
    void SemanticAnalyzer::HandleParamDecl(Decl* decl) {}
    void SemanticAnalyzer::HandleFunctionDecl(Decl* decl) {}
    void SemanticAnalyzer::HandleStructDecl(Decl* decl) {}

    void SemanticAnalyzer::HandleDecl(Decl* decl) {
        if (decl->Kind == DeclKind::Error) { return; }
        else if (decl->Kind == DeclKind::TranslationUnit) {
            return HandleTranslationUnitDecl(decl);
        } else if (decl->Kind == DeclKind::Var) {
            return HandleVarDecl(decl);
        } else if (decl->Kind == DeclKind::Param) {
            return HandleParamDecl(decl);
        } else if (decl->Kind == DeclKind::Function) {
            return HandleFunctionDecl(decl);
        } else if (decl->Kind == DeclKind::Struct) {
            return HandleStructDecl(decl);
        }

        ARIA_UNREACHABLE();
    }

    void SemanticAnalyzer::HandleBlockStmt(Stmt* stmt) {
        BlockStmt block = stmt->Block;

        for (Stmt* s : block.Stmts) {
            HandleStmt(s);
        }
    }

    void SemanticAnalyzer::HandleWhileStmt(Stmt* stmt) {
        WhileStmt wh = stmt->While;
        
        HandleExpr(wh.Condition);
        HandleStmt(wh.Body);
    }

    void SemanticAnalyzer::HandleDoWhileStmt(Stmt* stmt) {
        DoWhileStmt wh = stmt->DoWhile;
        
        HandleExpr(wh.Condition);
        HandleStmt(wh.Body);
    }

    void SemanticAnalyzer::HandleForStmt(Stmt* stmt) {
        ForStmt fs = stmt->For;
        
        if (fs.Prologue) { HandleDecl(fs.Prologue); }
        if (fs.Condition) { HandleExpr(fs.Condition); }
        if (fs.Step) { HandleExpr(fs.Step); }
        HandleStmt(fs.Body);
    }

    void SemanticAnalyzer::HandleIfStmt(Stmt* stmt) {
        IfStmt ifs = stmt->If;

        HandleExpr(ifs.Condition);
        HandleStmt(ifs.Body);
        if (ifs.ElseBody) { HandleStmt(ifs.ElseBody); }
    }

    void SemanticAnalyzer::HandleBreakStmt(Stmt* stmt) {}
    void SemanticAnalyzer::HandleContinueStmt(Stmt* stmt) {}

    void SemanticAnalyzer::HandleReturnStmt(Stmt* stmt) {
        ReturnStmt ret = stmt->Return;
        if (ret.Value) { HandleExpr(ret.Value); }
    }

    void SemanticAnalyzer::HandleStmt(Stmt* stmt) {
        if (stmt->Kind == StmtKind::Error) { return; }
        else if (stmt->Kind == StmtKind::Nop) { return; }
        else if (stmt->Kind == StmtKind::Block) {
            return HandleBlockStmt(stmt);
        } else if (stmt->Kind == StmtKind::While) {
            return HandleWhileStmt(stmt);
        } else if (stmt->Kind == StmtKind::DoWhile) {
            return HandleDoWhileStmt(stmt);
        } else if (stmt->Kind == StmtKind::For) {
            return HandleForStmt(stmt);
        } else if (stmt->Kind == StmtKind::If) {
            return HandleIfStmt(stmt);
        } else if (stmt->Kind == StmtKind::Break) {
            return HandleBreakStmt(stmt);
        } else if (stmt->Kind == StmtKind::Continue) {
            return HandleContinueStmt(stmt);
        } else if (stmt->Kind == StmtKind::Return) {
            return HandleReturnStmt(stmt);
        } else if (stmt->Kind == StmtKind::Expr) {
            return HandleExpr(stmt->ExprStmt);
        } else if (stmt->Kind == StmtKind::Decl) {
            return HandleDecl(stmt->DeclStmt);
        }

        ARIA_UNREACHABLE();
    }

} // namespace Aria::Internal
