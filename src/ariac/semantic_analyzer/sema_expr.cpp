#include "ariac/semantic_analyzer/semantic_analyzer.hpp"
#include "ariac/core/scratch_buffer.hpp"

namespace Aria::Internal {

    void SemanticAnalyzer::resolve_boolean_literal_expr(Expr* expr) {
        if (expr->result_discarded) {
            m_context->report_compiler_diagnostic(expr->loc, expr->range, "Discarding result of boolean literal", CompilerDiagKind::Warning);
        }
    }

    void SemanticAnalyzer::resolve_character_literal_expr(Expr* expr) {
        if (expr->result_discarded) {
            m_context->report_compiler_diagnostic(expr->loc, expr->range, "Discarding result of character literal", CompilerDiagKind::Warning);
        }
    }

    void SemanticAnalyzer::resolve_integer_literal_expr(Expr* expr) {
        if (expr->result_discarded) {
            m_context->report_compiler_diagnostic(expr->loc, expr->range, "Discarding result of integer literal", CompilerDiagKind::Warning);
        }
    }

    void SemanticAnalyzer::resolve_floating_literal_expr(Expr* expr) {
        if (expr->result_discarded) {
            m_context->report_compiler_diagnostic(expr->loc, expr->range, "Discarding result of floating point literal", CompilerDiagKind::Warning);
        }
    }

    void SemanticAnalyzer::resolve_string_literal_expr(Expr* expr) {
        if (expr->result_discarded) {
            m_context->report_compiler_diagnostic(expr->loc, expr->range, "Discarding result of string literal is not allowed");
        }
    }

    void SemanticAnalyzer::resolve_null_expr(Expr* expr) {
        if (expr->result_discarded) {
            m_context->report_compiler_diagnostic(expr->loc, expr->range, "Discarding result of null", CompilerDiagKind::Warning);
        }
    }

    void SemanticAnalyzer::resolve_decl_ref_expr(Expr* expr) {
        DeclRefExpr& ref = expr->decl_ref;
        std::string ident = fmt::format("{}", ref.identifier);

        auto getType = [](Decl* d) -> TypeInfo* {
            switch (d->kind) {
                case DeclKind::Var: { return d->var.type; }
                case DeclKind::Param: { return d->param.type; }
                case DeclKind::Function: { return d->function.type; }
                case DeclKind::OverloadedFunction: { return &error_type; }
                case DeclKind::Struct: { return &error_type; }

                default: ARIA_UNREACHABLE();
            }      
        };

        if (expr->result_discarded) {
            m_context->report_compiler_diagnostic(expr->loc, expr->range, "Discarding result of identifier", CompilerDiagKind::Warning);
        }

        Module* mod = nullptr;

        if (!ref.name_specifier) {
            for (auto& scope : m_scopes) {
                if (scope.declarations.contains(ident)) {
                    Decl* d = scope.declarations.at(ident).source_decl;
                    expr->type = getType(d);
                    ref.referenced_decl = d;
                    return;
                }
            }
        }

        if (ref.name_specifier) {
            ARIA_ASSERT(ref.name_specifier->kind == SpecifierKind::Scope, "Invalid specifier");

            // We may be referencing ourselves
            if (compare_module_names(ref.name_specifier->scope.identifier, m_context->active_comp_unit->parent->name)) {
                mod = m_context->active_comp_unit->parent;
            }

            for (Stmt* import : m_context->active_comp_unit->imports) {
                ARIA_ASSERT(import->kind == StmtKind::Import, "Invalid import stmt");

                if (compare_module_names(ref.name_specifier->scope.identifier, import->import.name)) {
                    mod = import->import.resolved_module;
                    break;
                }
            }

            if (!mod) {
                m_context->report_compiler_diagnostic(ref.name_specifier->loc, ref.name_specifier->range, fmt::format("Could not find module '{}'", ref.name_specifier->scope.identifier));
                expr->type = &error_type;
                ref.referenced_decl = &error_decl;
                return;
            } else {
                ref.name_specifier->scope.referenced_module = mod;
            }
        } else {
            mod = m_context->active_comp_unit->parent;
        }

        if (m_context->active_comp_unit->local_symbols.contains(ident)) {
            Decl* d = m_context->active_comp_unit->local_symbols.at(ident);
            ref.referenced_decl = d;
            expr->type = getType(d);
            return;
        }

        if (mod->symbols.contains(ident)) {
            Decl* d = mod->symbols.at(ident);

            ref.referenced_decl = d;
            expr->type = getType(d);
            return;
        }

        for (Stmt* import : m_context->active_comp_unit->imports) {
            ARIA_ASSERT(import->kind == StmtKind::Import, "Invalid import");

            if (!import->import.resolved_module) { continue; }

            if (import->import.resolved_module->symbols.contains(ident)) {
                m_context->report_compiler_diagnostic_with_notes(expr->loc, expr->range, 
                    "Symbols from other modules must be prefixed with the module name",
                    { fmt::format("Did you mean to write '{}::{}'?", import->import.name, ref.identifier) });
                expr->type = &error_type;
                ref.referenced_decl = &error_decl;
                return;
            }
        }

        expr->type = &error_type;
        ref.referenced_decl = &error_decl;
        m_context->report_compiler_diagnostic(expr->loc, expr->range, fmt::format("Unknown identifier '{}'", ref.identifier));
    }

    void SemanticAnalyzer::resolve_member_expr(Expr* expr) {
        MemberExpr& mem = expr->member;

        resolve_expr(mem.parent);
        TypeInfo* parentType = mem.parent->type;
        TypeInfo* memberType = nullptr;

        switch (parentType->kind) {
            case TypeKind::Structure: {
                StructDeclaration& sd = parentType->struct_;

                StructDecl s = sd.source_decl->struct_;

                for (Decl* field : s.fields) {
                    if (field->kind == DeclKind::Field) {
                        FieldDecl fd = field->field;
                        if (fd.identifier == mem.member) {
                            memberType = fd.type;
                        }
                    } else {
                        ARIA_UNREACHABLE();
                    }
                }

                break;
            }

            case TypeKind::Slice: {
                if (mem.member == "mem") {
                    memberType = TypeInfo::Create(m_context, TypeKind::Ptr, false);
                    memberType->base = parentType->base;
                    expr->kind = ExprKind::BuiltinMember;
                } else if (mem.member == "len") {
                    memberType = &ulong_type;
                    expr->kind = ExprKind::BuiltinMember;
                }

                break;
            }

            default: {
                m_context->report_compiler_diagnostic(mem.parent->loc, mem.parent->range, fmt::format("Expression must be of slice or struct type but is '{}'", type_info_to_string(parentType)));
                expr->type = &error_type;
                return;
            }
        }

        if (!memberType) {
            m_context->report_compiler_diagnostic(expr->loc, expr->range, fmt::format("Unknown member \"{}\" in '{}'", mem.member, type_info_to_string(parentType)));
            expr->type = &error_type;
            return;
        }

        expr->type = memberType;

        if (expr->result_discarded) {
            m_context->report_compiler_diagnostic(expr->loc, expr->range, "Discarding result of member access", CompilerDiagKind::Warning);
        }
    }

    void SemanticAnalyzer::resolve_call_expr(Expr* expr) {
        CallExpr& call = expr->call;

        resolve_expr(call.callee);
        TypeInfo* calleeType = call.callee->type;

        if (calleeType->kind != TypeKind::Function && !calleeType->is_error()) {
            m_context->report_compiler_diagnostic(expr->loc, expr->range, "Cannot call an object of non-function type");
            expr->type = &error_type;
            return;
        }

        if (call.callee->decl_ref.referenced_decl->kind != DeclKind::Error) {
            if (call.callee->kind == ExprKind::DeclRef && call.callee->decl_ref.referenced_decl->kind == DeclKind::OverloadedFunction) { // Overloaded function
                m_temporary_context = true;
                for (Expr* arg : call.arguments) {
                    resolve_expr(arg);
                }
                m_temporary_context = false;

                Decl* resolved = nullptr;
                Module* mod = call.callee->decl_ref.name_specifier ? call.callee->decl_ref.name_specifier->scope.referenced_module : m_context->active_comp_unit->parent;

                for (Decl* func : mod->overloaded_funcs.at(fmt::format("{}", call.callee->decl_ref.identifier))) {
                    for (size_t i = 0; i < func->function.type->function.param_types.size; i++) {
                        if (!type_is_equal(func->function.type->function.param_types.items[i], call.arguments.items[i]->type)) { goto again; }
                    }

                    goto done;

                done:
                    resolved = func;
                    break;

                again:
                    continue;
                }

                if (!resolved) {
                    m_context->report_compiler_diagnostic(expr->loc, expr->range, fmt::format("No matching overloaded function '{}' to call", call.callee->decl_ref.identifier));
                    for (size_t i = 0; i < call.arguments.size; i++) {
                        call.arguments.items[i]->type = &error_type;
                    }
                }

                call.callee->decl_ref.referenced_decl = resolved;
                expr->type = resolved->function.type->function.return_type;
                return;
            } else if (!call.callee->type->is_error()) { // Normal function
                FunctionDeclaration& fnDecl = calleeType->function;

                for (auto& attr : call.callee->decl_ref.referenced_decl->function.attributes) {
                    if (attr.kind == FunctionDecl::AttributeKind::Unsafe && !m_unsafe_context) {
                        m_context->report_compiler_diagnostic(expr->loc, expr->range, "Cannot call unsafe function in safe context");
                    }
                }

                if (fnDecl.param_types.size != call.arguments.size) {
                    m_context->report_compiler_diagnostic(expr->loc, expr->range, fmt::format("Mismatched argument count, function expects {} but got {}", fnDecl.param_types.size, call.arguments.size));
                    for (size_t i = 0; i < call.arguments.size; i++) {
                        call.arguments.items[i]->type = &error_type;
                    }
                } else {
                    for (size_t i = 0; i < fnDecl.param_types.size; i++) {
                        resolve_param_initializer(fnDecl.param_types.items[i], call.arguments.items[i]);
                    }
                }

                expr->type = fnDecl.return_type;
                expr->value_kind = (fnDecl.return_type->is_reference()) ? ExprValueKind::LValue : ExprValueKind::RValue;

                // We may need to create a temporary if a function returns a non-trivial type and it is discarded
                if (expr->result_discarded && !fnDecl.return_type->is_reference()) {
                    if (fnDecl.return_type->is_string()) {
                        replace_expr(expr, Expr::Create(m_context, expr->loc, expr->range, ExprKind::Temporary,
                            ExprValueKind::RValue, expr->type,
                            TemporaryExpr(Expr::Dup(m_context, expr), m_builtin_string_destructor)));
                    }
                }

                return;
            }
        } else {
            for (Expr* arg : call.arguments) {
                resolve_expr(arg);
            }
        }

        expr->type = &error_type;
    }

    void SemanticAnalyzer::resolve_array_subscript_expr(Expr* expr) {
        ArraySubscriptExpr& subs = expr->array_subscript;

        resolve_expr(subs.array);
        resolve_expr(subs.index);
        require_rvalue(subs.index);

        if (subs.array->type->is_error()) { expr->type = &error_type; return; }

        switch (subs.array->type->kind) {
            case TypeKind::Ptr: {
                require_rvalue(subs.array);
                expr->type = subs.array->type->base;
                break;
            }

            case TypeKind::Slice: {
                expr->type = subs.array->type->base;
                break;
            }

            case TypeKind::Array: {
                expr->type = subs.array->type->array.type;
                break;
            }

            default: m_context->report_compiler_diagnostic(subs.array->loc, subs.array->range, "'[' operator can only be used with a pointer/slice/array"); break;
        }

        ConversionCost cost = get_conversion_cost(&ulong_type, subs.index->type);
        if (cost.cast_needed) {
            if (cost.implicit_cast_possible) {
                insert_implicit_cast(&ulong_type, subs.index->type, subs.index, cost.kind);
            } else {
                m_context->report_compiler_diagnostic(subs.index->loc, subs.index->range, fmt::format("Array index cannot be implicitly converted from '{}' to 'ulong'", type_info_to_string(subs.index->type)));
            }
        }
    }

    void SemanticAnalyzer::resolve_to_slice_expr(Expr* expr) {
        ToSliceExpr& tos = expr->to_slice;

        resolve_expr(tos.source);
        resolve_expr(tos.len);
        require_rvalue(tos.len);

        if (tos.source->type->is_error()) { expr->type = &error_type; return; }

        switch (tos.source->type->kind) {
            case TypeKind::Ptr: {
                require_rvalue(tos.source);
                expr->type = TypeInfo::Create(m_context, TypeKind::Slice, false);
                expr->type->base = tos.source->type->base;
                break;
            }

            case TypeKind::Slice: {
                ARIA_TODO("slice to slice");
                // require_rvalue(subs.Array);
                // expr->type = subs.Array->type->Base;
                // break;
            }

            case TypeKind::Array: {
                ARIA_TODO("array to slice");
                // expr->type = subs.Array->type->Array.Type;
                // break;
            }

            default: m_context->report_compiler_diagnostic(tos.source->loc, tos.source->range, "Only a pointer/slice/array can be converted to a slice"); expr->type = &error_type; break;
        }

        ConversionCost cost = get_conversion_cost(&ulong_type, tos.len->type);
        if (cost.cast_needed) {
            if (cost.implicit_cast_possible) {
                insert_implicit_cast(&ulong_type, tos.len->type, tos.len, cost.kind);
            } else {
                m_context->report_compiler_diagnostic(tos.len->loc, tos.len->range, fmt::format("Slice length cannot be implicitly converted from '{}' to 'ulong'", type_info_to_string(tos.len->type)));
            }
        }
    }

    void SemanticAnalyzer::resolve_new_expr(Expr* expr) {
        NewExpr& n = expr->new_;
        
        if (expr->result_discarded) {
            m_context->report_compiler_diagnostic(expr->loc, expr->range, "Discarding result of 'new' expression is not allowed");
        }

        if (!m_unsafe_context) {
            m_context->report_compiler_diagnostic(expr->loc, expr->range, "'new' is only allowed in unsafe context");
        }

        if (!n.initializer) {
            create_default_initializer(&n.initializer, expr->type->base, expr->loc, expr->range);
        } else {
            if (n.array) {
                m_temporary_context = true;
                resolve_expr(n.initializer);
                require_rvalue(n.initializer);
                m_temporary_context = false;

                ConversionCost cost = get_conversion_cost(&ulong_type, n.initializer->type);
                if (cost.cast_needed) {
                    if (cost.implicit_cast_possible) {
                        insert_implicit_cast(&ulong_type, n.initializer->type, n.initializer, cost.kind);
                    } else {
                        m_context->report_compiler_diagnostic(n.initializer->loc, n.initializer->range, fmt::format("Cannot implicitly convert from '{}' to '{}'", type_info_to_string(n.initializer->type), type_info_to_string(&ulong_type)));
                    }
                }
            } else {
                m_temporary_context = true;
                resolve_expr(n.initializer);
                require_rvalue(n.initializer);
                m_temporary_context = false;

                ConversionCost cost = get_conversion_cost(expr->type->base, n.initializer->type);
                if (cost.cast_needed) {
                    if (cost.implicit_cast_possible) {
                        insert_implicit_cast(expr->type->base, n.initializer->type, n.initializer, cost.kind);
                    } else {
                        m_context->report_compiler_diagnostic(n.initializer->loc, n.initializer->range, fmt::format("Cannot implicitly convert from '{}' to '{}'", type_info_to_string(n.initializer->type), type_info_to_string(expr->type->base)));
                    }
                }
            }
        }
    }

    void SemanticAnalyzer::resolve_delete_expr(Expr* expr) {
        DeleteExpr& d = expr->delete_;

        if (!m_unsafe_context) {
            m_context->report_compiler_diagnostic(expr->loc, expr->range, "'delete' is only allowed in unsafe context");
        }

        resolve_expr(d.expression);
        require_rvalue(d.expression);

        if (!d.expression->type->is_pointer()) {
            m_context->report_compiler_diagnostic(expr->loc, expr->range, "'delete' can only be used with pointer types");
        }
    }

    void SemanticAnalyzer::resolve_format_expr(Expr* expr) {
        ARIA_TODO("SemanticAnalyzer::resolve_Format_expr()");
        // FormatExpr& format = expr->Format;
        // 
        // if (format.Args.Size == 0) {
        //     m_context->report_compiler_diagnostic(expr->loc, expr->range, "Format expression must have a format string");
        //     return;
        // }
        // 
        // if (format.Args.Items[0]->kind != ExprKind::StringConstant) {
        //     m_context->report_compiler_diagnostic(format.Args.Items[0]->loc, format.Args.Items[0]->range, "Format string must be a string literal");
        //     return;
        // }
        // 
        // if (expr->ResultDiscarded) {
        //     m_context->report_compiler_diagnostic(expr->loc, expr->range, "Discarding result of format is not allowed");
        //     return;
        // }
        // 
        // for (Expr* arg : format.Args) {
        //     m_TemporaryContext = true;
        //     resolve_expr(arg);
        // 
        //     require_rvalue(arg);
        //     m_TemporaryContext = false;
        // }
        // 
        // scratch_buffer_clear();
        // TinyVector<FormatExpr::FormatArg> formattedArgs;
        // std::string_view fmtStr = format.Args.Items[0]->Temporary.Expression->StringConstant.Value;
        // 
        // size_t idx = 0;
        // bool needsClosing = false;
        // 
        // for (size_t i = 0; i < fmtStr.length(); i++) {
        //     if (needsClosing) {
        //         if (fmtStr.at(i) == '}') {
        //             needsClosing = false;
        //             continue;
        //         } else {
        //             m_context->report_compiler_diagnostic(expr->loc, expr->range, "Missing closing curly brace in format string");
        //             return;
        //         }
        //     }
        // 
        //     if (fmtStr.at(i) == '{') {
        //         if (scratch_buffer_size() > 0) {
        //             StringBuilder tmpB;
        //             tmpB.Append(m_context, buf);
        //             buf.Clear();
        // 
        //             Expr* str = Expr::Create(m_context, expr->loc, expr->range, ExprKind::StringConstant,
        //                 ExprValueKind::RValue, &StringType,
        //                 StringConstantExpr(tmpB));
        // 
        //             formattedArgs.Append(m_context, { Expr::Create(m_context, expr->loc, expr->range, ExprKind::Temporary,
        //                 ExprValueKind::RValue, &StringType,
        //                 TemporaryExpr(str, m_BuiltInStringDestructor)) });
        //         }
        //         
        //         if (idx + 1 >= format.Args.Size) {
        //             m_context->report_compiler_diagnostic(expr->loc, expr->range, "Format string specifies more arguments than are provided");
        //             return;
        //         }
        // 
        //         formattedArgs.Append(m_context, { format.Args.Items[idx + 1] });
        //         needsClosing = true;
        //         idx++;
        //     } else {
        //         buf.Append(m_context, fmtStr.At(i));
        //     }
        // }
        // 
        // if (buf.Size() > 0) {
        //     StringBuilder tmpB;
        //     tmpB.Append(m_context, buf);
        //     buf.Clear();
        // 
        //     Expr* str = Expr::Create(m_context, expr->loc, expr->range, ExprKind::StringConstant,
        //         ExprValueKind::RValue, &StringType,
        //         StringConstantExpr(tmpB));
        // 
        //     formattedArgs.Append(m_context, { Expr::Create(m_context, expr->loc, expr->range, ExprKind::Temporary,
        //         ExprValueKind::RValue, &StringType,
        //         TemporaryExpr(str, m_BuiltInStringDestructor)) });
        // }
        // 
        // format.ResolvedArgs = formattedArgs;
        // if (m_TemporaryContext) {
        //     replace_expr(expr, Expr::Create(m_context, expr->loc, expr->range, ExprKind::Temporary,
        //         ExprValueKind::RValue, expr->type,
        //         TemporaryExpr(Expr::Dup(m_context, expr), m_BuiltInStringDestructor)));
        // }
    }

    void SemanticAnalyzer::resolve_paren_expr(Expr* expr) {
        ParenExpr& paren = expr->paren;
        resolve_expr(paren.expression);

        expr->type = paren.expression->type;
        expr->value_kind = paren.expression->value_kind;

        if (expr->result_discarded) {
            m_context->report_compiler_diagnostic(expr->loc, expr->range, "Discarding result of expression", CompilerDiagKind::Warning);
        }
    }

    void SemanticAnalyzer::resolve_cast_expr(Expr* expr) {
        CastExpr& cast = expr->cast;
        
        resolve_expr(cast.expression);
        require_rvalue(cast.expression);
        expr->type = cast.type;

        TypeInfo* srcType = cast.expression->type;
        TypeInfo* dstType = cast.type;

        ConversionCost cost = get_conversion_cost(dstType, srcType);

        if (cost.cast_needed) {
            if (cost.explicit_cast_possible) {
                insert_implicit_cast(dstType, srcType, cast.expression, cost.kind);
            } else {
                m_context->report_compiler_diagnostic(expr->loc, expr->range, fmt::format("Cannot cast '{}' to '{}'", type_info_to_string(srcType), type_info_to_string(dstType)));
            }
        }

        if (expr->result_discarded) {
            m_context->report_compiler_diagnostic(expr->loc, expr->range, "Discarding result of explicit cast", CompilerDiagKind::Warning);
        }
    }

    void SemanticAnalyzer::resolve_unary_operator_expr(Expr* expr) {
        UnaryOperatorExpr& unop = expr->unary_operator;
        
        resolve_expr(unop.expression);
        TypeInfo* type = unop.expression->type;
        
        switch (unop.op) {
            case UnaryOperatorKind::Negate: {
                require_rvalue(unop.expression);
                ARIA_ASSERT(type->is_numeric(), "todo: add error message");

                if (type->is_integral()) {
                    if (type->is_unsigned()) {
                        m_context->report_compiler_diagnostic(expr->loc, expr->range, fmt::format("Cannot negate expression of unsigned type '{}'", type_info_to_string(type)));
                    }
                }

                expr->type = type;
                break;
            }

            case UnaryOperatorKind::AddressOf: {
                if (type->is_error()) { expr->type = type; break; }

                if (!m_unsafe_context) {
                    m_context->report_compiler_diagnostic(expr->loc, expr->range, "Address of operation ('&') must be in an unsafe context");
                }

                if (unop.expression->value_kind != ExprValueKind::LValue) {
                    m_context->report_compiler_diagnostic(expr->loc, expr->range, "Address of operation ('&') requries an lvalue");
                }

                TypeInfo* newType = TypeInfo::Create(m_context, TypeKind::Ptr, false);
                newType->base = type;
                expr->type = newType;
                break;
            }

            case UnaryOperatorKind::Dereference: {
                if (type->is_error()) { expr->type = type; break; }

                if (!m_unsafe_context) {
                    m_context->report_compiler_diagnostic(expr->loc, expr->range, "Dereferencing of pointer must be in an unsafe context");
                }

                if (unop.expression->value_kind != ExprValueKind::LValue) {
                    m_context->report_compiler_diagnostic(expr->loc, expr->range, "Dereferencing requires an lvalue");
                }

                require_rvalue(unop.expression);

                if (type->is_pointer()) {
                    if (type->base->is_void()) {
                        m_context->report_compiler_diagnostic(expr->loc, expr->range, "Cannot dereference a void*");
                    }
                } else {
                    m_context->report_compiler_diagnostic(expr->loc, expr->range, "Dereferencing requires a pointer type");
                    break;
                }

                expr->type = type->base;
                expr->value_kind = ExprValueKind::LValue;
                break;
            }

            default: ARIA_UNREACHABLE();
        }
    }

    void SemanticAnalyzer::resolve_binary_operator_expr(Expr* expr) {
        BinaryOperatorExpr& binop = expr->binary_operator;

        resolve_expr(binop.lhs);
        resolve_expr(binop.rhs);

        Expr* LHS = binop.lhs;
        Expr* RHS = binop.rhs;

        switch (binop.op) {
            case BinaryOperatorKind::Add:
            case BinaryOperatorKind::Sub:
            case BinaryOperatorKind::Mul:
            case BinaryOperatorKind::Div:
            case BinaryOperatorKind::Mod:
            case BinaryOperatorKind::Less:
            case BinaryOperatorKind::LessOrEq:
            case BinaryOperatorKind::Greater:
            case BinaryOperatorKind::GreaterOrEq:
            case BinaryOperatorKind::IsEq: 
            case BinaryOperatorKind::IsNotEq: {
                if (!LHS->type->is_error()) {
                    if (!LHS->type->is_numeric()) {
                        m_context->report_compiler_diagnostic(LHS->loc, LHS->range, fmt::format("Expression must be of a numeric type but is of type '{}'", type_info_to_string(LHS->type)));
                    }

                    if (!LHS->type->is_numeric()) {
                        m_context->report_compiler_diagnostic(RHS->loc, RHS->range, fmt::format("Expression must be of a numeric type but is of type '{}'", type_info_to_string(RHS->type)));
                    }
                }

                insert_arithmetic_promotion(LHS, RHS);

                if (binop.op == BinaryOperatorKind::Less ||
                    binop.op == BinaryOperatorKind::LessOrEq ||
                    binop.op == BinaryOperatorKind::Greater ||
                    binop.op == BinaryOperatorKind::GreaterOrEq ||
                    binop.op == BinaryOperatorKind::IsEq ||
                    binop.op == BinaryOperatorKind::IsNotEq) 
                {
                    expr->type = &bool_type;
                    expr->value_kind = ExprValueKind::RValue;

                    if (expr->result_discarded) {
                        m_context->report_compiler_diagnostic(expr->loc, expr->range, "Discarding result of relational operator", CompilerDiagKind::Warning);
                    }
                    return;
                }

                expr->type = LHS->type;
                expr->value_kind = ExprValueKind::RValue;

                if (expr->result_discarded) {
                    m_context->report_compiler_diagnostic(expr->loc, expr->range, "Discarding result of binary operator", CompilerDiagKind::Warning);
                }
                return;
            }

            case BinaryOperatorKind::BitAnd:
            case BinaryOperatorKind::BitOr:
            case BinaryOperatorKind::BitXor:
            case BinaryOperatorKind::Shl:
            case BinaryOperatorKind::Shr: {
                if (!LHS->type->is_error()) {
                    if (!LHS->type->is_integral() && !LHS->type->is_string()) {
                        m_context->report_compiler_diagnostic(LHS->loc, LHS->range, fmt::format("Expression must be of a integral type but is of type '{}'", type_info_to_string(LHS->type)));
                    }

                    if (!LHS->type->is_integral()) {
                        m_context->report_compiler_diagnostic(RHS->loc, RHS->range, fmt::format("Expression must be of a integral type but is of type '{}'", type_info_to_string(RHS->type)));
                    }
                }

                insert_arithmetic_promotion(LHS, RHS);

                expr->type = LHS->type;
                expr->value_kind = ExprValueKind::RValue;

                if (expr->result_discarded) {
                    m_context->report_compiler_diagnostic(expr->loc, expr->range, "Discarding result of bitwise operator", CompilerDiagKind::Warning);
                }
                return;
            }

            case BinaryOperatorKind::Eq: {
                if (LHS->value_kind != ExprValueKind::LValue) {
                    m_context->report_compiler_diagnostic(LHS->loc, LHS->range, "Expression must be a modifiable lvalue");
                }

                require_rvalue(RHS);

                ConversionCost cost = get_conversion_cost(LHS->type, RHS->type);

                if (cost.cast_needed) {
                    if (cost.implicit_cast_possible) {
                        insert_implicit_cast(LHS->type, RHS->type, RHS, cost.kind);
                    } else {
                        m_context->report_compiler_diagnostic(expr->loc, expr->range, fmt::format("Cannot implicitly convert from '{}' to '{}'", type_info_to_string(RHS->type), type_info_to_string(LHS->type)));
                    }
                }

                expr->type = LHS->type;
                expr->value_kind = ExprValueKind::LValue;
                return;
            }

            case BinaryOperatorKind::LogAnd:
            case BinaryOperatorKind::LogOr: {
                TypeInfo* boolType = TypeInfo::Create(m_context, TypeKind::Bool, false);

                require_rvalue(LHS);
                require_rvalue(RHS);

                ConversionCost costLHS = get_conversion_cost(boolType, LHS->type);
                ConversionCost costRHS = get_conversion_cost(boolType, RHS->type);

                if (costLHS.cast_needed) {
                    if (costLHS.implicit_cast_possible) {
                        insert_implicit_cast(boolType, LHS->type, LHS, costLHS.kind);
                    } else {
                        m_context->report_compiler_diagnostic(LHS->loc, LHS->range, fmt::format("Cannot implicitly convert from '{}' to 'bool'", type_info_to_string(LHS->type)));
                    }
                }

                if (costRHS.cast_needed) {
                    if (costRHS.implicit_cast_possible) {
                        insert_implicit_cast(boolType, RHS->type, RHS, costRHS.kind);
                    } else {
                        m_context->report_compiler_diagnostic(LHS->loc, LHS->range, fmt::format("Cannot implicitly convert from '{}' to 'bool'", type_info_to_string(RHS->type)));
                    }
                }

                expr->type = boolType;
                expr->value_kind = ExprValueKind::RValue;

                if (expr->result_discarded) {
                    m_context->report_compiler_diagnostic(expr->loc, expr->range, "Discarding result of logical operator", CompilerDiagKind::Warning);
                }
                return;
            }

            default: ARIA_UNREACHABLE();
        }
    }

    void SemanticAnalyzer::resolve_compound_assign_expr(Expr* expr) {
        CompoundAssignExpr& compAss = expr->compound_assign;
        
        resolve_expr(compAss.lhs);
        resolve_expr(compAss.rhs);

        require_rvalue(compAss.rhs);
        
        Expr* LHS = compAss.lhs;
        Expr* RHS = compAss.rhs;
        
        TypeInfo* LHSType = LHS->type;
        TypeInfo* RHSType = RHS->type;
        
        if (LHS->value_kind != ExprValueKind::LValue) {
            m_context->report_compiler_diagnostic(compAss.lhs->loc, compAss.lhs->range, "Expression must be a modifiable lvalue");
        }
        
        ConversionCost cost = get_conversion_cost(LHSType, RHSType);
        
        if (cost.cast_needed) {
            if (cost.implicit_cast_possible) {
                insert_implicit_cast(LHSType, RHSType, RHS, cost.kind);
                RHSType = LHSType;
            } else {
                m_context->report_compiler_diagnostic(compAss.rhs->loc, compAss.rhs->range, fmt::format("Cannot implicitly convert from '{}' to '{}'", type_info_to_string(RHSType), type_info_to_string(LHSType)));
            }
        }
        
        expr->type = LHSType;
        expr->value_kind = ExprValueKind::LValue;
    }

    void SemanticAnalyzer::resolve_expr(Expr* expr) {
        switch (expr->kind) {
            case ExprKind::BooleanLiteral: resolve_boolean_literal_expr(expr); break;
            case ExprKind::CharacterLiteral: resolve_character_literal_expr(expr); break;
            case ExprKind::IntegerLiteral: resolve_integer_literal_expr(expr); break;
            case ExprKind::FloatingLiteral: resolve_floating_literal_expr(expr); break;
            case ExprKind::StringLiteral: resolve_string_literal_expr(expr); break;
            case ExprKind::Null: resolve_null_expr(expr); break;
            case ExprKind::DeclRef: resolve_decl_ref_expr(expr); break;
            case ExprKind::Member: resolve_member_expr(expr); break;
            case ExprKind::Call: resolve_call_expr(expr); break;
            case ExprKind::ArraySubscript: resolve_array_subscript_expr(expr); break;
            case ExprKind::ToSlice: resolve_to_slice_expr(expr); break;
            case ExprKind::New: resolve_new_expr(expr); break;
            case ExprKind::Delete: resolve_delete_expr(expr); break;
            case ExprKind::Format: resolve_format_expr(expr); break;
            case ExprKind::Paren: resolve_paren_expr(expr); break;
            case ExprKind::Cast: resolve_cast_expr(expr); break;
            case ExprKind::UnaryOperator: resolve_unary_operator_expr(expr); break;
            case ExprKind::BinaryOperator: resolve_binary_operator_expr(expr); break;
            case ExprKind::CompoundAssign: resolve_compound_assign_expr(expr); break;
            default: ARIA_UNREACHABLE();
        }
    }

    bool SemanticAnalyzer::is_const_expr(Expr* expr) {
        switch (expr->kind) {
            case ExprKind::Error:
            case ExprKind::BooleanLiteral:
            case ExprKind::CharacterLiteral:
            case ExprKind::IntegerLiteral:
            case ExprKind::FloatingLiteral:
            case ExprKind::StringLiteral:
            case ExprKind::Null:
                return true;

            case ExprKind::Paren:
                return is_const_expr(expr->paren.expression);

            case ExprKind::UnaryOperator:
                return is_const_expr(expr->unary_operator.expression);

            case ExprKind::BinaryOperator:
                return is_const_expr(expr->binary_operator.lhs) && is_const_expr(expr->binary_operator.rhs);

            default: return false;
        }
    }

    bool SemanticAnalyzer::eval_expr_bool(Expr* expr) {
        ARIA_ASSERT(is_const_expr(expr), "Cannot evaulate a non-constant expression");
        ARIA_ASSERT(expr->type->is_boolean(), "Type of expression must be bool");

        switch (expr->kind) {
            case ExprKind::BooleanLiteral: return expr->boolean_literal.value;

            case ExprKind::Paren: return eval_expr_bool(expr->paren.expression);

            case ExprKind::BinaryOperator: {
                bool lhs = eval_expr_bool(expr->binary_operator.lhs);
                bool rhs = eval_expr_bool(expr->binary_operator.rhs);

                switch (expr->binary_operator.op) {
                    case BinaryOperatorKind::Eq: return lhs == rhs;
                    case BinaryOperatorKind::IsNotEq: return lhs != rhs;
                    case BinaryOperatorKind::LogAnd: return lhs && rhs;
                    case BinaryOperatorKind::LogOr: return lhs || rhs;
                    default: ARIA_UNREACHABLE();
                }

                break;
            }

            default: ARIA_UNREACHABLE();
        }
    }

    void SemanticAnalyzer::insert_implicit_cast(TypeInfo* dstType, TypeInfo* srcType, Expr* srcExpr, CastKind castKind) {
        Expr* src = Expr::Dup(m_context, srcExpr); // We must copy the original expression to avoid overwriting the same memory
        Expr* implicitCast = Expr::Create(m_context, src->loc, src->range, ExprKind::ImplicitCast, ExprValueKind::RValue, dstType, ImplicitCastExpr(src, castKind));

        replace_expr(srcExpr, implicitCast);
    }

    void SemanticAnalyzer::require_rvalue(Expr* expr) {
        if (expr->value_kind == ExprValueKind::LValue) {
            if (expr->type->kind == TypeKind::String) {
                replace_expr(expr, Expr::Create(m_context, expr->loc, expr->range, 
                    ExprKind::Copy, ExprValueKind::RValue, expr->type, 
                    CopyExpr(Expr::Dup(m_context, expr), m_builtin_string_copy_constructor)));

                if (m_temporary_context) {
                    replace_expr(expr, Expr::Create(m_context, expr->loc, expr->range,
                        ExprKind::Temporary, ExprValueKind::RValue, expr->type,
                        TemporaryExpr(Expr::Dup(m_context, expr), m_builtin_string_destructor)));
                }
            } else {
                insert_implicit_cast(expr->type, expr->type, expr, CastKind::LValueToRValue);
            }
        }
    }

    void SemanticAnalyzer::insert_arithmetic_promotion(Expr* lhs, Expr* rhs) {
        TypeInfo* lhsType = lhs->type;
        TypeInfo* rhsType = rhs->type;

        if (lhsType->kind == TypeKind::Error || rhsType->kind == TypeKind::Error) {
            return;
        }

        require_rvalue(lhs);
        require_rvalue(rhs);

        if (type_is_equal(lhsType, rhsType)) {
            return;
        }

        if (lhsType->is_integral() && rhsType->is_integral()) {
            size_t lSize = type_get_size(lhsType);
            size_t rSize = type_get_size(rhsType);

            if (lSize > rSize) {
                insert_implicit_cast(lhsType, rhsType, rhs, CastKind::Integral);
            } else if (rSize > lSize) {
                insert_implicit_cast(rhsType, lhsType, lhs, CastKind::Integral);
            } else if (lSize == rSize) {
                // We know that the types are not equal so we likely have a signed/unsigned mismatch
                m_context->report_compiler_diagnostic(lhs->loc, SourceRange(lhs->range.start, rhs->range.end), fmt::format("Mismatched types '{}' and '{}' (implicit signedness conversions are not allowed here)", type_info_to_string(lhsType), type_info_to_string(rhsType)));
            }

            return;
        }

        if (lhsType->is_integral() && rhsType->is_floating_point()) {
            insert_implicit_cast(rhsType, lhsType, lhs, CastKind::IntegralToFloating);
            return;
        }

        if (lhsType->is_floating_point() && rhsType->is_integral()) {
            insert_implicit_cast(lhsType, rhsType, rhs, CastKind::IntegralToFloating);
            return;
        }

        if (lhsType->is_floating_point() && rhsType->is_floating_point()) {
            size_t lSize = type_get_size(lhsType);
            size_t rSize = type_get_size(rhsType);

            if (lSize > rSize) {
                insert_implicit_cast(lhsType, rhsType, rhs, CastKind::Floating);
            } else if (rSize > lSize) {
                insert_implicit_cast(rhsType, lhsType, lhs, CastKind::Floating);
            }

            return;
        }

        ARIA_UNREACHABLE();
    }

} // namespace Aria::Internal