#include "internal/compiler/semantic_analyzer/semantic_analyzer.hpp"

namespace BlackLua::Internal {

    SemanticAnalyzer::SemanticAnalyzer(ASTNodes* nodes, Context* ctx) : m_TypeChecker(ctx, nodes) {
        m_Nodes = nodes;
        m_Context = ctx;

        m_TypeChecker.CheckImpl();
        AnalyzeImpl();
    }

    void SemanticAnalyzer::AnalyzeImpl() {
        while (Peek()) {
            HandleNode(Consume());
        }
    }

    Node* SemanticAnalyzer::Peek(size_t count) {
        if (m_Index + count < m_Nodes->size()) {
            return m_Nodes->at(m_Index + count);
        } else {
            return nullptr;
        }
    }

    Node* SemanticAnalyzer::Consume() {
        BLUA_ASSERT(m_Index < m_Nodes->size(), "SemanticAnalyzer::Consume() of out bounds!");
        return m_Nodes->at(m_Index++);
    }

    TypeInfo* SemanticAnalyzer::HandleExprConstant(NodeExpr* expr) {
        ExprConstant* con = GetNode<ExprConstant>(expr);
        return con->ResolvedType;
    }

    TypeInfo* SemanticAnalyzer::HandleExprVarRef(NodeExpr* expr) {
        ExprVarRef* ref = GetNode<ExprVarRef>(expr);
        return ref->ResolvedType;
    }

    TypeInfo* SemanticAnalyzer::HandleExprArrayAccess(NodeExpr* expr) {
        BLUA_ASSERT(false, "todo");
    }

    TypeInfo* SemanticAnalyzer::HandleExprSelf(NodeExpr* expr) {
        BLUA_ASSERT(false, "todo");
    }

    TypeInfo* SemanticAnalyzer::HandleExprMember(NodeExpr* expr) {
        BLUA_ASSERT(false, "todo");
    }

    TypeInfo* SemanticAnalyzer::HandleExprMethodCall(NodeExpr* expr) {
        BLUA_ASSERT(false, "todo");
    }

    TypeInfo* SemanticAnalyzer::HandleExprCall(NodeExpr* expr) {
        BLUA_ASSERT(false, "todo");
    }

    TypeInfo* SemanticAnalyzer::HandleExprParen(NodeExpr* expr) {
        ExprParen* paren = GetNode<ExprParen>(expr);
        return HandleNodeExpression(paren->Expression);
    }

    TypeInfo* SemanticAnalyzer::HandleExprCast(NodeExpr* expr) {
        BLUA_ASSERT(false, "todo");
    }

    TypeInfo* SemanticAnalyzer::HandleExprUnaryOperator(NodeExpr* expr) {
        ExprUnaryOperator* unop = GetNode<ExprUnaryOperator>(expr);

        TypeInfo* type = HandleNodeExpression(unop->Expression);
        unop->ResolvedType = type;
        return type;
    }

    TypeInfo* SemanticAnalyzer::HandleExprBinaryOperator(NodeExpr* expr) {
        ExprBinaryOperator* binop = GetNode<ExprBinaryOperator>(expr);
        return binop->ResolvedType;
    }

    TypeInfo* SemanticAnalyzer::HandleNodeExpression(NodeExpr* expr) {
        if (GetNode<ExprConstant>(expr)) {
            return HandleExprConstant(expr);
        } else if (GetNode<ExprVarRef>(expr)) {
            return HandleExprVarRef(expr);
        } else if (GetNode<ExprArrayAccess>(expr)) {
            return HandleExprArrayAccess(expr);
        } else if (GetNode<ExprMember>(expr)) {
            return HandleExprMember(expr);
        } else if (GetNode<ExprMethodCall>(expr)) {
            return HandleExprMethodCall(expr);
        } else if (GetNode<ExprCall>(expr)) {
            return HandleExprCall(expr);
        } else if (GetNode<ExprParen>(expr)) {
            return HandleExprParen(expr);
        } else if (GetNode<ExprCast>(expr)) {
            return HandleExprCast(expr);
        } else if (GetNode<ExprUnaryOperator>(expr)) {
            return HandleExprUnaryOperator(expr);
        } else if (GetNode<ExprBinaryOperator>(expr)) {
            return HandleExprBinaryOperator(expr);
        }

        BLUA_UNREACHABLE();
    }

    void SemanticAnalyzer::HandleStmtCompound(NodeStmt* stmt) {
        StmtCompound* compound = GetNode<StmtCompound>(stmt);

        for (size_t i = 0; i < compound->Nodes.Size; i++) {
            HandleNode(compound->Nodes.Items[i]);
        }
    }

    void SemanticAnalyzer::HandleStmtVarDecl(NodeStmt* stmt) {
        StmtVarDecl* decl = GetNode<StmtVarDecl>(stmt);

        std::string ident = fmt::format("{}", decl->Identifier);
        auto& symbolTable = (m_Scopes.size() > 0) ? m_Scopes.back().DeclaredSymbols : m_DeclaredSymbols;
        
        // We do not allow redeclarations in the same scope
        if (symbolTable.contains(ident)) {
            m_Context->ReportCompilerError(stmt->Loc.Line, stmt->Loc.Column, 
                                           stmt->Range.Start.Line, stmt->Range.Start.Column,
                                           stmt->Range.End.Line, stmt->Range.End.Column,
                                           fmt::format("Redeclaring variable {}", ident));

            return;
        }

        Declaration d;
        d.Decl = stmt;
        d.Extern = false;
        d.Type = decl->ResolvedType;

        symbolTable[ident] = d;
    }

    void SemanticAnalyzer::HandleStmtParamDecl(NodeStmt* stmt) {
        BLUA_ASSERT(false, "todo");
    }

    void SemanticAnalyzer::HandleStmtFunctionDecl(NodeStmt* stmt) {
        StmtFunctionDecl* decl = GetNode<StmtFunctionDecl>(stmt);

        std::string ident = fmt::format("{}", decl->Name);

        if (decl->Extern && decl->Body) {
            m_Context->ReportCompilerError(stmt->Loc.Line, stmt->Loc.Column, 
                                           stmt->Range.Start.Line, stmt->Range.Start.Column,
                                           stmt->Range.End.Line, stmt->Range.End.Column,
                                           "Defining a body for a function marked \"extern\" {}");

            return;
        }

        // If we have already defined a function with this name, there are some extra checks we perform
        if (m_DeclaredSymbols.contains(ident)) {
            Declaration d = m_DeclaredSymbols.at(ident);

            if (d.Extern != decl->Extern) {
                m_Context->ReportCompilerError(stmt->Loc.Line, stmt->Loc.Column, 
                                               stmt->Range.Start.Line, stmt->Range.Start.Column,
                                               stmt->Range.End.Line, stmt->Range.End.Column,
                                               "Redefining function with a different extern-ness");

                return;
            }

            // Check if previous declarations match
            if (!GetNode<StmtFunctionDecl>(d.Decl)) {
                m_Context->ReportCompilerError(stmt->Loc.Line, stmt->Loc.Column, 
                                               stmt->Range.Start.Line, stmt->Range.Start.Column,
                                               stmt->Range.End.Line, stmt->Range.End.Column,
                                               "Redefining function as a different type");

                return;
            }
        }

        if (decl->Body) {
            PushScope();
            HandleStmtCompound(decl->Body);
            PopScope();
        }

        Declaration d;
        d.Decl = stmt;
        d.Extern = decl->Extern;
        d.Type = decl->ResolvedType;
        m_DeclaredSymbols[ident] = d;
    }

    void SemanticAnalyzer::HandleStmtStructDecl(NodeStmt* stmt) {
        BLUA_ASSERT(false, "todo");
    }

    void SemanticAnalyzer::HandleStmtFieldDecl(NodeStmt* stmt) {
        BLUA_ASSERT(false, "todo");
    }

    void SemanticAnalyzer::HandleStmtMethodDecl(NodeStmt* stmt) {
        BLUA_ASSERT(false, "todo");
    }

    void SemanticAnalyzer::HandleStmtWhile(NodeStmt* stmt) {
        BLUA_ASSERT(false, "todo");
    }

    void SemanticAnalyzer::HandleStmtDoWhile(NodeStmt* stmt) {
        BLUA_ASSERT(false, "todo");
    }

    void SemanticAnalyzer::HandleStmtFor(NodeStmt* stmt) {
        BLUA_ASSERT(false, "todo");
    }

    void SemanticAnalyzer::HandleStmtIf(NodeStmt* stmt) {
        BLUA_ASSERT(false, "todo");
    }

    void SemanticAnalyzer::HandleStmtReturn(NodeStmt* stmt) {
        BLUA_ASSERT(false, "todo");
    }

    void SemanticAnalyzer::HandleNodeStatement(NodeStmt* stmt) {
        if (GetNode<StmtCompound>(stmt)) {
            HandleStmtCompound(stmt);
            return;
        } else if (GetNode<StmtVarDecl>(stmt)) {
            HandleStmtVarDecl(stmt);
            return;
        } else if (GetNode<StmtParamDecl>(stmt)) {
            HandleStmtParamDecl(stmt);
            return;
        } else if (GetNode<StmtFunctionDecl>(stmt)) {
            HandleStmtFunctionDecl(stmt);
            return;
        } else if (GetNode<StmtStructDecl>(stmt)) {
            HandleStmtStructDecl(stmt);
            return;
        } else if (GetNode<StmtFieldDecl>(stmt)) {
            HandleStmtFieldDecl(stmt);
            return;
        } else if (GetNode<StmtMethodDecl>(stmt)) {
            HandleStmtMethodDecl(stmt);
            return;
        } else if (GetNode<StmtWhile>(stmt)) {
            HandleStmtWhile(stmt);
            return;
        } else if (GetNode<StmtDoWhile>(stmt)) {
            HandleStmtDoWhile(stmt);
            return;
        } else if (GetNode<StmtFor>(stmt)) {
            HandleStmtFor(stmt);
            return;
        } else if (GetNode<StmtReturn>(stmt)) {
            HandleStmtReturn(stmt);
            return;
        }

        BLUA_UNREACHABLE();
    }

    void SemanticAnalyzer::HandleNode(Node* node) {
        if (NodeExpr* expr = GetNode<NodeExpr>(node)) {
            HandleNodeExpression(expr);
            return;
        } else if (NodeStmt* stmt = GetNode<NodeStmt>(node)) {
            HandleNodeStatement(stmt);
            return;
        }

        BLUA_UNREACHABLE();
    }

    void SemanticAnalyzer::PushScope(TypeInfo* returnType) {
        Scope scope;
        if (returnType) {
            scope.ReturnType = returnType;
        } else {
            if (m_Scopes.size() > 0) {
                scope.ReturnType = m_Scopes[m_Scopes.size() - 1].ReturnType;
            }
        }

        m_Scopes.push_back(scope);
    }

    void SemanticAnalyzer::PopScope() {
        BLUA_ASSERT(m_Scopes.size() > 0, "SemanticAnalyzer::PopScope() called with no active scopes");
        m_Scopes.pop_back();
    }

} // namespace BlackLua::Internal
