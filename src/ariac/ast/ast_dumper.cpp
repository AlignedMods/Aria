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

        for (Decl* gen : m_unit->generics) {
            dump_decl(gen, 4);
        }

        for (Decl* var : m_unit->globals) {
            dump_decl(var, 4);
        }

        for (Decl* impl : m_unit->impls) {
            dump_decl(impl, 4);
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

            case ExprKind::DeclRef: m_output += fmt::format("DeclRefExpr {} '{}' {} {} '{}' {}\n", 
                source_loc_to_string(expr->loc), expr->decl_ref.identifier,
                decl_kind_to_string(expr->decl_ref.referenced_decl->kind), reinterpret_cast<void*>(expr->decl_ref.referenced_decl),
                type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                if (expr->decl_ref.name_specifier) {
                    dump_specifier(expr->decl_ref.name_specifier, indentation + 4);
                }

                return;

            case ExprKind::Member: m_output += fmt::format("MemberExpr {} '{}'{} {} '{}' {}\n",
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

            case ExprKind::BuiltinCall: m_output += fmt::format("BuiltinCallExpr {} {} '{}' '{}' {}\n",
                source_loc_to_string(expr->loc), builtin_call_kind_to_string(expr->builtin_call.kind), 
                type_info_to_string(expr->builtin_call.type ? expr->builtin_call.type : expr->builtin_call.expression->type, false), 
                type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                if (expr->builtin_call.expression && !expr->builtin_call.type) { dump_expr(expr->builtin_call.expression, indentation + 4); }
                return;

            case ExprKind::IntrinsicCall: m_output += fmt::format("IntrinsicCallExpr {} {} '{}' {}\n",
                source_loc_to_string(expr->loc), intrinsic_call_kind_to_string(expr->intrinsic_call.kind),
                type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                for (Expr* arg : expr->intrinsic_call.arguments) {
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

            case ExprKind::Paren: m_output += fmt::format("ParenExpr {} '{}' {}\n",
                source_loc_to_string(expr->loc), type_info_to_string(expr->type, false), expr_value_kind_to_string(expr->value_kind));

                dump_expr(expr->paren.expression, indentation + 4);
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

            case ExprKind::UnaryOperator: m_output += fmt::format("UnaryOperatorExpr {} '{}' {} '{}' {}\n",
                source_loc_to_string(expr->loc), unary_op_kind_to_string(expr->unary_operator.op), expr->unary_operator.infix ? "infix" : "prefix",
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

            case ExprKind::Const: { m_output += fmt::format("ConstExpr {} '{}'\n",
                source_loc_to_string(expr->loc), type_info_to_string(expr->type));

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

            case DeclKind::Import: m_output += fmt::format("ImportDecl {} '{}'{}{}\n",
                source_loc_to_string(decl->loc), decl->import.name, decl->import.alias.empty() ? "" : fmt::format(" as '{}'", decl->import.alias), decl->import.implicit ? " implicit" : "");
                return;

            case DeclKind::Var: m_output += fmt::format("VarDecl {} {}{}'{}' '{}'\n",
                source_loc_to_string(decl->loc), decl->var.global_var ? "global " : "", decl->var.const_var ? "const " : "", decl->var.identifier, type_info_to_string(decl->var.type, false));
                if (decl->var.initializer) {
                    dump_expr(decl->var.initializer, indentation + 4);
                }
                return;

            case DeclKind::Param: m_output += fmt::format("ParamDecl {} '{}' '{}'{}\n",
                source_loc_to_string(decl->loc), decl->param.identifier, type_info_to_string(decl->param.type, false), decl->param.variadic ? " variadic" : "");
                return;

            case DeclKind::Function: m_output += fmt::format("FunctionDecl {} '{}' {} '{}' {}\n",
                source_loc_to_string(decl->loc), decl->function.identifier, decl_visibility_to_string(decl->visibility), type_info_to_string(decl->function.type, false), linkage_kind_to_string(decl->function.linkage_kind));

                dump_attributes(decl->attributes, indentation + 4);
                
                for (Decl* param : decl->function.parameters) {
                    dump_decl(param, indentation + 4);
                }

                if (decl->function.body) {
                    dump_stmt(decl->function.body, indentation + 4);
                }
                return;

            case DeclKind::FunctionSpecilization: m_output += fmt::format("FunctionSpecilizationDecl {} ", source_loc_to_string(decl->loc));
                for (size_t i = 0; i < decl->function_specilization.types.size; i++) {
                    m_output += fmt::format("'{}' ", type_info_to_string(decl->function_specilization.types.items[i]));
                }
                m_output += '\n';

                dump_decl(decl->function_specilization.source, indentation + 4);
                return;

            case DeclKind::Struct: m_output += fmt::format("StructDecl {} '{}'\n",
                source_loc_to_string(decl->loc), decl->struct_.identifier);

                for (Decl* field : decl->struct_.fields) {
                    dump_decl(field, indentation + 4);
                }

                return;

            case DeclKind::StructSpecilization: m_output += fmt::format("StructSpecilizationDecl {} ", source_loc_to_string(decl->loc));
                for (size_t i = 0; i < decl->struct_specilization.types.size; i++) {
                    m_output += fmt::format("'{}' ", type_info_to_string(decl->struct_specilization.types.items[i]));
                }
                m_output += '\n';

                dump_decl(decl->struct_specilization.source, indentation + 4);

                for (Decl* impl : decl->struct_specilization.impls) {
                    dump_decl(impl, indentation + 4);
                }

                return;

            case DeclKind::Impl: m_output += fmt::format("ImplDecl {} '{}'\n",
                source_loc_to_string(decl->loc), decl->impl.identifier);

                for (Decl* field : decl->impl.fields) {
                    dump_decl(field, indentation + 4);
                }

                return;

            case DeclKind::Typedef: m_output += fmt::format("TypedefDecl {} '{}' '{}'\n",
                source_loc_to_string(decl->loc), type_info_to_string(decl->typedef_.type, false), decl->typedef_.identifier);

                dump_attributes(decl->attributes, indentation + 4);
                return;

            case DeclKind::Enum: m_output += fmt::format("EnumDecl {} '{}'\n",
                source_loc_to_string(decl->loc), decl->enum_.identifier);

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

                for (Decl* param : decl->method.parameters) {
                    dump_decl(param, indentation + 4);
                }

                dump_stmt(decl->method.body, indentation + 4);
                return;

            case DeclKind::Generic: m_output += fmt::format("GenericDecl {}\n", source_loc_to_string(decl->loc));
                for (Decl* param : decl->generic.parameters) {
                    dump_decl(param, indentation + 4);
                }

                dump_decl(decl->generic.decl, indentation + 4);

                for (Decl* specilization : decl->generic.specilizations) {
                    dump_decl(specilization, indentation + 4);
                }

                return;

            case DeclKind::GenericParameter: m_output += fmt::format("GenericParameterDecl {} '{}'{} ", source_loc_to_string(decl->loc), decl->generic_parameter.identifier,
                decl->generic_parameter.variadic ? " variadic" : "");
                for (size_t i = 0; i < decl->generic_parameter.requirements.size; i++) {
                     if (i < 0) { m_output += ", "; }
                     m_output += generic_requirement_kind_to_string(decl->generic_parameter.requirements.items[i].kind);
                }
                m_output += '\n';

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

            case StmtKind::Block: m_output += fmt::format("BlockStmt {}{}\n",
                source_loc_to_string(stmt->loc), stmt->block.reaches_end ? " reaches_end" : "");
                for (Stmt* stmt : stmt->block.stmts) {
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

            case StmtKind::Break: m_output += fmt::format("BreakStmt {}\n", source_loc_to_string(stmt->loc));
                return;

            case StmtKind::Continue: m_output += fmt::format("ContinueStmt {}\n", source_loc_to_string(stmt->loc));
                return;

            case StmtKind::Return: m_output += fmt::format("ReturnStmt {}\n", source_loc_to_string(stmt->loc));
                if (stmt->return_.value) { dump_expr(stmt->return_.value, indentation + 4); }
                return;

            case StmtKind::Defer: m_output += fmt::format("DeferStmt {}\n", source_loc_to_string(stmt->loc));
                dump_stmt(stmt->defer.statement, indentation + 4);
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
            case ConstExprKind::Boolean: m_output += fmt::format("value: Boolean {}\n", val->boolean); break;
            case ConstExprKind::Integer: m_output += fmt::format("value: Integer {}\n",val->integer); break;
            case ConstExprKind::Floating: m_output += fmt::format("value: Floating {}\n", val->number); break;
            case ConstExprKind::String: m_output += fmt::format("value: String {:?}\n", val->string); break;
            case ConstExprKind::Struct: {
                m_output += "value: Struct\n";

                for (size_t i = 0; i < val->values.size; i++) {
                    ARIA_ASSERT(val->values.items[i]->kind == ExprKind::Const, "Not a constant expression");
                    dump_const_expr_val(&val->values.items[i]->const_, indentation + 4);
                }

                break;
            }

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

            case DeclAttributeKind::Noreturn: {
                m_output += "NoreturnAttribute\n";
                return;
            }

            default: ARIA_UNREACHABLE("Invalid attribute kind");
        }
    }

    std::string ASTDumper::source_loc_to_string(SourceLoc loc) {
        if (!loc.is_valid()) {
            m_prev_loc = loc;
            return "<invalid_loc>";
        }

        if (loc.line == m_prev_loc.line) {
            m_prev_loc = loc;
            return fmt::format("<col:{}>", loc.col);
        }

        m_prev_loc = loc;
        return fmt::format("<line:{}, col:{}>", loc.line, loc.col);
    }

} // namespace ariac
