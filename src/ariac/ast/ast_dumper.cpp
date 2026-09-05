#include "ariac/ast/ast_dumper.hpp"

#include "fmt/std.h"

namespace ariac {

    ASTDumper::ASTDumper(Module* mod) {
        for (CompilationUnit* unit : mod->units) {
            if (unit->is_stdlib) { continue; }

            m_unit = unit;
            dump_ast_impl();
        }
    }

    std::string& ASTDumper::get_output() {
        return m_output;
    }

    void ASTDumper::dump_ast_impl() {
        m_output += fmt::format("ModuleDecl '{}' {}\n", m_unit->parent->name, m_unit->filename);
        for (Decl* im : m_unit->imports) {
            dump_decl(im, 4);
        }

        for (Decl* td : m_unit->typedefs) {
            dump_decl(td, 4);
        }

        for (Decl* enu : m_unit->enums) {
            dump_decl(enu, 4);
        }

        for (Decl* stru : m_unit->structs) {
            dump_decl(stru, 4);
        }

        for (Decl* var : m_unit->globals) {
            dump_decl(var, 4);
        }

        for (Decl* func : m_unit->funcs) {
            dump_decl(func, 4);
        }
    }

    void ASTDumper::dump_expr(Expr* expr, size_t indentation) {
        std::string ident;
        ident.append(indentation, ' ');
        m_output += ident;

        if (expr == nullptr) { m_output += "<<NULL>>\n"; return; };

        switch (expr->kind) {
            case ExprKind::Error: m_output += fmt::format("ErrorExpr {} '{}' {}\n", 
                source_loc_to_string(expr->loc), type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind)); 
                return;

            case ExprKind::BooleanLiteral: m_output += fmt::format("BooleanLiteralExpr {} {} '{}' {}\n", 
                source_loc_to_string(expr->loc), expr->boolean_literal.value, 
                type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind)); 
                return;

            case ExprKind::CharacterLiteral: m_output += fmt::format("CharacterLiteralExpr {} 0x{:x} '{}' {}\n", 
                source_loc_to_string(expr->loc), expr->character_literal.value, 
                type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind)); 
                return;

            case ExprKind::IntegerLiteral: m_output += fmt::format("IntegerLiteralExpr {} {} '{}' {}\n", 
                source_loc_to_string(expr->loc), expr->integer_literal.value, 
                type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind)); 
                return;

            case ExprKind::FloatingLiteral: m_output += fmt::format("FloatingLiteralExpr {} {} '{}' {}\n", 
                source_loc_to_string(expr->loc), expr->floating_literal.value, 
                type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind)); 
                return;

            case ExprKind::StringLiteral: m_output += fmt::format("StringLiteralExpr {} {:?} '{}' {}\n", 
                source_loc_to_string(expr->loc), expr->string_literal.value, 
                type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind)); 
                return;

            case ExprKind::ArrayFiller: m_output += fmt::format("ArrayFillerExpr {} '{}' {}\n",
                source_loc_to_string(expr->loc), type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                dump_expr(expr->array_filler.initializer, indentation + 4);
                return;

            case ExprKind::Null: m_output += fmt::format("NullExpr {} '{}' {}\n", 
                source_loc_to_string(expr->loc), type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind)); 
                return;

            case ExprKind::DeclRef: m_output += fmt::format("DeclRefExpr {} '{}' {} {}{} '{}' {}\n", 
                source_loc_to_string(expr->loc), expr->decl_ref.identifier,
                decl_kind_to_string(expr->decl_ref.referenced_decl->kind), reinterpret_cast<void*>(expr->decl_ref.referenced_decl),
                expr->decl_ref.provides_generic_args ? " provides_generic_args" : "", 
                type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                for (TypeInfo* t : expr->decl_ref.generic_arguments) {
                    dump_type(t, indentation + 4);
                }

                if (expr->decl_ref.name_specifier) {
                    dump_specifier(expr->decl_ref.name_specifier, indentation + 4);
                }

                return;

            case ExprKind::TypeInfo: m_output += fmt::format("TypeInfoExpr {} '{}' '{}' {}\n", 
                source_loc_to_string(expr->loc), expr->type_info.type->to_string(),
                type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));
                return;

            case ExprKind::Member: m_output += fmt::format("MemberExpr {} '{}'{} {} {} '{}' {}\n",
                source_loc_to_string(expr->loc), expr->member.member, expr->member.implicit_deref ? " implicit_deref" : "",
                decl_kind_to_string(expr->member.referenced_member->kind), reinterpret_cast<void*>(expr->member.referenced_member),
                type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                dump_expr(expr->member.parent, indentation + 4);
                return;

            case ExprKind::BuiltinMember: m_output += fmt::format("BuiltinMemberExpr {} '{}'{} '{}' {}\n",
                source_loc_to_string(expr->loc), expr->member.member, expr->member.implicit_deref ? " implicit_deref" : "",
                type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                dump_expr(expr->member.parent, indentation + 4);
                return;

            case ExprKind::DependentMember: m_output += fmt::format("DependentMemberExpr {} '{}'{} '{}' {}\n",
                source_loc_to_string(expr->loc), expr->member.member, expr->member.implicit_deref ? " implicit_deref" : "",
                type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                dump_expr(expr->member.parent, indentation + 4);
                return;

            case ExprKind::TypeMember: m_output += fmt::format("TypeMemberExpr {} '{}' '{}' '{}' {}\n",
                source_loc_to_string(expr->loc), expr->type_member.member, expr->type_member.type->to_string(false),
                expr->type->to_string(false), expr_value_kind_to_string(expr->value_kind));
                return;

            case ExprKind::Self: m_output += fmt::format("SelfExpr {} '{}' {}\n", 
                source_loc_to_string(expr->loc), type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind)); 
                return;

            case ExprKind::Call: m_output += fmt::format("CallExpr {} '{}' {}\n",
                source_loc_to_string(expr->loc), type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                dump_expr(expr->call.callee, indentation + 4);
                for (Expr* arg : expr->call.arguments) {
                    dump_expr(arg, indentation + 4);
                }
                return;

            case ExprKind::BuiltinCall: m_output += fmt::format("BuiltinCallExpr {} {} '{}' {}\n",
                source_loc_to_string(expr->loc), builtin_call_kind_to_string(expr->builtin_call.kind),
                type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                for (Expr* arg : expr->builtin_call.arguments) {
                    dump_expr(arg, indentation + 4);
                }
                return;

            case ExprKind::Construct: m_output += fmt::format("ConstructExpr {} '{}' {}\n",
                source_loc_to_string(expr->loc), type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                for (Expr* arg : expr->construct.arguments) {
                    dump_expr(arg, indentation + 4);
                }
                return;

            case ExprKind::ArrayLiteral: m_output += fmt::format("ArrayLiteralExpr {} '{}' {}\n",
                source_loc_to_string(expr->loc), type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                for (Expr* arg : expr->array_literal.arguments) {
                    dump_expr(arg, indentation + 4);
                }
                return;

            case ExprKind::MethodCall: m_output += fmt::format("MethodCallExpr {} '{}' {}\n",
                source_loc_to_string(expr->loc), type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                dump_expr(expr->call.callee, indentation + 4);
                for (Expr* arg : expr->call.arguments) {
                    dump_expr(arg, indentation + 4);
                }
                return;

            case ExprKind::ArraySubscript: m_output += fmt::format("ArraySubscriptExpr {} '{}' {}\n",
                source_loc_to_string(expr->loc), type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                dump_expr(expr->array_subscript.array, indentation + 4);
                dump_expr(expr->array_subscript.index, indentation + 4);
                return;

            case ExprKind::ToSlice: m_output += fmt::format("ToSliceExpr {} '{}' {}\n",
                source_loc_to_string(expr->loc), type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                dump_expr(expr->to_slice.len, indentation + 4);
                dump_expr(expr->to_slice.source, indentation + 4);
                return;

            case ExprKind::Copy: m_output += fmt::format("CopyExpr {} '{}' {}\n",
                source_loc_to_string(expr->loc), type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                dump_expr(expr->copy.expression, indentation + 4);
                return;

            case ExprKind::Move: m_output += fmt::format("MoveExpr {} '{}' {}\n",
                source_loc_to_string(expr->loc), type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                dump_expr(expr->move.expression, indentation + 4);
                return;

            case ExprKind::Temporary: m_output += fmt::format("TemporaryExpr {} Destructor {} '{}' {}\n",
                source_loc_to_string(expr->loc), reinterpret_cast<void*>(expr->temporary.dtor), type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                dump_expr(expr->temporary.expression, indentation + 4);
                return;

            case ExprKind::MaterializeTemporary: m_output += fmt::format("MaterializeTemporaryExpr {} '{}' {}\n",
                source_loc_to_string(expr->loc), type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                dump_expr(expr->materialize_temporary.expression, indentation + 4);
                return;

            case ExprKind::ExprWithCleanups: m_output += fmt::format("ExprWithCleanups {} '{}' {}\n",
                source_loc_to_string(expr->loc), type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                dump_expr(expr->expr_with_cleanups.expression, indentation + 4);
                return;

            case ExprKind::DefaultArg: m_output += fmt::format("DefaultArgExpr '{}' {} Param {}\n",
                expr->type->to_string(), expr_value_kind_to_string(expr->value_kind), reinterpret_cast<void*>(expr->default_arg.parameter));

                dump_expr(expr->default_arg.argument, indentation + 4);
                return;

            case ExprKind::Paren: m_output += fmt::format("ParenExpr {} '{}' {}\n",
                source_loc_to_string(expr->loc), type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                dump_expr(expr->paren.expression, indentation + 4);
                return;

            case ExprKind::Ternary: m_output += fmt::format("TernaryExpr {} '{}' {}\n",
                source_loc_to_string(expr->loc), type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                dump_expr(expr->ternary.condition, indentation + 4);
                dump_expr(expr->ternary.first, indentation + 4);
                dump_expr(expr->ternary.second, indentation + 4);
                return;

            case ExprKind::Cast: m_output += fmt::format("CastExpr {} '{}' {}\n",
                source_loc_to_string(expr->loc), type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                dump_expr(expr->cast.expression, indentation + 4);
                return;

            case ExprKind::ImplicitCast: m_output += fmt::format("ImplicitCastExpr {} <{}> '{}' {}\n",
                source_loc_to_string(expr->loc), cast_kind_to_string(expr->implicit_cast.kind),
                type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                dump_expr(expr->implicit_cast.expression, indentation + 4);
                return;

            case ExprKind::UnaryOperator: m_output += fmt::format("UnaryOperatorExpr {} '{}' '{}' {}\n",
                source_loc_to_string(expr->loc), unary_op_kind_to_string(expr->unary_operator.op),
                type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                dump_expr(expr->unary_operator.expression, indentation + 4);
                return;

            case ExprKind::BinaryOperator: m_output += fmt::format("BinaryOperatorExpr {} '{}' '{}' {}\n",
                source_loc_to_string(expr->loc), binary_op_kind_to_string(expr->binary_operator.op),
                type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                dump_expr(expr->binary_operator.lhs, indentation + 4);
                dump_expr(expr->binary_operator.rhs, indentation + 4);
                return;

            case ExprKind::CompoundAssign: m_output += fmt::format("CompoundAssignExpr {} '{}' '{}' {}\n",
                source_loc_to_string(expr->loc), binary_op_kind_to_string(expr->compound_assign.op),
                type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                dump_expr(expr->compound_assign.lhs, indentation + 4);
                dump_expr(expr->compound_assign.rhs, indentation + 4);
                return;

            case ExprKind::Const: { m_output += fmt::format("ConstExpr {} '{}' {}\n",
                source_loc_to_string(expr->loc), type_info_to_string(expr->type), expr_value_kind_to_string(expr->value_kind));

                dump_const_expr_val(&expr->const_, indentation + 4);

                return;
            }

            default: ARIA_UNREACHABLE("Invalid expr kind");
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

            case DeclKind::Module: m_output += fmt::format("ModuleDecl '{}'\n",
                decl->module.name);
                return;

            case DeclKind::Import: m_output += fmt::format("ImportDecl {} '{}'\n",
                source_loc_to_string(decl->loc), decl->import.name);
                if (decl->import.parent) {
                    dump_decl(decl->import.parent, indentation + 4);
                }
                return;

            case DeclKind::Var: m_output += fmt::format("VarDecl {} {}{}{}'{}' '{}'\n",
                source_loc_to_string(decl->loc), decl->var.global_var ? "global " : "", decl->var.const_var ? "const " : "", decl->var.ref_var ? "reference " : "", decl->var.identifier, type_info_to_string(decl->var.type, false));
                if (decl->var.initializer) {
                    dump_expr(decl->var.initializer, indentation + 4);
                }
                return;

            case DeclKind::Param: m_output += fmt::format("ParamDecl {} '{}' '{}'{}\n",
                source_loc_to_string(decl->loc), decl->param.identifier, type_info_to_string(decl->param.type, false), decl->param.variadic ? " variadic" : "");

                if (decl->param.default_arg) {
                    dump_expr(decl->param.default_arg, indentation + 4);
                }
                return;

            case DeclKind::Function: m_output += fmt::format("FunctionDecl {} '{}' {} '{}'{}{}{}",
                source_loc_to_string(decl->loc), decl->function.identifier, decl_visibility_to_string(decl->visibility), 
                type_info_to_string(decl->function.type, false),
                (decl->function.linkage_kind != LinkageKind::None) ? fmt::format(" {}", linkage_kind_to_string(decl->function.linkage_kind)) : "",
                decl->function.is_const ? " const" : "", decl->function.is_deleted ? " deleted" : "");

                if (decl->function.is_specilization) {
                    m_output += decl->function.specilization_info.is_explicit ? " explicit_instantiation" : " implicit_instantiation";

                    if (!decl->function.specilization_info.is_explicit) {
                        m_output += fmt::format(" instantiated at {}", source_loc_to_string(decl->function.specilization_info.instantiation_loc));
                    }
                }

                m_output += '\n';

                if (decl->function.is_specilization) {
                    for (TypeInfo* t : decl->function.specilization_info.types) {
                        dump_type(t, indentation + 4);
                    }
                }

                dump_attributes(decl->attributes, indentation + 4);
                
                for (Decl* param : decl->function.type->function.params) {
                    dump_decl(param, indentation + 4);
                }

                if (decl->function.body) {
                    dump_stmt(decl->function.body, indentation + 4);
                }
                return;

            case DeclKind::FunctionOverloadSet: m_output += fmt::format("FunctionOverloadSetDecl {} '{}'\n",
                source_loc_to_string(decl->loc), decl->function_overload_set.identifier);

                ident += "    ";
                for (Decl* f : decl->function_overload_set.funcs) {
                    m_output += fmt::format("{}{}: {}\n", ident, decl_kind_to_string(f->kind), reinterpret_cast<void*>(f));
                }

                return;

            case DeclKind::Struct: m_output += fmt::format("StructDecl {} '{}'{}\n",
                source_loc_to_string(decl->loc), decl->struct_.identifier,
                decl->struct_.parent ? fmt::format(" parent {} {}", decl_kind_to_string(decl->struct_.parent->kind), 
                                                                   reinterpret_cast<void*>(decl->struct_.parent)) : "");

                for (Decl* field : decl->struct_.fields) {
                    dump_decl(field, indentation + 4);
                }
                return;

            case DeclKind::StructSpecilization: m_output += fmt::format("StructSpecilizationDecl {} instantiated at {}\n",
                source_loc_to_string(decl->loc), source_loc_to_string(decl->struct_specilization.instantiation_loc));
                for (TypeInfo* t : decl->struct_specilization.types) {
                    dump_type(t, indentation + 4);
                }

                dump_decl(decl->struct_specilization.source, indentation + 4);
                return;

            case DeclKind::Typedef: m_output += fmt::format("TypedefDecl {} '{}' '{}'\n",
                source_loc_to_string(decl->loc), type_info_to_string(decl->typedef_.type, false), decl->typedef_.identifier);

                dump_attributes(decl->attributes, indentation + 4);
                return;

            case DeclKind::Enum: m_output += fmt::format("EnumDecl {} '{}' '{}'\n",
                source_loc_to_string(decl->loc), decl->enum_.identifier, type_info_to_string(decl->enum_.backing_type, false));

                dump_attributes(decl->attributes, indentation + 4);

                for (Decl* field : decl->enum_.fields) {
                    dump_decl(field, indentation + 4);
                }

                return;

            case DeclKind::EnumConstant: m_output += fmt::format("EnumConstantDecl {} '{}' {}\n",
                source_loc_to_string(decl->loc), decl->enum_constant.identifier, decl->enum_constant.resolved_value);
                if (decl->enum_constant.value) {
                    dump_expr(decl->enum_constant.value, indentation + 4);
                }
                return;

            case DeclKind::Field: m_output += fmt::format("FieldDecl {} '{}' '{}' {}\n",
                source_loc_to_string(decl->loc), decl->field.identifier, type_info_to_string(decl->field.type, false), decl_visibility_to_string(decl->visibility));
                return;

            case DeclKind::Method: m_output += fmt::format("MethodDecl {} '{}' '{}'\n",
                source_loc_to_string(decl->loc), decl->method.identifier, type_info_to_string(decl->method.type, false));

                for (Decl* param : decl->method.type->function.params) {
                    dump_decl(param, indentation + 4);
                }

                dump_stmt(decl->method.body, indentation + 4);
                return;

            case DeclKind::Destructor: m_output += fmt::format("DestructorDecl {} '{}'\n",
                source_loc_to_string(decl->loc), decl->destructor.type->to_string(false));

                dump_stmt(decl->destructor.body, indentation + 4);
                return;

            case DeclKind::Template: m_output += fmt::format("TemplateDecl {}\n", source_loc_to_string(decl->loc));
                for (Decl* param : decl->template_.parameters) {
                    dump_decl(param, indentation + 4);
                }

                dump_decl(decl->template_.template_decl, indentation + 4);

                for (Decl* specilization : decl->template_.specilizations) {
                    // Skip explicit specilizations
                    if (specilization->kind == DeclKind::Function && specilization->function.specilization_info.is_explicit) {
                        continue;
                    }

                    dump_decl(specilization, indentation + 4);
                }

                return;

            case DeclKind::TemplateParam: m_output += fmt::format("TemplateParamDecl {} '{}'\n", 
                source_loc_to_string(decl->loc), decl->template_param.identifier);
                return;

            default: ARIA_UNREACHABLE("Invalid decl kind");
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
            case StmtKind::Error: m_output += fmt::format("ErrorStmt {}\n", source_loc_to_string(stmt->loc));
                return;

            case StmtKind::Nop: m_output += fmt::format("NopStmt {}\n", source_loc_to_string(stmt->loc));
                return;

            case StmtKind::Compound: m_output += fmt::format("CompoundStmt {}\n",
                source_loc_to_string(stmt->loc));
                for (Stmt* stmt : stmt->compound.stmts) {
                    dump_stmt(stmt, indentation + 4);
                }
                return;

            case StmtKind::While: m_output += fmt::format("WhileStmt {}{}\n",
                source_loc_to_string(stmt->loc), stmt->while_.infinite ? " infinite" : "");
                dump_expr(stmt->while_.condition, indentation + 4);
                dump_stmt(stmt->while_.body, indentation + 4);
                return;

            case StmtKind::DoWhile: m_output += fmt::format("DoWhileStmt {}{}\n",
                source_loc_to_string(stmt->loc), stmt->do_while.infinite ? " infinite" : "");
                dump_expr(stmt->do_while.condition, indentation + 4);
                dump_stmt(stmt->do_while.body, indentation + 4);
                return;

            case StmtKind::For: m_output += fmt::format("ForStmt {}{}\n",
                source_loc_to_string(stmt->loc), stmt->for_.infinite ? " infinite" : "");
                dump_decl(stmt->for_.prologue, indentation + 4);
                dump_expr(stmt->for_.condition, indentation + 4);
                dump_expr(stmt->for_.step, indentation + 4);
                dump_stmt(stmt->for_.body, indentation + 4);
                return;

            case StmtKind::If: m_output += fmt::format("IfStmt {}\n", source_loc_to_string(stmt->loc));
                dump_expr(stmt->if_.condition, indentation + 4);
                dump_stmt(stmt->if_.body, indentation + 4);

                if (stmt->if_.else_body) { dump_stmt(stmt->if_.else_body, indentation + 4); }
                return;

            case StmtKind::Switch: m_output += fmt::format("SwitchStmt {}\n", source_loc_to_string(stmt->loc));
                dump_expr(stmt->switch_.expression, indentation + 4);

                for (Stmt* c : stmt->switch_.cases) {
                    dump_stmt(c, indentation + 4);
                }
                return;

            case StmtKind::Case: m_output += fmt::format("CaseStmt {}\n", source_loc_to_string(stmt->loc));
                dump_expr(stmt->case_.condition, indentation + 4);
                dump_stmt(stmt->case_.body, indentation + 4);
                return;

            case StmtKind::Break: m_output += fmt::format("BreakStmt {} {}\n", source_loc_to_string(stmt->loc), reinterpret_cast<void*>(stmt->break_.target));
                return;

            case StmtKind::Continue: m_output += fmt::format("ContinueStmt {} {}\n", source_loc_to_string(stmt->loc), reinterpret_cast<void*>(stmt->continue_.target));
                return;

            case StmtKind::Nextcase: m_output += fmt::format("NextcaseStmt {} {}\n", source_loc_to_string(stmt->loc), reinterpret_cast<void*>(stmt->nextcase.target));
                return;

            case StmtKind::Return: m_output += fmt::format("ReturnStmt {}\n", source_loc_to_string(stmt->loc));
                if (stmt->return_.value) { dump_expr(stmt->return_.value, indentation + 4); }
                return;

            case StmtKind::Defer: m_output += fmt::format("DeferStmt {}\n", source_loc_to_string(stmt->loc));
                dump_stmt(stmt->defer.statement, indentation + 4);
                return;

            case StmtKind::CompileIf: m_output += fmt::format("CompileIfStmt {} unresolved\n", source_loc_to_string(stmt->loc));
                dump_expr(stmt->if_.condition, indentation + 4);
                return;

            case StmtKind::Assert: m_output += fmt::format("AssertStmt {}\n", source_loc_to_string(stmt->loc));
                dump_expr(stmt->assert_.condition, indentation + 4);
                for (Expr* arg : stmt->assert_.arguments) {
                    dump_expr(arg, indentation + 4);
                }
                return;

            case StmtKind::Unreachable: m_output += fmt::format("UnreachableStmt {}\n", source_loc_to_string(stmt->loc));
                for (Expr* arg : stmt->unreachable.arguments) {
                    dump_expr(arg, indentation + 4);
                }
                return;

            default: ARIA_UNREACHABLE("Invalid stmt kind");
        }
    }

    void ASTDumper::dump_specifier(Specifier* spec, size_t indentation) {
        if (spec == nullptr) { m_output += "<<NULL>>\n"; return; };

        std::string ident;
        ident.append(indentation, ' ');
        m_output += ident;

        if (spec->kind == SpecifierKind::Name) {
            m_output += fmt::format("NameSpecifier {} {} '{}'\n", source_loc_to_string(spec->loc), reinterpret_cast<void*>(spec->name.referenced_module), spec->name.identifier);
            if (spec->name.parent) { dump_specifier(spec->name.parent, indentation + 4); }
            return;
        }

        ARIA_UNREACHABLE("Invalid specifier kind");
    }

    void ASTDumper::dump_const_expr_val(ConstExpr* val, size_t indentation) {
        std::string ident;
        ident.append(indentation, ' ');
        m_output += ident;

        switch (val->kind) {
            case ConstExprKind::Error: m_output += "value: Error error\n"; break;
            case ConstExprKind::Bool: m_output += fmt::format("value: Bool {}\n", val->boolean); break;
            case ConstExprKind::Int: m_output += fmt::format("value: Int {}\n",val->integer); break;
            case ConstExprKind::Float: m_output += fmt::format("value: Float {}\n", val->number); break;
            case ConstExprKind::String: m_output += fmt::format("value: String {:?}\n", val->string); break;

            case ConstExprKind::Any: {
                m_output += fmt::format("value: Any '{}'\n", val->value->type->to_string());
                dump_const_expr_val(&val->value->const_, indentation + 4);
                break;
            }

            case ConstExprKind::Struct: {
                m_output += "value: Struct\n";

                for (size_t i = 0; i < val->values.size; i++) {
                    ARIA_ASSERT(val->values.items[i]->kind == ExprKind::Const, "Not a constant expression");
                    dump_const_expr_val(&val->values.items[i]->const_, indentation + 4);
                }

                break;
            }

            case ConstExprKind::Typeid: m_output += fmt::format("value: Typeid '{}'\n", type_info_to_string(val->type, false)); break;

            default: ARIA_ASSERT(false, static_cast<u64>(val->kind));
        }
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
                dump_expr(attr.expr, indentation + 4);
                return;
            }

            case DeclAttributeKind::Builtin: {
                m_output += fmt::format("BuiltinAttribute '{}'\n", attr.string);
                return;
            }

            case DeclAttributeKind::Init: {
                m_output += fmt::format("InitAttribute\n");
                return;
            }

            case DeclAttributeKind::Set: {
                m_output += fmt::format("SetAttribute {}\n", (attr.expr->kind == ExprKind::DeclRef) ? reinterpret_cast<void*>(attr.expr->decl_ref.referenced_decl) : nullptr);
                return;
            }

            default: ARIA_UNREACHABLE("Invalid attribute kind");
        }
    }

    void ASTDumper::dump_type(TypeInfo* type, size_t indentation) {
        std::string ident;
        ident.append(indentation, ' ');
        m_output += ident;

        if (type->is_error()) {
            m_output += "ErrorType '<error_type'>\n";
        } else if (type->is_primitive()) {
            m_output += fmt::format("PrimitiveType '{}'\n", type->to_string(false));
        } else if (type->is_pointer()) {
            m_output += fmt::format("PointerType '{}'\n", type->to_string(false));
            dump_type(type->pointer.base, indentation + 4);
        } else if (type->is_array()) {
            m_output += fmt::format("ArrayType {} '{}'\n", type->array.size, type->to_string(false));
            dump_type(type->array.base, indentation + 4);
        } else if (type->is_slice()) {
            m_output += fmt::format("SliceType '{}'\n", type->to_string(false));
            dump_type(type->slice.base, indentation + 4);
        } else if (type->is_function()) {
            m_output += fmt::format("FunctionType '{}'\n", type->to_string(false));
            dump_type(type->function.return_type, indentation + 4);

            for (Decl* p : type->function.params) {
                dump_decl(p, indentation + 4);
            }
        } else if (type->is_method()) {
            m_output += fmt::format("MethodType '{}'\n", type->to_string(false));
            dump_type(type->function.return_type, indentation + 4);

            for (Decl* p : type->function.params) {
                dump_decl(p, indentation + 4);
            }
        } else if (type->is_struct()) {
            m_output += fmt::format("StructType '{}'\n", type->to_string(false));
        } else if (type->is_typedef()) {
            m_output += fmt::format("TypedefType '{}'\n", type->to_string(false));
        } else if (type->is_enum()) {
            m_output += fmt::format("EnumType '{}'\n", type->to_string(false));
        } else if (type->is_struct_specilization()) {
            m_output += fmt::format("StructSpecilizationType '{}'\n", type->to_string(false));

            for (TypeInfo* t : type->struct_specilization.arguments) {
                dump_type(t, indentation + 4);
            }
        } else if (type->is_template()) {
            m_output += fmt::format("TemplateType '{}'\n", type->template_.identifier);
        } else {
            ARIA_UNREACHABLE("Invalid type kind");
        }
    }

    std::string ASTDumper::source_loc_to_string(SourceLoc loc) {
        // A hack to perform a 'defer', aria supports this but C++ for some reason doesn't
        struct SetPrevLoc {
            SetPrevLoc(SourceLoc l, SourceLoc* prev)
                : loc(l), prev(prev) {}
            ~SetPrevLoc() { *prev = loc; }

            SourceLoc loc;
            SourceLoc* prev;
        } _set_prev_loc(loc, &m_prev_loc);

        if (!loc.is_valid()) {
            return "<invalid_loc>";
        }

        if (!m_prev_loc.is_valid()) {
            return fmt::format("<line:{}, col:{}>", loc.line, loc.col);
        }

        if (loc.unit != m_prev_loc.unit) {
            return fmt::format("<file:{}, line:{}, col:{}", reinterpret_cast<CompilationUnit*>(loc.unit)->filename, loc.line, loc.col);
        }

        if (loc.line == m_prev_loc.line) {
            return fmt::format("<col:{}>", loc.col);
        }

        m_prev_loc = loc;
        return fmt::format("<line:{}, col:{}>", loc.line, loc.col);
    }

} // namespace ariac
