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
        
        if (expr->Kind == ExprKind::Error) {
            m_Output += fmt::format("ErrorExpr '{}' {}\n", TypeInfoToString(expr->Type), ExprValueKindToString(expr->ValueKind)); return;
        } else if (expr->Kind == ExprKind::BooleanConstant) {
            m_Output += fmt::format("BooleanConstantExpr {} '{}' {}\n", expr->BooleanConstant.Value, TypeInfoToString(expr->Type), ExprValueKindToString(expr->ValueKind)); return;
        } else if (expr->Kind == ExprKind::CharacterConstant) {
            m_Output += fmt::format("CharacterConstant '{:c}' '{}' {}\n", expr->CharacterConstant.Value, TypeInfoToString(expr->Type), ExprValueKindToString(expr->ValueKind)); return;
        } else if (expr->Kind == ExprKind::IntegerConstant) {
            m_Output += fmt::format("IntegerConstantExpr {} '{}' {}\n", expr->IntegerConstant.Value, TypeInfoToString(expr->Type), ExprValueKindToString(expr->ValueKind)); return;
        } else if (expr->Kind == ExprKind::FloatingConstant) {
            m_Output += fmt::format("FloatingConstantExpr {} '{}' {}\n", expr->FloatingConstant.Value, TypeInfoToString(expr->Type), ExprValueKindToString(expr->ValueKind)); return;
        } else if (expr->Kind == ExprKind::StringConstant) {
            m_Output += fmt::format("StringConstantExpr {:?} '{}' {}\n", expr->StringConstant.Value, TypeInfoToString(expr->Type), ExprValueKindToString(expr->ValueKind)); return;
        } else if (expr->Kind == ExprKind::DeclRef) {
            m_Output += fmt::format("DeclRefExpr '{}' '{}' {} {} {}\n", expr->DeclRef.Identifier, TypeInfoToString(expr->Type), ExprValueKindToString(expr->ValueKind), DeclKindToString(expr->DeclRef.ReferencedDecl->Kind), static_cast<void*>(expr->DeclRef.ReferencedDecl));
            if (expr->DeclRef.NameSpecifier) {
                DumpSpecifier(expr->DeclRef.NameSpecifier, indentation + 4);
            }
            return;
        } else if (expr->Kind == ExprKind::Member) {
            m_Output += fmt::format("MemberExpr '{}' '{}' {}\n", expr->Member.Member, TypeInfoToString(expr->Type), ExprValueKindToString(expr->ValueKind));
            DumpExpr(expr->Member.Parent, indentation + 4);
            return;
        } else if (expr->Kind == ExprKind::Self) {
            m_Output += fmt::format("SelfExpr '{}' {}\n", TypeInfoToString(expr->Type), ExprValueKindToString(expr->ValueKind)); return;
        } else if (expr->Kind == ExprKind::Temporary) {
            m_Output += fmt::format("TemporaryExpr '{}' {}\n", TypeInfoToString(expr->Type), ExprValueKindToString(expr->ValueKind));
            DumpExpr(expr->Temporary.Expression, indentation + 4);
            return;
        } else if (expr->Kind == ExprKind::Copy) {
            m_Output += fmt::format("CopyExpr '{}' {}\n", TypeInfoToString(expr->Type), ExprValueKindToString(expr->ValueKind));
            DumpExpr(expr->Copy.Expression, indentation + 4);
            return;
        } else if (expr->Kind == ExprKind::Call) {
            m_Output += fmt::format("CallExpr '{}' {}\n", TypeInfoToString(expr->Type), ExprValueKindToString(expr->ValueKind));
            for (Expr* e : expr->Call.Arguments) {
                DumpExpr(e, indentation + 4);
            }
            DumpExpr(expr->Call.Callee, indentation + 4);
            return;
        } else if (expr->Kind == ExprKind::Construct) {
            m_Output += fmt::format("ConstructExpr '{}' {} {}\n", TypeInfoToString(expr->Type), ExprValueKindToString(expr->ValueKind), reinterpret_cast<void*>(expr->Construct.Ctor));
            for (Expr* e : expr->Construct.Arguments) {
                DumpExpr(e, indentation + 4);
            }
            return;
        } else if (expr->Kind == ExprKind::MethodCall) {
            m_Output += fmt::format("MethodCallExpr '{}' {}\n", TypeInfoToString(expr->Type), ExprValueKindToString(expr->ValueKind));
            for (Expr* e : expr->MethodCall.Arguments) {
                DumpExpr(e, indentation + 4);
            }
            DumpExpr(expr->MethodCall.Callee, indentation + 4);
            return;
        } else if (expr->Kind == ExprKind::Paren) {
            m_Output += fmt::format("ParenExpr '{}' {}\n", TypeInfoToString(expr->Type), ExprValueKindToString(expr->ValueKind));
            DumpExpr(expr->Paren.Expression, indentation + 4);
            return;
        } else if (expr->Kind == ExprKind::Cast) {
            m_Output += fmt::format("CastExpr '{}' <{}> {}\n", TypeInfoToString(expr->Type), CastKindToString(expr->Cast.Kind), ExprValueKindToString(expr->ValueKind));
            DumpExpr(expr->Cast.Expression, indentation + 4);
            return;
        } else if (expr->Kind == ExprKind::ImplicitCast) {
            m_Output += fmt::format("ImplicitCastExpr '{}' <{}> {}\n", TypeInfoToString(expr->Type), CastKindToString(expr->ImplicitCast.Kind), ExprValueKindToString(expr->ValueKind));
            DumpExpr(expr->ImplicitCast.Expression, indentation + 4);
            return;
        } else if (expr->Kind == ExprKind::UnaryOperator) {
            m_Output += fmt::format("UnaryOperatorExpr '{}' '{}' {}\n", UnaryOperatorKindToString(expr->UnaryOperator.Operator), TypeInfoToString(expr->Type), ExprValueKindToString(expr->ValueKind));
            DumpExpr(expr->UnaryOperator.Expression, indentation + 4);
            return;
        } else if (expr->Kind == ExprKind::BinaryOperator) {
            m_Output += fmt::format("BinaryOperatorExpr '{}' '{}' {}\n", BinaryOperatorKindToString(expr->BinaryOperator.Operator), TypeInfoToString(expr->Type), ExprValueKindToString(expr->ValueKind));
            DumpExpr(expr->BinaryOperator.LHS, indentation + 4);
            DumpExpr(expr->BinaryOperator.RHS, indentation + 4);
            return;
        } else if (expr->Kind == ExprKind::CompoundAssign) {
            m_Output += fmt::format("CompoundAssignExpr '{}' '{}' {}\n", BinaryOperatorKindToString(expr->BinaryOperator.Operator), TypeInfoToString(expr->Type), ExprValueKindToString(expr->ValueKind));
            DumpExpr(expr->BinaryOperator.LHS, indentation + 4);
            DumpExpr(expr->BinaryOperator.RHS, indentation + 4);
            return;
        }

        ARIA_UNREACHABLE();
    }

    void ASTDumper::DumpDecl(Decl* decl, size_t indentation) {
        std::string ident;
        ident.append(indentation, ' ');
        m_Output += ident;

        if (decl == nullptr) { m_Output += "<<NULL>>\n"; return; };

        if (decl->Kind == DeclKind::Error) {
            m_Output += "ErrorDecl\n";
            return;
        }
        else if (decl->Kind == DeclKind::TranslationUnit) {
            m_Output += "TranslationUnitDecl\n";

            for (Stmt* stmt : decl->TranslationUnit.Stmts) {
                DumpStmt(stmt, indentation + 4);
            }
            return;
        } else if (decl->Kind == DeclKind::Module) {
            m_Output += fmt::format("ModuleDecl '{}'\n", decl->Module.Name);
            return;
        } else if (decl->Kind == DeclKind::Var) {
            m_Output += fmt::format("VarDecl '{}' '{}'\n", decl->Var.Identifier, TypeInfoToString(decl->Var.Type));
            if (decl->Var.Initializer) {
                DumpExpr(decl->Var.Initializer, indentation + 4);
            }
            return;
        } else if (decl->Kind == DeclKind::Param) {
            m_Output += fmt::format("ParamDecl '{}' '{}'\n", decl->Param.Identifier, TypeInfoToString(decl->Param.Type));
            return;
        } else if (decl->Kind == DeclKind::Function) {
            m_Output += fmt::format("FunctionDecl '{}' '{}' {}\n", decl->Function.Identifier, TypeInfoToString(decl->Function.Type), DumpDeclarationFlags(decl->Flags));
            for (Decl* p : decl->Function.Parameters) {
                DumpDecl(p, indentation + 4);
            }
            if (decl->Function.Body) {
                DumpStmt(decl->Function.Body, indentation + 4);
            }
            return;
        } else if (decl->Kind == DeclKind::Struct) {
            m_Output += fmt::format("StructDecl '{}'\n", decl->Struct.Identifier);

            for (Decl* field : decl->Struct.Fields) {
                DumpDecl(field, indentation + 4);
            }
            return;
        } else if (decl->Kind == DeclKind::Field) {
            m_Output += fmt::format("FieldDecl '{}' '{}'\n", decl->Field.Identifier, TypeInfoToString(decl->Field.Type));
            return;
        } else if (decl->Kind == DeclKind::Constructor) {
            m_Output += "ConstructorDecl\n";

            for (Decl* param : decl->Constructor.Parameters) {
                DumpDecl(param, indentation + 4);
            }

            DumpStmt(decl->Constructor.Body, indentation + 4);
            return;
        } else if (decl->Kind == DeclKind::Destructor) {
            m_Output += "DestructorDecl\n";

            DumpStmt(decl->Destructor.Body, indentation + 4);
            return;
        } else if (decl->Kind == DeclKind::Method) {
            m_Output += fmt::format("MethodDecl '{}' '{}'\n", decl->Method.Identifier, TypeInfoToString(decl->Method.Type));
            for (Decl* p : decl->Method.Parameters) {
                DumpDecl(p, indentation + 4);
            }
            if (decl->Method.Body) {
                DumpStmt(decl->Method.Body, indentation + 4);
            }
            return;
        }
        
        ARIA_UNREACHABLE();
    }

    void ASTDumper::DumpStmt(Stmt* stmt, size_t indentation) {
        if (stmt == nullptr) { m_Output += "<<NULL>>\n"; return; };

        if (stmt->Kind == StmtKind::Decl) {
            DumpDecl(stmt->DeclStmt, indentation);
            return;
        } else if (stmt->Kind == StmtKind::Expr) {
            DumpExpr(stmt->ExprStmt, indentation);
            return;
        }

        std::string ident;
        ident.append(indentation, ' ');
        m_Output += ident;

        if (stmt->Kind == StmtKind::Error) {
            m_Output += fmt::format("ErrorStmt\n");
            return;
        }
        else if (stmt->Kind == StmtKind::Nop) {
            m_Output += fmt::format("NopStmt\n");
            return;
        } else if (stmt->Kind == StmtKind::Import) {
            m_Output += fmt::format("ImportStmt '{}'\n", stmt->Import.Name);
            return;
        } else if (stmt->Kind == StmtKind::Block) {
            m_Output += fmt::format("BlockStmt\n");
            for (Stmt* stmt : stmt->Block.Stmts) {
                DumpStmt(stmt, indentation + 4);
            }
            return;
        } else if (stmt->Kind == StmtKind::While) {
            m_Output += "WhileStmt\n";
            DumpExpr(stmt->While.Condition, indentation + 4);
            DumpStmt(stmt->While.Body, indentation + 4);
            return;
        } else if (stmt->Kind == StmtKind::DoWhile) {
            m_Output += "DoWhileStmt\n";
            DumpExpr(stmt->DoWhile.Condition, indentation + 4);
            DumpStmt(stmt->DoWhile.Body, indentation + 4);
            return;
        } else if (stmt->Kind == StmtKind::For) {
            m_Output += "ForStmt\n";
            DumpDecl(stmt->For.Prologue, indentation + 4);
            DumpExpr(stmt->For.Condition, indentation + 4);
            DumpExpr(stmt->For.Step, indentation + 4);
            DumpStmt(stmt->For.Body, indentation + 4);
            return;
        } else if (stmt->Kind == StmtKind::If) {
            m_Output += "IfStmt\n";
            DumpExpr(stmt->If.Condition, indentation + 4);
            DumpStmt(stmt->If.Body, indentation + 4);
            if (stmt->If.ElseBody) DumpStmt(stmt->If.ElseBody, indentation + 4);
            return;
        } else if (stmt->Kind == StmtKind::Break) {
            m_Output += "BreakStmt\n";
            return;
        } else if (stmt->Kind == StmtKind::Continue) {
            m_Output += "ContinueStmt\n";
            return;
        } else if (stmt->Kind == StmtKind::Return) {
            m_Output += "ReturnStmt\n";
            DumpExpr(stmt->Return.Value, indentation + 4);
            return;
        } 

        ARIA_UNREACHABLE();
    }

    void ASTDumper::DumpSpecifier(Specifier* spec, size_t indentation) {
        if (spec == nullptr) { m_Output += "<<NULL>>\n"; return; };

        std::string ident;
        ident.append(indentation, ' ');
        m_Output += ident;

        if (spec->Kind == SpecifierKind::Scope) {
            m_Output += fmt::format("ScopeSpecifier '{}' {}\n", spec->Scope.Identifier, static_cast<void*>(spec->Scope.ReferencedModule));
            return;
        }

        ARIA_UNREACHABLE();
    }

    std::string ASTDumper::DumpDeclarationFlags(int flags) {
        std::string result;

        if (flags & DECL_FLAG_EXTERN) {
            result = "@extern ";
        }

        if (flags & DECL_FLAG_NOMANGLE) {
            result += "@nomangle ";
        }

        if (flags & DECL_FLAG_PRIVATE) {
            result += "@private ";
        }

        return result;
    }

} // namespace Aria::Internal
