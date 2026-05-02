#include "aria/internal/compiler/ast/ast_dumper.hpp"

#include "fmt/std.h"

namespace Aria::Internal {

    ASTDumper::ASTDumper(Stmt* rootASTNode) {
        m_RootASTNode = rootASTNode;

        dump_ast_impl();
    }

    std::string& ASTDumper::get_output() {
        return m_Output;
    }

    void ASTDumper::dump_ast_impl() {
        dump_stmt(m_RootASTNode, 0);
    }

    void ASTDumper::dump_expr(Expr* expr, size_t indentation) {
        std::string ident;
        ident.append(indentation, ' ');
        m_Output += ident;

        if (expr == nullptr) { m_Output += "<<NULL>>\n"; return; };
        
        if (expr->Kind == ExprKind::Error) {
            m_Output += fmt::format("ErrorExpr '{}' {}\n", type_info_to_string(expr->Type), ExprValueKindToString(expr->ValueKind)); return;
        } else if (expr->Kind == ExprKind::BooleanConstant) {
            m_Output += fmt::format("BooleanConstantExpr {} '{}' {}\n", expr->BooleanConstant.Value, type_info_to_string(expr->Type), ExprValueKindToString(expr->ValueKind)); return;
        } else if (expr->Kind == ExprKind::CharacterConstant) {
            m_Output += fmt::format("CharacterConstant 0x{:x} '{}' {}\n", expr->CharacterConstant.Value, type_info_to_string(expr->Type), ExprValueKindToString(expr->ValueKind)); return;
        } else if (expr->Kind == ExprKind::IntegerConstant) {
            m_Output += fmt::format("IntegerConstantExpr {} '{}' {}\n", expr->IntegerConstant.Value, type_info_to_string(expr->Type), ExprValueKindToString(expr->ValueKind)); return;
        } else if (expr->Kind == ExprKind::FloatingConstant) {
            m_Output += fmt::format("FloatingConstantExpr {} '{}' {}\n", expr->FloatingConstant.Value, type_info_to_string(expr->Type), ExprValueKindToString(expr->ValueKind)); return;
        } else if (expr->Kind == ExprKind::StringConstant) {
            m_Output += fmt::format("StringConstantExpr {:?} '{}' {}\n", expr->StringConstant.Value, type_info_to_string(expr->Type), ExprValueKindToString(expr->ValueKind)); return;
        } else if (expr->Kind == ExprKind::Null) {
            m_Output += fmt::format("NullExpr '{}' {}\n", type_info_to_string(expr->Type), ExprValueKindToString(expr->ValueKind)); return;
        } else if (expr->Kind == ExprKind::DeclRef) {
            m_Output += fmt::format("DeclRefExpr '{}' '{}' {} {} {}\n", expr->DeclRef.Identifier, type_info_to_string(expr->Type), ExprValueKindToString(expr->ValueKind), DeclKindToString(expr->DeclRef.ReferencedDecl->Kind), static_cast<void*>(expr->DeclRef.ReferencedDecl));
            if (expr->DeclRef.NameSpecifier) {
                dump_specifier(expr->DeclRef.NameSpecifier, indentation + 4);
            }
            return;
        } else if (expr->Kind == ExprKind::Member) {
            m_Output += fmt::format("MemberExpr '{}' '{}' {}\n", expr->Member.Member, type_info_to_string(expr->Type), ExprValueKindToString(expr->ValueKind));
            dump_expr(expr->Member.Parent, indentation + 4);
            return;
        } else if (expr->Kind == ExprKind::BuiltinMember) {
            m_Output += fmt::format("BuiltinMemberExpr '{}' '{}' {}\n", expr->Member.Member, type_info_to_string(expr->Type), ExprValueKindToString(expr->ValueKind));
            dump_expr(expr->Member.Parent, indentation + 4);
            return;
        } else if (expr->Kind == ExprKind::Self) {
            m_Output += fmt::format("SelfExpr '{}' {}\n", type_info_to_string(expr->Type), ExprValueKindToString(expr->ValueKind)); return;
        } else if (expr->Kind == ExprKind::Temporary) {
            m_Output += fmt::format("TemporaryExpr '{}' {}\n", type_info_to_string(expr->Type), ExprValueKindToString(expr->ValueKind));
            dump_expr(expr->Temporary.Expression, indentation + 4);
            return;
        } else if (expr->Kind == ExprKind::Copy) {
            m_Output += fmt::format("CopyExpr '{}' {}\n", type_info_to_string(expr->Type), ExprValueKindToString(expr->ValueKind));
            dump_expr(expr->Copy.Expression, indentation + 4);
            return;
        } else if (expr->Kind == ExprKind::Call) {
            m_Output += fmt::format("CallExpr '{}' {}\n", type_info_to_string(expr->Type), ExprValueKindToString(expr->ValueKind));
            for (Expr* e : expr->Call.Arguments) {
                dump_expr(e, indentation + 4);
            }
            dump_expr(expr->Call.Callee, indentation + 4);
            return;
        } else if (expr->Kind == ExprKind::Construct) {
            m_Output += fmt::format("ConstructExpr '{}' {} {}\n", type_info_to_string(expr->Type), ExprValueKindToString(expr->ValueKind), reinterpret_cast<void*>(expr->Construct.Ctor));
            for (Expr* e : expr->Construct.Arguments) {
                dump_expr(e, indentation + 4);
            }
            return;
        } else if (expr->Kind == ExprKind::MethodCall) {
            m_Output += fmt::format("MethodCallExpr '{}' {}\n", type_info_to_string(expr->Type), ExprValueKindToString(expr->ValueKind));
            for (Expr* e : expr->MethodCall.Arguments) {
                dump_expr(e, indentation + 4);
            }
            dump_expr(expr->MethodCall.Callee, indentation + 4);
            return;
        } else if (expr->Kind == ExprKind::ToSlice) {
            m_Output += fmt::format("ToSliceExpr '{}' {}\n", type_info_to_string(expr->Type), ExprValueKindToString(expr->ValueKind));
            dump_expr(expr->ToSlice.Source, indentation + 4);
            dump_expr(expr->ToSlice.Len, indentation + 4);
            return;
        }  else if (expr->Kind == ExprKind::ArraySubscript) {
            m_Output += fmt::format("ArraySubscriptExpr '{}' {}\n", type_info_to_string(expr->Type), ExprValueKindToString(expr->ValueKind));
            dump_expr(expr->ArraySubscript.Array, indentation + 4);
            dump_expr(expr->ArraySubscript.Index, indentation + 4);
            return;
        } else if (expr->Kind == ExprKind::New) {
            m_Output += fmt::format("NewExpr {}'{}' {}\n", expr->New.Array ? "array " : "", type_info_to_string(expr->Type), ExprValueKindToString(expr->ValueKind));
            if (expr->New.Initializer) {
                dump_expr(expr->New.Initializer, indentation + 4);
            }
            return;
        } else if (expr->Kind == ExprKind::Delete) {
            m_Output += fmt::format("DeleteExpr '{}' {}\n", type_info_to_string(expr->Type), ExprValueKindToString(expr->ValueKind));
            dump_expr(expr->Delete.Expression, indentation + 4);
            return;
        } else if (expr->Kind == ExprKind::Format) {
            m_Output += fmt::format("FormatExpr '{}' {}\n", type_info_to_string(expr->Type), ExprValueKindToString(expr->ValueKind));
            for (auto& arg : expr->Format.ResolvedArgs) {
                dump_expr(arg.Arg, indentation + 4);
            }
            return;
        } else if (expr->Kind == ExprKind::Paren) {
            m_Output += fmt::format("ParenExpr '{}' {}\n", type_info_to_string(expr->Type), ExprValueKindToString(expr->ValueKind));
            dump_expr(expr->Paren.Expression, indentation + 4);
            return;
        } else if (expr->Kind == ExprKind::Cast) {
            m_Output += fmt::format("CastExpr '{}' {}\n", type_info_to_string(expr->Type), ExprValueKindToString(expr->ValueKind));
            dump_expr(expr->Cast.Expression, indentation + 4);
            return;
        } else if (expr->Kind == ExprKind::ImplicitCast) {
            m_Output += fmt::format("ImplicitCastExpr '{}' <{}> {}\n", type_info_to_string(expr->Type), CastKindToString(expr->ImplicitCast.Kind), ExprValueKindToString(expr->ValueKind));
            dump_expr(expr->ImplicitCast.Expression, indentation + 4);
            return;
        } else if (expr->Kind == ExprKind::UnaryOperator) {
            m_Output += fmt::format("UnaryOperatorExpr '{}' '{}' {}\n", UnaryOperatorKindToString(expr->UnaryOperator.Operator), type_info_to_string(expr->Type), ExprValueKindToString(expr->ValueKind));
            dump_expr(expr->UnaryOperator.Expression, indentation + 4);
            return;
        } else if (expr->Kind == ExprKind::BinaryOperator) {
            m_Output += fmt::format("BinaryOperatorExpr '{}' '{}' {}\n", BinaryOperatorKindToString(expr->BinaryOperator.Operator), type_info_to_string(expr->Type), ExprValueKindToString(expr->ValueKind));
            dump_expr(expr->BinaryOperator.LHS, indentation + 4);
            dump_expr(expr->BinaryOperator.RHS, indentation + 4);
            return;
        } else if (expr->Kind == ExprKind::CompoundAssign) {
            m_Output += fmt::format("CompoundAssignExpr '{}' '{}' {}\n", BinaryOperatorKindToString(expr->BinaryOperator.Operator), type_info_to_string(expr->Type), ExprValueKindToString(expr->ValueKind));
            dump_expr(expr->BinaryOperator.LHS, indentation + 4);
            dump_expr(expr->BinaryOperator.RHS, indentation + 4);
            return;
        }

        ARIA_UNREACHABLE();
    }

    void ASTDumper::dump_decl(Decl* decl, size_t indentation) {
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
                dump_stmt(stmt, indentation + 4);
            }
            return;
        } else if (decl->Kind == DeclKind::Module) {
            m_Output += fmt::format("ModuleDecl '{}'\n", decl->Module.Name);
            return;
        } else if (decl->Kind == DeclKind::Var) {
            m_Output += fmt::format("VarDecl '{}' '{}'\n", decl->Var.Identifier, type_info_to_string(decl->Var.Type));
            if (decl->Var.Initializer) {
                dump_expr(decl->Var.Initializer, indentation + 4);
            }
            return;
        } else if (decl->Kind == DeclKind::Param) {
            m_Output += fmt::format("ParamDecl '{}' '{}'\n", decl->Param.Identifier, type_info_to_string(decl->Param.Type));
            return;
        } else if (decl->Kind == DeclKind::Function) {
            m_Output += fmt::format("FunctionDecl '{}' '{}'\n", decl->Function.Identifier, type_info_to_string(decl->Function.Type));
            for (auto& attr : decl->Function.Attributes) {
                dump_function_attr(attr, indentation + 4);
            }

            for (Decl* p : decl->Function.Parameters) {
                dump_decl(p, indentation + 4);
            }

            if (decl->Function.Body) {
                dump_stmt(decl->Function.Body, indentation + 4);
            }
            return;
        } else if (decl->Kind == DeclKind::Struct) {
            m_Output += fmt::format("StructDecl '{}'\n", decl->Struct.Identifier);

            for (Decl* field : decl->Struct.Fields) {
                dump_decl(field, indentation + 4);
            }
            return;
        } else if (decl->Kind == DeclKind::Field) {
            m_Output += fmt::format("FieldDecl '{}' '{}'\n", decl->Field.Identifier, type_info_to_string(decl->Field.Type));
            return;
        } else if (decl->Kind == DeclKind::Constructor) {
            m_Output += "ConstructorDecl\n";

            for (Decl* param : decl->Constructor.Parameters) {
                dump_decl(param, indentation + 4);
            }

            dump_stmt(decl->Constructor.Body, indentation + 4);
            return;
        } else if (decl->Kind == DeclKind::Destructor) {
            m_Output += "DestructorDecl\n";

            dump_stmt(decl->Destructor.Body, indentation + 4);
            return;
        } else if (decl->Kind == DeclKind::Method) {
            m_Output += fmt::format("MethodDecl '{}' '{}'\n", decl->Method.Identifier, type_info_to_string(decl->Method.Type));
            for (Decl* p : decl->Method.Parameters) {
                dump_decl(p, indentation + 4);
            }
            if (decl->Method.Body) {
                dump_stmt(decl->Method.Body, indentation + 4);
            }
            return;
        }
        
        ARIA_UNREACHABLE();
    }

    void ASTDumper::dump_stmt(Stmt* stmt, size_t indentation) {
        if (stmt == nullptr) { m_Output += "<<NULL>>\n"; return; };

        if (stmt->Kind == StmtKind::Decl) {
            dump_decl(stmt->DeclStmt, indentation);
            return;
        } else if (stmt->Kind == StmtKind::Expr) {
            dump_expr(stmt->ExprStmt, indentation);
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
            m_Output += fmt::format("BlockStmt {}\n", stmt->Block.Unsafe ? "unsafe" : "");
            for (Stmt* stmt : stmt->Block.Stmts) {
                dump_stmt(stmt, indentation + 4);
            }
            return;
        } else if (stmt->Kind == StmtKind::While) {
            m_Output += "WhileStmt\n";
            dump_expr(stmt->While.Condition, indentation + 4);
            dump_stmt(stmt->While.Body, indentation + 4);
            return;
        } else if (stmt->Kind == StmtKind::DoWhile) {
            m_Output += "DoWhileStmt\n";
            dump_expr(stmt->DoWhile.Condition, indentation + 4);
            dump_stmt(stmt->DoWhile.Body, indentation + 4);
            return;
        } else if (stmt->Kind == StmtKind::For) {
            m_Output += "ForStmt\n";
            dump_decl(stmt->For.Prologue, indentation + 4);
            dump_expr(stmt->For.Condition, indentation + 4);
            dump_expr(stmt->For.Step, indentation + 4);
            dump_stmt(stmt->For.Body, indentation + 4);
            return;
        } else if (stmt->Kind == StmtKind::If) {
            m_Output += "IfStmt\n";
            dump_expr(stmt->If.Condition, indentation + 4);
            dump_stmt(stmt->If.Body, indentation + 4);
            if (stmt->If.ElseBody) dump_stmt(stmt->If.ElseBody, indentation + 4);
            return;
        } else if (stmt->Kind == StmtKind::Break) {
            m_Output += "BreakStmt\n";
            return;
        } else if (stmt->Kind == StmtKind::Continue) {
            m_Output += "ContinueStmt\n";
            return;
        } else if (stmt->Kind == StmtKind::Return) {
            m_Output += "ReturnStmt\n";
            dump_expr(stmt->Return.Value, indentation + 4);
            return;
        } 

        ARIA_UNREACHABLE();
    }

    void ASTDumper::dump_specifier(Specifier* spec, size_t indentation) {
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

    void ASTDumper::dump_function_attr(FunctionDecl::Attribute attr, size_t indentation) {
        std::string ident;
        ident.append(indentation, ' ');
        m_Output += ident;

        switch (attr.Kind) {
            case FunctionDecl::AttributeKind::Extern: m_Output += fmt::format("ExternAttribute {:?}\n", attr.Arg); break;
            case FunctionDecl::AttributeKind::NoMangle: m_Output += "NoMangleAttribute\n"; break;
            case FunctionDecl::AttributeKind::Unsafe: m_Output += "UnsafeAttribute\n"; break;
            default: ARIA_UNREACHABLE();
        }
    }

} // namespace Aria::Internal
