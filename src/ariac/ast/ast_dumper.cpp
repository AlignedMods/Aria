#include "ariac/ast/ast_dumper.hpp"

#include "fmt/std.h"

namespace ariac {

    ASTDumper::ASTDumper(Stmt* rootASTNode) {
        m_root_ast_node = rootASTNode;

        dump_ast_impl();
    }

    std::string& ASTDumper::get_output() {
        return m_output;
    }

    void ASTDumper::dump_ast_impl() {
        dump_stmt(m_root_ast_node, 0);
    }

    void ASTDumper::dump_expr(Expr* expr, size_t indentation) {
        std::string ident;
        ident.append(indentation, ' ');
        m_output += ident;

        if (expr == nullptr) { m_output += "<<NULL>>\n"; return; };

        switch (expr->kind) {
            case ExprKind::Error: m_output += fmt::format("ErrorExpr '{}' {}\n", 
                type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind)); 
                return;

            case ExprKind::BooleanLiteral: m_output += fmt::format("BooleanLiteralExpr {} '{}' {}\n", 
                expr->boolean_literal.value, 
                type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind)); 
                return;

            case ExprKind::CharacterLiteral: m_output += fmt::format("CharacterLiteralExpr 0x{:x} '{}' {}\n", 
                expr->character_literal.value, 
                type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind)); 
                return;

            case ExprKind::IntegerLiteral: m_output += fmt::format("IntegerLiteralExpr {} '{}' {}\n", 
                expr->integer_literal.value, 
                type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind)); 
                return;

            case ExprKind::FloatingLiteral: m_output += fmt::format("FloatingLiteralExpr {} '{}' {}\n", 
                expr->floating_literal.value, 
                type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind)); 
                return;

            case ExprKind::StringLiteral: m_output += fmt::format("StringLiteralExpr {:?} '{}' {}\n", 
                expr->string_literal.value, 
                type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind)); 
                return;

            case ExprKind::ArrayFiller: m_output += fmt::format("ArrayFillerExpr '{}' {}\n",
                type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                dump_expr(expr->array_filler.initializer, indentation + 4);
                return;

            case ExprKind::Null: m_output += fmt::format("NullExpr '{}' {}\n", 
                type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind)); 
                return;

            case ExprKind::DeclRef: m_output += fmt::format("DeclRefExpr '{}' {} '{}' {}\n", 
                expr->decl_ref.identifier, decl_kind_to_string(expr->decl_ref.referenced_decl->kind),
                type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                if (expr->decl_ref.name_specifier) {
                    dump_specifier(expr->decl_ref.name_specifier, indentation + 4);
                }

                return;

            case ExprKind::Member: m_output += fmt::format("MemberExpr '{}'{} '{}' {}\n",
                expr->member.member, expr->member.implicit_deref ? " implicit_deref" : "",
                type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                dump_expr(expr->member.parent, indentation + 4);
                return;

            case ExprKind::BuiltinMember: m_output += fmt::format("BuiltinMemberExpr '{}' '{}' {}\n",
                expr->member.member,
                type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                dump_expr(expr->member.parent, indentation + 4);
                return;

            case ExprKind::Self: m_output += fmt::format("SelfExpr '{}' {}\n", 
                type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind)); 
                return;

            case ExprKind::Temporary: m_output += fmt::format("TemporaryExpr '{}' {}\n", 
                type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind)); 
                
                dump_expr(expr->temporary.expression, indentation + 4);
                return;

            case ExprKind::Call: m_output += fmt::format("CallExpr '{}' {}\n",
                type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                dump_expr(expr->call.callee, indentation + 4);
                for (Expr* arg : expr->call.arguments) {
                    dump_expr(arg, indentation + 4);
                }
                return;

            case ExprKind::Construct: m_output += fmt::format("ConstructExpr '{}' {}\n",
                type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                for (Expr* arg : expr->construct.arguments) {
                    dump_expr(arg, indentation + 4);
                }
                return;

            case ExprKind::MethodCall: m_output += fmt::format("MethodCallExpr '{}' {}\n",
                type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                dump_expr(expr->method_call.callee, indentation + 4);
                for (Expr* arg : expr->method_call.arguments) {
                    dump_expr(arg, indentation + 4);
                }
                return;

            case ExprKind::ArraySubscript: m_output += fmt::format("ArraySubscriptExpr '{}' {}\n",
                type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                dump_expr(expr->array_subscript.array, indentation + 4);
                dump_expr(expr->array_subscript.index, indentation + 4);
                return;

            case ExprKind::ToSlice: m_output += fmt::format("ToSliceExpr '{}' {}\n",
                type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                dump_expr(expr->to_slice.len, indentation + 4);
                dump_expr(expr->to_slice.source, indentation + 4);
                return;

            case ExprKind::New: m_output += fmt::format("NewExpr {}'{}' {}\n",
                expr->new_.array ? "array " : "",
                type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                dump_expr(expr->new_.initializer, indentation + 4);
                return;

            case ExprKind::Delete: m_output += fmt::format("DeleteExpr '{}' {}\n",
                type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                dump_expr(expr->delete_.expression, indentation + 4);
                return;

            case ExprKind::Sizeof: m_output += fmt::format("SizeofExpr '{}' '{}' {}\n",
                type_info_to_string(expr->sizeof_.expression ? expr->sizeof_.expression->type : expr->sizeof_.type, false), type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                if (expr->sizeof_.expression) { dump_expr(expr->sizeof_.expression, indentation + 4); }
                return;

            case ExprKind::Paren: m_output += fmt::format("ParenExpr '{}' {}\n",
                type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                dump_expr(expr->paren.expression, indentation + 4);
                return;

            case ExprKind::Cast: m_output += fmt::format("CastExpr '{}' {}\n",
                type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                dump_expr(expr->cast.expression, indentation + 4);
                return;

            case ExprKind::ImplicitCast: m_output += fmt::format("ImplicitCastExpr <{}> '{}' {}\n",
                cast_kind_to_string(expr->implicit_cast.kind),
                type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                dump_expr(expr->implicit_cast.expression, indentation + 4);
                return;

            case ExprKind::UnaryOperator: m_output += fmt::format("UnaryOperatorExpr '{}' {} '{}' {}\n",
                unary_op_kind_to_string(expr->unary_operator.op), expr->unary_operator.infix ? "infix" : "prefix",
                type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                dump_expr(expr->unary_operator.expression, indentation + 4);
                return;

            case ExprKind::BinaryOperator: m_output += fmt::format("BinaryOperatorExpr '{}' '{}' {}\n",
                binary_op_kind_to_string(expr->binary_operator.op),
                type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                dump_expr(expr->binary_operator.lhs, indentation + 4);
                dump_expr(expr->binary_operator.rhs, indentation + 4);
                return;

            case ExprKind::CompoundAssign: m_output += fmt::format("CompoundAssignExpr '{}' '{}' {}\n",
                binary_op_kind_to_string(expr->compound_assign.op),
                type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                dump_expr(expr->compound_assign.lhs, indentation + 4);
                dump_expr(expr->compound_assign.rhs, indentation + 4);
                return;

            case ExprKind::Const: { m_output += fmt::format("ConstExpr '{}'\n",
                type_info_to_string(expr->type));

                ident.append(4, ' ');
                m_output += ident;

                switch (expr->const_.kind) {
                    case ConstExprKind::Error: m_output += "value: Error error\n"; break;
                    case ConstExprKind::Boolean: m_output += fmt::format("value: Boolean {}\n", expr->const_.boolean); break;
                    case ConstExprKind::Integer: m_output += fmt::format("value: Integer {}\n", expr->const_.integer); break;
                    case ConstExprKind::Floating: m_output += fmt::format("value: Floating {}\n", expr->const_.number); break;
                    case ConstExprKind::String: m_output += fmt::format("value: String {:?}\n", expr->const_.string); break;
                    case ConstExprKind::Struct: {
                        m_output += "value: Struct {\n";

                        for (size_t i = 0; i < expr->const_.values.size; i++) {
                            dump_expr(expr->const_.values.items[i], indentation + 8);
                        }

                        m_output += ident;
                        m_output += "}\n";

                        break;
                    }
                    default: ARIA_UNREACHABLE();
                }

                return;
            }

            default: ARIA_UNREACHABLE();
        }
    }

    void ASTDumper::dump_decl(Decl* decl, size_t indentation) {
        std::string ident;
        ident.append(indentation, ' ');
        m_output += ident;

        if (decl == nullptr) { m_output += "<<NULL>>\n"; return; };

        switch (decl->kind) {
            case DeclKind::Error: m_output += "ErrorDecl\n";
                return;

            case DeclKind::TranslationUnit: m_output += fmt::format("TranslationUnitDecl '{}'\n", decl->translation_unit.name);
                for (Stmt* stmt : decl->translation_unit.stmts) {
                    dump_stmt(stmt, indentation + 4);
                }
                return;

            case DeclKind::Module: m_output += fmt::format("ModuleDecl '{}'\n",
                decl->module.name);
                return;

            case DeclKind::Var: m_output += fmt::format("VarDecl {}{}'{}' '{}'\n",
                decl->var.global_var ? "global " : "", decl->var.const_var ? "const " : "", decl->var.identifier, type_info_to_string(decl->var.type, false));
                if (decl->var.initializer) {
                    dump_expr(decl->var.initializer, indentation + 4);
                }
                return;

            case DeclKind::Param: m_output += fmt::format("ParamDecl '{}' '{}'\n",
                decl->param.identifier, type_info_to_string(decl->param.type, false));
                return;

            case DeclKind::Function: m_output += fmt::format("FunctionDecl '{}' {} '{}' {}\n",
                decl->function.identifier, decl_visibility_to_string(decl->visibility), type_info_to_string(decl->function.type, false), linkage_kind_to_string(decl->function.linkage_kind));

                dump_attributes(decl->attributes, indentation + 4);
                
                for (Decl* param : decl->function.parameters) {
                    dump_decl(param, indentation + 4);
                }

                if (decl->function.body) {
                    dump_stmt(decl->function.body, indentation + 4);
                }
                return;

            case DeclKind::Struct: m_output += fmt::format("StructDecl '{}'\n",
                decl->struct_.identifier);

                for (Decl* field : decl->struct_.fields) {
                    dump_decl(field, indentation + 4);
                }

                return;

            case DeclKind::Impl: m_output += fmt::format("ImplDecl '{}'\n",
                decl->impl.identifier);

                for (Decl* field : decl->impl.fields) {
                    dump_decl(field, indentation + 4);
                }

                return;

            case DeclKind::Typedef: m_output += fmt::format("TypedefDecl '{}' '{}'\n",
                type_info_to_string(decl->typedef_.type, false), decl->typedef_.identifier);
                return;

            case DeclKind::Field: m_output += fmt::format("FieldDecl '{}' '{}' {}\n",
                decl->field.identifier, type_info_to_string(decl->field.type, false), decl_visibility_to_string(decl->visibility));
                return;

            case DeclKind::Method: m_output += fmt::format("MethodDecl '{}' '{}'\n",
                decl->method.identifier, type_info_to_string(decl->method.type, false));

                for (Decl* param : decl->method.parameters) {
                    dump_decl(param, indentation + 4);
                }

                dump_stmt(decl->method.body, indentation + 4);
                return;

            default: ARIA_UNREACHABLE();
        }
    }

    void ASTDumper::dump_stmt(Stmt* stmt, size_t indentation) {
        if (stmt == nullptr) { m_output += "<<NULL>>\n"; return; };

        if (stmt->kind == StmtKind::Decl) {
            dump_decl(stmt->decl, indentation);
            return;
        } else if (stmt->kind == StmtKind::Expr) {
            dump_expr(stmt->expr, indentation);
            return;
        }
        
        std::string ident;
        ident.append(indentation, ' ');
        m_output += ident;

        switch (stmt->kind) {
            case StmtKind::Error: m_output += "ErrorStmt\n";
                return;

            case StmtKind::Nop: m_output += "NopStmt\n";
                return;

            case StmtKind::Import: m_output += fmt::format("ImportStmt '{}'\n",
                stmt->import.name);
                return;

            case StmtKind::Block: m_output += fmt::format("BlockStmt {}\n",
                stmt->block.unsafe ? "unsafe" : "");
                for (Stmt* stmt : stmt->block.stmts) {
                    dump_stmt(stmt, indentation + 4);
                }
                return;

            case StmtKind::While: m_output += "WhileStmt\n";
                dump_expr(stmt->while_.condition, indentation + 4);
                dump_stmt(stmt->while_.body, indentation + 4);
                return;

            case StmtKind::DoWhile: m_output += "DoWhileStmt\n";
                dump_expr(stmt->do_while.condition, indentation + 4);
                dump_stmt(stmt->do_while.body, indentation + 4);
                return;

            case StmtKind::For: m_output += "ForStmt\n";
                dump_decl(stmt->for_.prologue, indentation + 4);
                dump_expr(stmt->for_.condition, indentation + 4);
                dump_expr(stmt->for_.step, indentation + 4);
                dump_stmt(stmt->for_.body, indentation + 4);
                return;

            case StmtKind::If: m_output += "IfStmt\n";
                dump_expr(stmt->if_.condition, indentation + 4);
                dump_stmt(stmt->if_.body, indentation + 4);
                if (stmt->if_.else_body) { dump_stmt(stmt->if_.else_body, indentation + 4); }
                return;

            case StmtKind::Break: m_output += "BreakStmt\n";
                return;

            case StmtKind::Continue: m_output += "ContinueStmt\n";
                return;

            case StmtKind::Return: m_output += "ReturnStmt\n";
                dump_expr(stmt->return_.value, indentation + 4);
                return;

            case StmtKind::Defer: m_output += "DeferStmt\n";
                dump_stmt(stmt->defer.statement, indentation + 4);
                return;

            default: ARIA_UNREACHABLE();
        }
    }

    void ASTDumper::dump_specifier(Specifier* spec, size_t indentation) {
        if (spec == nullptr) { m_output += "<<NULL>>\n"; return; };

        std::string ident;
        ident.append(indentation, ' ');
        m_output += ident;

        if (spec->kind == SpecifierKind::Name) {
            m_output += fmt::format("NameSpecifier '{}'\n", spec->name.identifier);
            return;
        }

        ARIA_UNREACHABLE();
    }

    void ASTDumper::dump_attributes(TinyVector<DeclAttribute> attrs, size_t indentation) {
        for (auto& attr : attrs) {
            dump_attribute(attr, indentation);
        }
    }

    void ASTDumper::dump_attribute(DeclAttribute attr, size_t indentation) {
        std::string ident;
        ident.append(indentation, ' ');
        m_output += ident;

        switch (attr.kind) {
            case DeclAttributeKind::If: {
                m_output += "IfAttribute\n";
                dump_expr(attr.arg, indentation + 4);
                return;
            }

            default: ARIA_UNREACHABLE();
        }
    }

} // namespace ariac
