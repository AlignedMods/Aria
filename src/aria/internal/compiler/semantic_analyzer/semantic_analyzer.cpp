#include "aria/internal/compiler/semantic_analyzer/semantic_analyzer.hpp"
#include "aria/internal/compiler/ast/ast.hpp"

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

    TypeInfo* SemanticAnalyzer::HandleBooleanConstantExpr(Expr* expr) { return expr->GetResolvedType(); }
    TypeInfo* SemanticAnalyzer::HandleCharacterConstantExpr(Expr* expr) { return expr->GetResolvedType(); }
    TypeInfo* SemanticAnalyzer::HandleIntegerConstantExpr(Expr* expr) { return expr->GetResolvedType(); }
    TypeInfo* SemanticAnalyzer::HandleFloatingConstantExpr(Expr* expr) { return expr->GetResolvedType(); }
    TypeInfo* SemanticAnalyzer::HandleStringConstantExpr(Expr* expr) { return expr->GetResolvedType(); }
    TypeInfo* SemanticAnalyzer::HandleDeclRefExpr(Expr* expr) { return expr->GetResolvedType(); }
    TypeInfo* SemanticAnalyzer::HandleMemberExpr(Expr* expr) { return expr->GetResolvedType(); }
    TypeInfo* SemanticAnalyzer::HandleCallExpr(Expr* expr) { return expr->GetResolvedType(); }
    TypeInfo* SemanticAnalyzer::HandleMethodCallExpr(Expr* expr) { return expr->GetResolvedType(); }
    TypeInfo* SemanticAnalyzer::HandleParenExpr(Expr* expr) { return expr->GetResolvedType(); }
    TypeInfo* SemanticAnalyzer::HandleCastExpr(Expr* expr) { return expr->GetResolvedType(); }
    TypeInfo* SemanticAnalyzer::HandleImplicitCastExpr(Expr* expr) { return expr->GetResolvedType(); }
    TypeInfo* SemanticAnalyzer::HandleUnaryOperatorExpr(Expr* expr) { return expr->GetResolvedType(); }
    TypeInfo* SemanticAnalyzer::HandleBinaryOperatorExpr(Expr* expr) { return expr->GetResolvedType(); }
    TypeInfo* SemanticAnalyzer::HandleCompoundAssignExpr(Expr* expr) { return expr->GetResolvedType(); }

    TypeInfo* SemanticAnalyzer::HandleExpr(Expr* expr) {
        if (GetNode<BooleanConstantExpr>(expr)) {
            return HandleBooleanConstantExpr(expr);
        } else if (GetNode<CharacterConstantExpr>(expr)) {
            return HandleCharacterConstantExpr(expr);
        } else if (GetNode<IntegerConstantExpr>(expr)) {
            return HandleIntegerConstantExpr(expr);
        } else if (GetNode<FloatingConstantExpr>(expr)) {
            return HandleFloatingConstantExpr(expr);
        } else if (GetNode<StringConstantExpr>(expr)) {
            return HandleStringConstantExpr(expr);
        } else if (GetNode<DeclRefExpr>(expr)) {
            return HandleDeclRefExpr(expr);
        } else if (GetNode<MemberExpr>(expr)) {
            return HandleMemberExpr(expr);
        } else if (GetNode<CallExpr>(expr)) {
            return HandleCallExpr(expr);
        } else if (GetNode<MethodCallExpr>(expr)) {
            return HandleMethodCallExpr(expr);
        } else if (GetNode<ParenExpr>(expr)) {
            return HandleParenExpr(expr);
        } else if (GetNode<CastExpr>(expr)) {
            return HandleCastExpr(expr);
        } else if (GetNode<ImplicitCastExpr>(expr)) {
            return HandleImplicitCastExpr(expr);
        } else if (GetNode<UnaryOperatorExpr>(expr)) {
            return HandleUnaryOperatorExpr(expr);
        } else if (GetNode<BinaryOperatorExpr>(expr)) {
            return HandleBinaryOperatorExpr(expr);
        } else if (GetNode<CompoundAssignExpr>(expr)) {
            return HandleCompoundAssignExpr(expr);
        }

        ARIA_UNREACHABLE();
    }

    void SemanticAnalyzer::HandleTranslationUnitDecl(Decl* decl) {
        TranslationUnitDecl* tu = GetNode<TranslationUnitDecl>(decl);

        for (Stmt* stmt : tu->GetStmts()) {
            HandleStmt(stmt);
        }
    }

    void SemanticAnalyzer::HandleVarDecl(Decl* decl) {
        VarDecl* varDecl = GetNode<VarDecl>(decl);

        auto& map = GetActiveDeclMap();

        VariableDeclaration d;
        d.Type = varDecl->GetResolvedType();
        d.SourceDecl = varDecl;

        map[varDecl->GetIdentifier()] = d;
    }

    void SemanticAnalyzer::HandleParamDecl(Decl* decl) {
        ParamDecl* paramDecl = GetNode<ParamDecl>(decl);

        auto& map = GetActiveDeclMap();

        ParameterDeclaration d;
        d.Type = paramDecl->GetResolvedType();
        d.SourceDecl = paramDecl;

        map[paramDecl->GetIdentifier()] = d;
    }

    void SemanticAnalyzer::HandleFunctionDecl(Decl* decl) {
        FunctionDecl* fnDecl = GetNode<FunctionDecl>(decl);

        if (fnDecl->GetBody()) {
            PushScope();

            for (ParamDecl* p : fnDecl->GetParameters()) {
                HandleParamDecl(p);
            }

            HandleCompoundStmt(fnDecl->GetBody());

            PopScope();
        }
    }

    void SemanticAnalyzer::HandleStructDecl(Decl* decl) {}

    void SemanticAnalyzer::HandleDecl(Decl* decl) {
        if (GetNode<TranslationUnitDecl>(decl)) {
            return HandleTranslationUnitDecl(decl);
        } else if (GetNode<VarDecl>(decl)) {
            return HandleVarDecl(decl);
        } else if (GetNode<ParamDecl>(decl)) {
            return HandleParamDecl(decl);
        } else if (GetNode<FunctionDecl>(decl)) {
            return HandleFunctionDecl(decl);
        } else if (GetNode<StructDecl>(decl)) {
            return HandleStructDecl(decl);
        }

        ARIA_UNREACHABLE();
    }

    void SemanticAnalyzer::HandleCompoundStmt(Stmt* stmt) {
        CompoundStmt* compound = GetNode<CompoundStmt>(stmt);

        for (Stmt* s : compound->GetStmts()) {
            HandleStmt(s);
        }
    }

    void SemanticAnalyzer::HandleWhileStmt(Stmt* stmt) {
        WhileStmt* wh = GetNode<WhileStmt>(stmt);

        HandleExpr(wh->GetCondition());
        HandleStmt(wh->GetBody());
    }

    void SemanticAnalyzer::HandleDoWhileStmt(Stmt* stmt) {
        DoWhileStmt* wh = GetNode<DoWhileStmt>(stmt);

        HandleExpr(wh->GetCondition());
        HandleStmt(wh->GetBody());
    }

    void SemanticAnalyzer::HandleForStmt(Stmt* stmt) {
        ARIA_ASSERT(false, "todo");
    }

    void SemanticAnalyzer::HandleIfStmt(Stmt* stmt) {}

    void SemanticAnalyzer::HandleReturnStmt(Stmt* stmt) {}

    void SemanticAnalyzer::HandleStmt(Stmt* stmt) {
        if (GetNode<CompoundStmt>(stmt)) {
            PushScope();
            HandleCompoundStmt(stmt);
            PopScope();
            return;
        } else if (GetNode<WhileStmt>(stmt)) {
            HandleWhileStmt(stmt);
            return;
        } else if (GetNode<DoWhileStmt>(stmt)) {
            HandleDoWhileStmt(stmt);
            return;
        } else if (GetNode<ForStmt>(stmt)) {
            HandleForStmt(stmt);
            return;
        } else if (GetNode<IfStmt>(stmt)) {
            HandleIfStmt(stmt);
            return;
        } else if (GetNode<ReturnStmt>(stmt)) {
            HandleReturnStmt(stmt);
            return;
        } else if (Expr* expr = GetNode<Expr>(stmt)) {
            HandleExpr(expr);
            return;
        } else if (Decl* decl = GetNode<Decl>(stmt)) {
            HandleDecl(decl);
            return;
        }

        ARIA_UNREACHABLE();
    }

    void SemanticAnalyzer::PushScope() {
        m_Scopes.emplace_back();
    }

    void SemanticAnalyzer::PopScope() {
        ARIA_ASSERT(m_Scopes.size() > 0, "SemanticAnalyzer::PopScope() called with no active scopes");
        m_Scopes.pop_back();
    }

    std::unordered_map<std::string, SemanticAnalyzer::Declaration>& SemanticAnalyzer::GetActiveDeclMap() {
        if (m_Scopes.size() == 0) {
            return m_GlobalDeclaredSymbols;
        }

        return m_Scopes.back().DeclaredSymbols;
    }

} // namespace Aria::Internal
