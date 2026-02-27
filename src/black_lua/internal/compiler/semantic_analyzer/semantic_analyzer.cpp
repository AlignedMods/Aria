#include "internal/compiler/semantic_analyzer/semantic_analyzer.hpp"

namespace BlackLua::Internal {

    SemanticAnalyzer::SemanticAnalyzer(ASTNodes* nodes, Context* ctx) : m_TypeChecker(ctx) {
        m_Nodes = nodes;
        m_Context = ctx;

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

    VariableType* SemanticAnalyzer::HandleExprConstant(NodeExpr* expr) {
        ExprConstant* con = GetNode<ExprConstant>(expr);

        VariableType* type = nullptr;

        if (GetNode<ConstantBool>(con)) {
            type = CreateVarType(m_Context, PrimitiveType::Bool, false, true);
        } else if (GetNode<ConstantChar>(con)) {
            type = CreateVarType(m_Context, PrimitiveType::Char, false);
        } else if (ConstantInt* ci = GetNode<ConstantInt>(con)) {
            type = CreateVarType(m_Context, PrimitiveType::Int, false, !ci->Unsigned);
        } else if (ConstantLong* cl = GetNode<ConstantLong>(con)) {
            type = CreateVarType(m_Context, PrimitiveType::Long, false, !cl->Unsigned);
        } else if (GetNode<ConstantFloat>(con)) {
            type = CreateVarType(m_Context, PrimitiveType::Float, false);
        } else if (GetNode<ConstantDouble>(con)) {
            type = CreateVarType(m_Context, PrimitiveType::Double, false);
        } else if (GetNode<ConstantString>(con)) {
            type = CreateVarType(m_Context, PrimitiveType::String, false);
        }
        
        con->ResolvedType = type;
        
        return type;
    }

    VariableType* SemanticAnalyzer::HandleExprVarRef(NodeExpr* expr) {
        ExprVarRef* ref = GetNode<ExprVarRef>(expr);

        std::string ident = fmt::format("{}", ref->Identifier);

        // Check whether this variable could be coming from "self"
        if (m_ActiveStruct) {
            for (const auto& field : m_ActiveStruct->Fields) {
                if (field.Identifier == ref->Identifier) {
                    // We have a match
                    ExprMember* memberExpr = Allocate<ExprMember>();
                    memberExpr->Member = ref->Identifier;
                    memberExpr->Parent = Allocate<NodeExpr>(Allocate<ExprSelf>(), expr->Range, expr->Loc);
                    memberExpr->ResolvedMemberType = field.ResolvedType;
                    memberExpr->ResolvedParentType = CreateVarType(m_Context, PrimitiveType::Structure, true, *m_ActiveStruct);

                    BLUA_ASSERT(false, "todo");

                    expr->Data = memberExpr;
                    return memberExpr->ResolvedMemberType;
                }
            }
        }

        // Next we check all the active scopes
        {
            ptrdiff_t currentScopeIndex = static_cast<ptrdiff_t>(m_Scopes.size() - 1);

            while (currentScopeIndex > 0) {
                if (m_Scopes[static_cast<size_t>(currentScopeIndex)].DeclaredSymbols.contains(ident)) {
                    Declaration decl = m_Scopes[static_cast<size_t>(currentScopeIndex)].DeclaredSymbols.at(ident);

                    ref->ResolvedType = CreateVarType(m_Context, decl.Type->Type, true, decl.Type->Data);
                    return ref->ResolvedType;
                }

                currentScopeIndex--;
            }
        }

        // Lastly we check the global variables
        if (m_DeclaredSymbols.contains(ident)) {
            Declaration decl = m_DeclaredSymbols.at(ident);

            ref->ResolvedType = CreateVarType(m_Context, decl.Type->Type, true, decl.Type->Data);
            return ref->ResolvedType;
        }

        // If all else fails, there is no such variable declared anywhere
        m_Context->ReportCompilerError(expr->Loc.Line, expr->Loc.Column, 
                                       expr->Range.Start.Line, expr->Range.Start.Column,
                                       expr->Range.End.Line, expr->Range.End.Column,
                                       fmt::format("Undeclared identifier {}", ident));
        return nullptr;
    }

    VariableType* SemanticAnalyzer::HandleExprArrayAccess(NodeExpr* expr) {
        BLUA_ASSERT(false, "todo");
    }

    VariableType* SemanticAnalyzer::HandleExprSelf(NodeExpr* expr) {
        BLUA_ASSERT(false, "todo");
    }

    VariableType* SemanticAnalyzer::HandleExprMember(NodeExpr* expr) {
        BLUA_ASSERT(false, "todo");
    }

    VariableType* SemanticAnalyzer::HandleExprMethodCall(NodeExpr* expr) {
        BLUA_ASSERT(false, "todo");
    }

    VariableType* SemanticAnalyzer::HandleExprCall(NodeExpr* expr) {
        ExprCall* call = GetNode<ExprCall>(expr);

        std::string ident = fmt::format("{}", call->Name);

        if (!m_DeclaredSymbols.contains(ident)) {
            m_Context->ReportCompilerError(expr->Loc.Line, expr->Loc.Column, 
                                           expr->Range.Start.Line, expr->Range.Start.Column,
                                           expr->Range.End.Line, expr->Range.End.Column,
                                           fmt::format("Calling undeclared identifier {}", ident));

            return nullptr;
        }

        Declaration decl = m_DeclaredSymbols.at(ident);

        call->Extern = decl.Extern;
        call->ResolvedReturnType = decl.Type;
        return call->ResolvedReturnType;
    }

    VariableType* SemanticAnalyzer::HandleExprParen(NodeExpr* expr) {
        ExprParen* paren = GetNode<ExprParen>(expr);
        return HandleNodeExpression(paren->Expression);
    }

    VariableType* SemanticAnalyzer::HandleExprCast(NodeExpr* expr) {
        BLUA_ASSERT(false, "todo");
    }

    VariableType* SemanticAnalyzer::HandleExprUnaryOperator(NodeExpr* expr) {
        ExprUnaryOperator* unop = GetNode<ExprUnaryOperator>(expr);

        VariableType* type = HandleNodeExpression(unop->Expression);
        unop->ResolvedType = type;
        return type;
    }

    VariableType* SemanticAnalyzer::HandleExprBinaryOperator(NodeExpr* expr) {
        ExprBinaryOperator* binop = GetNode<ExprBinaryOperator>(expr);
        VariableType* LHSType = HandleNodeExpression(binop->LHS);
        VariableType* RHSType = HandleNodeExpression(binop->RHS);

        // First we transform the AST with any needed implicit casts
        if (binop->Type == BinaryOperatorType::Eq) {
            LHSType = m_TypeChecker.RequireLValue(LHSType, binop->LHS);
            RHSType = m_TypeChecker.RequireRValue(RHSType, binop->RHS);
        } else {
            LHSType = m_TypeChecker.RequireRValue(LHSType, binop->LHS);
            RHSType = m_TypeChecker.RequireRValue(RHSType, binop->RHS);
        }

        m_TypeChecker.InsertImplicitCast(LHSType, RHSType, binop->LHS, binop->RHS);

        switch (binop->Type) {
            case BinaryOperatorType::Add:     case BinaryOperatorType::AddInPlace:
            case BinaryOperatorType::Sub:     case BinaryOperatorType::SubInPlace:
            case BinaryOperatorType::Mul:     case BinaryOperatorType::MulInPlace:
            case BinaryOperatorType::Div:     case BinaryOperatorType::DivInPlace:
            case BinaryOperatorType::Mod:     case BinaryOperatorType::ModInPlace:
            case BinaryOperatorType::And:     case BinaryOperatorType::AndInPlace:
            case BinaryOperatorType::Or:      case BinaryOperatorType::OrInPlace:
            case BinaryOperatorType::Xor:     case BinaryOperatorType::XorInPlace: {
                binop->ResolvedType = LHSType;
                binop->ResolvedSourceType = LHSType;
                break;
            }

            case BinaryOperatorType::IsEq:    case BinaryOperatorType::IsNotEq:
            case BinaryOperatorType::Less:    case BinaryOperatorType::LessOrEq:
            case BinaryOperatorType::Greater: case BinaryOperatorType::GreaterOrEq:
            case BinaryOperatorType::BitAnd:
            case BinaryOperatorType::BitOr: {
                binop->ResolvedType = CreateVarType(m_Context, PrimitiveType::Bool, false, true);
                binop->ResolvedSourceType = LHSType;
                break;
            }

            case BinaryOperatorType::Eq: {
                binop->ResolvedType = LHSType;
                binop->ResolvedSourceType = RHSType;
                break;
            }
        }

        return binop->ResolvedType;
    }

    VariableType* SemanticAnalyzer::HandleNodeExpression(NodeExpr* expr) {
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

        VariableType* type = m_TypeChecker.GetVariableTypeFromString(StringView(decl->Type.Data(), decl->Type.Size()));
        decl->ResolvedType = type;

        Declaration d;
        d.Decl = stmt;
        d.Extern = false;
        d.Type = type;

        symbolTable[ident] = d;

        // Handle potential initilization
        if (decl->Value) {
            VariableType* valType = HandleNodeExpression(decl->Value);
            valType = m_TypeChecker.RequireRValue(valType, decl->Value);
            m_TypeChecker.InsertImplicitCast(d.Type, valType, nullptr, decl->Value);
        }
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
        d.Type = m_TypeChecker.GetVariableTypeFromString(StringView(decl->ReturnType.Data(), decl->ReturnType.Size()));
        m_DeclaredSymbols[ident] = d;

        decl->ResolvedType = d.Type;
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

    void SemanticAnalyzer::PushScope(VariableType* returnType) {
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