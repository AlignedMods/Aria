#include "aria/internal/compiler/ast/ast_dumper.hpp"

#include "fmt/std.h"

namespace Aria::Internal {

    ASTDumper::ASTDumper(Stmt* rootASTNode) {
        m_RootASTNode = rootASTNode;

        DumpASTImpl();
    }

    std::string& ASTDumper::GetOutput() {
        return m_Output;
    }

    void ASTDumper::DumpASTImpl() {
        DumpStmt(m_RootASTNode, 0);
    }

    void ASTDumper::DumpExpr(Expr* expr, size_t indentation) {
        std::string ident;
        ident.append(indentation, ' ');
        m_Output += ident;

        if (expr == nullptr) { m_Output += "<<NULL>>\n"; return; };
        
        if (BooleanConstantExpr* bc = GetNode<BooleanConstantExpr>(expr)) {
            m_Output += fmt::format("BooleanConstantExpr {} '{}' {}\n", bc->GetValue(), TypeInfoToString(bc->GetResolvedType()), ExprValueKindToString(bc->GetValueKind())); return;
        } else if (CharacterConstantExpr* cc = GetNode<CharacterConstantExpr>(expr)) {
            m_Output += fmt::format("BooleanConstantExpr '{}' '{}' {}\n", cc->GetValue(), TypeInfoToString(cc->GetResolvedType()), ExprValueKindToString(cc->GetValueKind())); return;
        } else if (IntegerConstantExpr* ic = GetNode<IntegerConstantExpr>(expr)) {
            m_Output += fmt::format("IntegerConstantExpr {} '{}' {}\n", ic->GetValue(), TypeInfoToString(ic->GetResolvedType()), ExprValueKindToString(ic->GetValueKind())); return;
        } else if (FloatingConstantExpr* fc = GetNode<FloatingConstantExpr>(expr)) {
            m_Output += fmt::format("FloatingConstantExpr {} '{}' {}\n", fc->GetValue(), TypeInfoToString(fc->GetResolvedType()), ExprValueKindToString(fc->GetValueKind())); return;
        } else if (StringConstantExpr* sc = GetNode<StringConstantExpr>(expr)) {
            m_Output += fmt::format("StringConstantExpr \"{}\" '{}' {}\n", sc->GetValue(), TypeInfoToString(sc->GetResolvedType()), ExprValueKindToString(sc->GetValueKind())); return;
        } else if (DeclRefExpr* declRef = GetNode<DeclRefExpr>(expr)) {
            m_Output += fmt::format("DeclRefExpr '{}' '{}' {} {}\n", declRef->GetRawIdentifier(), TypeInfoToString(declRef->GetResolvedType()), DeclRefKindToString(declRef->GetKind()), ExprValueKindToString(declRef->GetValueKind())); return;
        } else if (MemberExpr* mem = GetNode<MemberExpr>(expr)) {
            m_Output += fmt::format("MemberExpr '{}' '{}' {}\n", mem->GetMember(), TypeInfoToString(mem->GetResolvedType()), ExprValueKindToString(mem->GetValueKind()));
            DumpExpr(mem->GetParent(), indentation + 4);
            return;
        } else if (SelfExpr* self = GetNode<SelfExpr>(expr)) {
            m_Output += fmt::format("SelfExpr '{}' {}\n", TypeInfoToString(self->GetResolvedType()), ExprValueKindToString(expr->GetValueKind()));
            return;
        } else if (TemporaryExpr* tempExpr = GetNode<TemporaryExpr>(expr)) {
            m_Output += fmt::format("TemporaryExpr '{}' {}\n", TypeInfoToString(tempExpr->GetResolvedType()), ExprValueKindToString(expr->GetValueKind()));
            DumpExpr(tempExpr->GetExpression(), indentation + 4);
            return;
        } else if (CallExpr* call = GetNode<CallExpr>(expr)) {
            m_Output += fmt::format("CallExpr '{}' {}\n", TypeInfoToString(call->GetResolvedType()), ExprValueKindToString(call->GetValueKind()));
            for (Expr* e : call->GetArguments()) {
                DumpExpr(e, indentation + 4);
            }
            DumpExpr(call->GetCallee(), indentation + 4);
            return;
        } else if (MethodCallExpr* mcall = GetNode<MethodCallExpr>(expr)) {
            m_Output += fmt::format("MethodCallExpr '{}' {}\n", TypeInfoToString(mcall->GetResolvedType()), ExprValueKindToString(mcall->GetValueKind()));
            for (Expr* e : mcall->GetArguments()) {
                DumpExpr(e, indentation + 4);
            }
            DumpExpr(mcall->GetCallee(), indentation + 4);
            return;
        } else if (ParenExpr* paren = GetNode<ParenExpr>(expr)) {
            m_Output += fmt::format("ParenExpr '{}' {}\n", TypeInfoToString(paren->GetResolvedType()), ExprValueKindToString(paren->GetValueKind()));
            DumpExpr(paren->GetChildExpr(), indentation + 4);
            return;
        } else if (CastExpr* cast = GetNode<CastExpr>(expr)) {
            m_Output += fmt::format("CastExpr '{}' <{}> {}\n", TypeInfoToString(cast->GetResolvedType()), CastKindToString(cast->GetCastKind()), ExprValueKindToString(cast->GetValueKind()));
            DumpExpr(cast->GetChildExpr(), indentation + 4);
            return;
        } else if (ImplicitCastExpr* icast = GetNode<ImplicitCastExpr>(expr)) {
            m_Output += fmt::format("ImplicitCastExpr '{}' <{}> {}\n", TypeInfoToString(icast->GetResolvedType()), CastKindToString(icast->GetCastKind()), ExprValueKindToString(icast->GetValueKind()));
            DumpExpr(icast->GetChildExpr(), indentation + 4);
            return;
        } else if (UnaryOperatorExpr* unOp = GetNode<UnaryOperatorExpr>(expr)) {
            m_Output += fmt::format("UnaryOperatorExpr '{}' '{}' {}\n", UnaryOperatorKindToString(unOp->GetUnaryOperator()), TypeInfoToString(unOp->GetResolvedType()), ExprValueKindToString(unOp->GetValueKind()));
            DumpExpr(unOp->GetChildExpr(), indentation + 4);
            return;
        } else if (BinaryOperatorExpr* binOp = GetNode<BinaryOperatorExpr>(expr)) {
            m_Output += fmt::format("BinaryOperatorExpr '{}' '{}' {}\n", BinaryOperatorKindToString(binOp->GetBinaryOperator()), TypeInfoToString(binOp->GetResolvedType()), ExprValueKindToString(binOp->GetValueKind()));
            DumpExpr(binOp->GetLHS(), indentation + 4);
            DumpExpr(binOp->GetRHS(), indentation + 4);
            return;
        } else if (CompoundAssignExpr* compAss = GetNode<CompoundAssignExpr>(expr)) {
            m_Output += fmt::format("CompoundAssignExpr '{}' '{}' {}\n", BinaryOperatorKindToString(compAss->GetBinaryOperator()), TypeInfoToString(compAss->GetResolvedType()), ExprValueKindToString(compAss->GetValueKind()));
            DumpExpr(compAss->GetLHS(), indentation + 4);
            DumpExpr(compAss->GetRHS(), indentation + 4);
            return;
        }

        ARIA_UNREACHABLE();
    }

    void ASTDumper::DumpDecl(Decl* decl, size_t indentation) {
        std::string ident;
        ident.append(indentation, ' ');
        m_Output += ident;

        if (decl == nullptr) { m_Output += "<<NULL>>\n"; return; };

        if (TranslationUnitDecl* tuDecl = GetNode<TranslationUnitDecl>(decl)) {
            m_Output += "TranslationUnitDecl\n";

            for (Stmt* stmt : tuDecl->GetStmts()) {
                DumpStmt(stmt, indentation + 4);
            }
            return;
        } else if (VarDecl* varDecl = GetNode<VarDecl>(decl)) {
            m_Output += fmt::format("VarDecl '{}' '{}'\n", varDecl->GetRawIdentifier(), TypeInfoToString(varDecl->GetResolvedType()));
            if (varDecl->GetDefaultValue()) {
                DumpExpr(varDecl->GetDefaultValue(), indentation + 4);
            }
            return;
        } else if (ParamDecl* paramDecl = GetNode<ParamDecl>(decl)) {
            m_Output += fmt::format("ParamDecl '{}' '{}'\n", paramDecl->GetRawIdentifier(), TypeInfoToString(paramDecl->GetResolvedType()));
            return;
        } else if (FunctionDecl* fnDecl = GetNode<FunctionDecl>(decl)) {
            m_Output += fmt::format("FunctionDecl '{}' '{}' {}\n", fnDecl->GetRawIdentifier(), TypeInfoToString(fnDecl->GetResolvedType()), fnDecl->IsExtern() ? "extern" : "");
            for (Decl* p : fnDecl->GetParameters()) {
                DumpDecl(p, indentation + 4);
            }
            if (fnDecl->GetBody()) {
                DumpStmt(fnDecl->GetBody(), indentation + 4);
            }
            return;
        } else if (StructDecl* structDecl = GetNode<StructDecl>(decl)) {
            m_Output += fmt::format("StructDecl '{}'\n", structDecl->GetRawIdentifier());
            for (const auto& field : structDecl->GetFields()) {
                DumpDecl(field, indentation + 4);
            }
            return;
        } else if (FieldDecl* fieldDecl = GetNode<FieldDecl>(decl)) {
            m_Output += fmt::format("FieldDecl '{}' '{}'\n", fieldDecl->GetRawIdentifier(), TypeInfoToString(fieldDecl->GetResolvedType()));
            return;
        } else if (MethodDecl* methodDecl = GetNode<MethodDecl>(decl)) {
            m_Output += fmt::format("MethodDecl '{}' '{}'\n", methodDecl->GetRawIdentifier(), TypeInfoToString(methodDecl->GetResolvedType()));
            for (Decl* p : methodDecl->GetParameters()) {
                DumpDecl(p, indentation + 4);
            }
            if (methodDecl->GetBody()) {
                DumpStmt(methodDecl->GetBody(), indentation + 4);
            }
            return;
        }
        
        ARIA_UNREACHABLE();
    }

    void ASTDumper::DumpStmt(Stmt* stmt, size_t indentation) {
        if (Decl* decl = GetNode<Decl>(stmt)) {
            DumpDecl(decl, indentation);
            return;
        } else if (Expr* expr = GetNode<Expr>(stmt)) {
            DumpExpr(expr, indentation);
            return;
        }

        std::string ident;
        ident.append(indentation, ' ');
        m_Output += ident;

        if (stmt == nullptr) { m_Output += "<<NULL>>\n"; return; };

        if (CompoundStmt* compound = GetNode<CompoundStmt>(stmt)) {
            m_Output += fmt::format("CompoundStmt\n");
            for (Stmt* stmt : compound->GetStmts()) {
                DumpStmt(stmt, indentation + 4);
            }
            return;
        } else if (WhileStmt* wh = GetNode<WhileStmt>(stmt)) {
            m_Output += "WhileStmt\n";
            DumpExpr(wh->GetCondition(), indentation + 4);
            DumpStmt(wh->GetBody(), indentation + 4);
            return;
        } else if (DoWhileStmt* doWh = GetNode<DoWhileStmt>(stmt)) {
            m_Output += "DoWhileStmt\n";
            DumpStmt(doWh->GetBody(), indentation + 4);
            DumpExpr(doWh->GetCondition(), indentation + 4);
            return;
        } else if (ForStmt* fo = GetNode<ForStmt>(stmt)) {
            m_Output += "ForStmt\n";
            DumpStmt(fo->GetPrologue(), indentation + 4);
            DumpExpr(fo->GetCondition(), indentation + 4);
            DumpExpr(fo->GetEpilogue(), indentation + 4);
            DumpStmt(fo->GetBody(), indentation + 4);
            return;
        } else if (IfStmt* i = GetNode<IfStmt>(stmt)) {
            m_Output += "IfStmt\n";
            DumpExpr(i->GetCondition(), indentation + 4);
            DumpStmt(i->GetBody(), indentation + 4);
            DumpStmt(i->GetElseBody(), indentation + 4);
            return;
        } else if (ReturnStmt* ret = GetNode<ReturnStmt>(stmt)) {
            m_Output += "ReturnStmt\n";
            DumpExpr(ret->GetValue(), indentation + 4);
            return;
        } 

        ARIA_UNREACHABLE();
    }

} // namespace Aria::Internal
