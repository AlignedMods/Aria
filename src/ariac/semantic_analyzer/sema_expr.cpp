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
        std::string_view ident = ref.identifier;

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

            if (mod != m_context->active_comp_unit->parent && d->visibility == DeclVisibility::Private) {
                m_context->report_compiler_diagnostic(expr->loc, expr->range, fmt::format("Symbol '{}' (declared in '{}') is private and cannot be accessed", ref.identifier, mod->name));
                m_context->report_compiler_diagnostic(d->loc, d->range, "Declared here", CompilerDiagKind::Note, d->parent_unit);
            }

            ref.referenced_decl = d;
            expr->type = getType(d);
            return;
        }

        if (m_active_struct) {
            if (m_active_struct->struct_.source_decl->struct_.field_lookup.contains(ident)) {
                TypeInfo* self_type = TypeInfo::Create(m_context, TypeKind::Ref);
                self_type->base = m_active_struct;

                Decl* source = m_active_struct->struct_.source_decl->struct_.field_lookup.at(ident);
                TypeInfo* mem_type = nullptr;

                switch (source->kind) {
                    case DeclKind::Field: mem_type = source->field.type; break;
                    case DeclKind::Method: mem_type = source->method.type; break;
                    case DeclKind::OverloadedMethod: mem_type = &error_type; break;
                    default: ARIA_UNREACHABLE();
                }

                Expr* self = Expr::Create(m_context, expr->loc, expr->range, ExprKind::Self,
                    ExprValueKind::LValue, self_type, ErrorExpr());

                Expr* member = Expr::Create(m_context, expr->loc, expr->range, ExprKind::Member,
                    ExprValueKind::LValue, mem_type,
                    MemberExpr(ident, self));

                replace_expr(expr, member);
                return;
            }
        }

        for (Stmt* import : m_context->active_comp_unit->imports) {
            ARIA_ASSERT(import->kind == StmtKind::Import, "Invalid import");

            if (!import->import.resolved_module) { continue; }

            if (import->import.resolved_module->symbols.contains(ident)) {
                if (import->import.resolved_module->symbols.at(ident)->kind == DeclKind::Struct) {
                    expr->type = &error_type;
                    ref.referenced_decl = import->import.resolved_module->symbols.at(ident);
                    return;
                }
                
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
        TypeInfo* parent_type = mem.parent->type;
        TypeInfo* member_type = nullptr;

        bool searching = true;
        while (searching) {
            switch (parent_type->kind) {
                case TypeKind::Error: {
                    expr->type = &error_type;
                    mem.referenced_member = &error_decl;
                    return;
                }

                case TypeKind::Structure: {
                    StructDeclaration& sd = parent_type->struct_;

                    StructDecl s = sd.source_decl->struct_;
                    if (!s.field_lookup.contains(mem.member)) {
                        searching = false;
                        break;
                    }

                    Decl* fd = s.field_lookup.at(mem.member);
                    switch (fd->kind) {
                        case DeclKind::Field: member_type = fd->field.type; break;
                        case DeclKind::Method: member_type = fd->method.type; break;
                        case DeclKind::OverloadedMethod: member_type = &error_type; break;
                        default: ARIA_UNREACHABLE();
                    }
                    mem.referenced_member = fd;

                    if (fd->visibility == DeclVisibility::Private && mem.parent->kind != ExprKind::Self) {
                        m_context->report_compiler_diagnostic(expr->loc, expr->range, fmt::format("'{}' is private and cannot be accessed", mem.member));
                        m_context->report_compiler_diagnostic(fd->loc, fd->range, "Declared here", CompilerDiagKind::Note, fd->parent_unit);
                    }

                    searching = false;
                    break;
                }

                case TypeKind::Slice: {
                    if (mem.member == "mem") {
                        member_type = TypeInfo::Create(m_context, TypeKind::Ptr);
                        member_type->base = parent_type->base;
                        expr->kind = ExprKind::BuiltinMember;
                    } else if (mem.member == "len") {
                        member_type = &ulong_type;
                        expr->kind = ExprKind::BuiltinMember;
                    }

                    searching = false;
                    break;
                }

                case TypeKind::Ref: { parent_type = parent_type->base; break; }
                case TypeKind::Ptr: {
                    if (mem.implicit_deref) {
                        m_context->report_compiler_diagnostic(expr->loc, expr->range, "'.' operator allows only one level of implicit dereferncing");
                    }

                    parent_type = parent_type->base;
                    mem.implicit_deref = true;
                    break;
                }

                default: {
                    m_context->report_compiler_diagnostic(mem.parent->loc, mem.parent->range, fmt::format("Expression must be of slice or struct type but is '{}'", type_info_to_string(parent_type)));
                    expr->type = &error_type;
                    mem.referenced_member = &error_decl;
                    return;
                }
            }
        }
        

        if (!member_type) {
            m_context->report_compiler_diagnostic(expr->loc, expr->range, fmt::format("Unknown member \"{}\" in '{}'", mem.member, type_info_to_string(parent_type)));
            mem.referenced_member = &error_decl;
            expr->type = &error_type;
            return;
        }

        expr->type = member_type;

        if (expr->result_discarded) {
            m_context->report_compiler_diagnostic(expr->loc, expr->range, "Discarding result of member access", CompilerDiagKind::Warning);
        }
    }

    void SemanticAnalyzer::resolve_call_expr(Expr* expr) {
        CallExpr& call = expr->call;

        resolve_expr(call.callee);

        if (call.callee->kind == ExprKind::Member) {
            expr->kind = ExprKind::MethodCall;
            resolve_method_call_expr(expr);
            return;
        }

        TypeInfo* calleeType = call.callee->type;

        if (calleeType->kind != TypeKind::Function && !calleeType->is_error()) {
            m_context->report_compiler_diagnostic(expr->loc, expr->range, "Cannot call an object of non-function type");
            expr->type = &error_type;
            return;
        }

        if (call.callee->decl_ref.referenced_decl->kind != DeclKind::Error) {
            if (call.callee->kind == ExprKind::DeclRef && call.callee->decl_ref.referenced_decl->kind == DeclKind::OverloadedFunction) { // Overloaded function
                resolve_decl(call.callee->decl_ref.referenced_decl);
                m_temporary_context = true;
                for (Expr* arg : call.arguments) {
                    resolve_expr(arg);
                }
                m_temporary_context = false;

                Decl* resolved = nullptr;

                for (Decl* func : call.callee->decl_ref.referenced_decl->overloaded_function.funcs) {
                    if (func->function.type->function.param_types.size != call.arguments.size) { goto again; }

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

                    expr->type = &error_type;
                    return;
                }

                for (size_t i = 0; i < resolved->function.type->function.param_types.size; i++) {
                    if (!resolved->function.type->function.param_types.items[i]->is_reference()) { require_rvalue(call.arguments.items[i]); }
                }

                call.callee->decl_ref.referenced_decl = resolved;
                call.callee->type = resolved->function.type;
                expr->type = resolved->function.type->function.return_type;
                return;
            } else if (!call.callee->type->is_error()) { // Normal function
                resolve_type(call.callee->decl_ref.referenced_decl->loc, call.callee->decl_ref.referenced_decl->range, call.callee->decl_ref.referenced_decl->function.type);
                FunctionDeclaration& fnDecl = calleeType->function;

                for (auto& attr : call.callee->decl_ref.referenced_decl->function.attributes) {
                    if (attr.kind == FunctionDecl::AttributeKind::Unsafe && !m_unsafe_context) {
                        m_context->report_compiler_diagnostic(expr->loc, expr->range, "Cannot call unsafe function in safe context");
                    }
                }

                if (fnDecl.param_types.size != call.arguments.size) {
                    m_context->report_compiler_diagnostic(expr->loc, expr->range, fmt::format("Mismatched argument count, function expects {} but got {}", fnDecl.param_types.size, call.arguments.size));
                    for (size_t i = 0; i < call.arguments.size; i++) {
                        resolve_expr(call.arguments.items[i]);
                    }
                } else {
                    for (size_t i = 0; i < fnDecl.param_types.size; i++) {
                        resolve_param_initializer(fnDecl.param_types.items[i], call.arguments.items[i]);
                    }
                }

                expr->type = fnDecl.return_type;
                expr->value_kind = (fnDecl.return_type->is_reference()) ? ExprValueKind::LValue : ExprValueKind::RValue;

                return;
            }
        } else {
            for (Expr* arg : call.arguments) {
                resolve_expr(arg);
            }
        }

        expr->type = &error_type;
        call.callee->kind = ExprKind::Error;
    }

    void SemanticAnalyzer::resolve_construct_expr(Expr* expr) {
        ConstructExpr& construct = expr->construct;

        if (!expr->type) {
            m_context->report_compiler_diagnostic(expr->loc, expr->range, "Construct expression requries an explicit type");
            replace_expr(expr, &error_expr);
            return;
        }

        resolve_type(expr->loc, expr->range, expr->type);
        
        if (!expr->type->is_structure()) {
            if (construct.arguments.size == 0) {
                Expr* def = nullptr;
                create_default_initializer(&def, expr->type, expr->loc, expr->range);
                replace_expr(expr, def);
            } else if (construct.arguments.size == 1) {
                replace_expr(expr, construct.arguments.items[0]);
            } else {
                m_context->report_compiler_diagnostic(expr->loc, expr->range, "Constructing a non-struct type must have 0 or 1 argument");
                replace_expr(expr, &error_expr);
            }
        } else {
            ARIA_TODO("Construct");
        }
    }

    void SemanticAnalyzer::resolve_method_call_expr(Expr* expr) {
        MethodCallExpr& mc = expr->method_call;

        resolve_expr(mc.callee);
        TypeInfo* callee_type = mc.callee->type;

        if (!callee_type->is_function() && !callee_type->is_error()) {
            m_context->report_compiler_diagnostic(expr->loc, expr->range, "Cannot call an object of non-function type");
            expr->type = &error_type;
            return;
        }

        if (mc.callee->member.referenced_member->kind != DeclKind::Error) {
            if (mc.callee->member.referenced_member->kind == DeclKind::OverloadedMethod) {
                resolve_decl(mc.callee->member.referenced_member);
                m_temporary_context = true;
                for (Expr* arg : mc.arguments) {
                    resolve_expr(arg);
                }
                m_temporary_context = false;

                Decl* resolved = nullptr;

                for (Decl* func : mc.callee->member.referenced_member->overloaded_method.funcs) {
                    if (func->method.type->function.param_types.size != mc.arguments.size) { goto again; }

                    for (size_t i = 0; i < func->method.type->function.param_types.size; i++) {
                        if (!type_is_equal(func->method.type->function.param_types.items[i], mc.arguments.items[i]->type)) { goto again; }
                    }

                    goto done;

                done:
                    resolved = func;
                    break;

                again:
                    continue;
                }

                if (!resolved) {
                    m_context->report_compiler_diagnostic(expr->loc, expr->range, fmt::format("No matching overloaded method '{}' to call", mc.callee->member.member));
                    for (size_t i = 0; i < mc.arguments.size; i++) {
                        mc.arguments.items[i]->type = &error_type;
                    }

                    expr->type = &error_type;
                    return;
                }

                for (size_t i = 0; i < resolved->method.type->function.param_types.size; i++) {
                    if (!resolved->method.type->function.param_types.items[i]->is_reference()) { require_rvalue(mc.arguments.items[i]); }
                }

                mc.callee->member.referenced_member = resolved;
                mc.callee->type = resolved->method.type;
                expr->type = resolved->method.type->function.return_type;
                expr->value_kind = (resolved->method.type->function.return_type->is_reference()) ? ExprValueKind::LValue : ExprValueKind::RValue;
            } else if (mc.callee->member.referenced_member->kind == DeclKind::Method) {
                resolve_type(mc.callee->member.referenced_member->loc, mc.callee->member.referenced_member->range, mc.callee->member.referenced_member->method.type);
                FunctionDeclaration& fnDecl = callee_type->function;

                if (fnDecl.param_types.size != mc.arguments.size) {
                    m_context->report_compiler_diagnostic(expr->loc, expr->range, fmt::format("Mismatched argument count, method expects {} but got {}", fnDecl.param_types.size, mc.arguments.size));
                    for (size_t i = 0; i < mc.arguments.size; i++) {
                        resolve_expr(mc.arguments.items[i]);
                    }
                } else {
                    for (size_t i = 0; i < fnDecl.param_types.size; i++) {
                        resolve_param_initializer(fnDecl.param_types.items[i], mc.arguments.items[i]);
                    }
                }

                expr->type = fnDecl.return_type;
                expr->value_kind = (fnDecl.return_type->is_reference()) ? ExprValueKind::LValue : ExprValueKind::RValue;
            }
        } else {
            for (Expr* arg : mc.arguments) {
                resolve_expr(arg);
            }

            expr->type = &error_type;
        }
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

                if (subs.array->type->base->is_void()) {
                    m_context->report_compiler_diagnostic(expr->loc, expr->range, "Cannot index into 'void*' because it would dereference to 'void'");
                    expr->type = &error_type;
                    break;
                }

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

            default: m_context->report_compiler_diagnostic(subs.array->loc, subs.array->range, "'[' operator can only be used with a pointer/slice/array"); expr->type = &error_type; break;
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
                expr->type = TypeInfo::Create(m_context, TypeKind::Slice);
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

        if (srcType->is_error() || dstType->is_error()) { return; }

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

                TypeInfo* newType = TypeInfo::Create(m_context, TypeKind::Ptr);
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

            case UnaryOperatorKind::Increment: {
                if (!unop.expression->type->is_error()) {
                    if (!unop.expression->type->is_numeric()) {
                        m_context->report_compiler_diagnostic(unop.expression->loc, unop.expression->range, fmt::format("Expression must be of a numeric type but is of type '{}'", type_info_to_string(unop.expression->type)));
                        expr->type = &error_type;
                        break;
                    }
                }

                if (unop.expression->value_kind != ExprValueKind::LValue) {
                    m_context->report_compiler_diagnostic(unop.expression->loc, unop.expression->range, "Expression must be a modifiable lvalue");
                    expr->type = &error_type;
                    break;
                }

                expr->type = unop.expression->type;
                expr->value_kind = ExprValueKind::RValue;
                break;
            }

            case UnaryOperatorKind::Decrement: {
                if (!unop.expression->type->is_error()) {
                    if (!unop.expression->type->is_numeric()) {
                        m_context->report_compiler_diagnostic(unop.expression->loc, unop.expression->range, fmt::format("Expression must be of a numeric type but is of type '{}'", type_info_to_string(unop.expression->type)));
                        expr->type = &error_type;
                        break;
                    }
                }

                if (unop.expression->value_kind != ExprValueKind::LValue) {
                    m_context->report_compiler_diagnostic(unop.expression->loc, unop.expression->range, "Expression must be a modifiable lvalue");
                    expr->type = &error_type;
                    break;
                }

                expr->type = unop.expression->type;
                expr->value_kind = ExprValueKind::RValue;
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
                        expr->type = &error_type;
                        break;
                    }

                    if (!RHS->type->is_numeric()) {
                        m_context->report_compiler_diagnostic(RHS->loc, RHS->range, fmt::format("Expression must be of a numeric type but is of type '{}'", type_info_to_string(RHS->type)));
                        expr->type = &error_type;
                        break;
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
                require_rvalue(LHS);
                require_rvalue(RHS);

                ConversionCost costLHS = get_conversion_cost(&bool_type, LHS->type);
                ConversionCost costRHS = get_conversion_cost(&bool_type, RHS->type);

                if (costLHS.cast_needed) {
                    if (costLHS.implicit_cast_possible) {
                        insert_implicit_cast(&bool_type, LHS->type, LHS, costLHS.kind);
                    } else {
                        m_context->report_compiler_diagnostic(LHS->loc, LHS->range, fmt::format("Cannot implicitly convert from '{}' to 'bool'", type_info_to_string(LHS->type)));
                    }
                }

                if (costRHS.cast_needed) {
                    if (costRHS.implicit_cast_possible) {
                        insert_implicit_cast(&bool_type, RHS->type, RHS, costRHS.kind);
                    } else {
                        m_context->report_compiler_diagnostic(LHS->loc, LHS->range, fmt::format("Cannot implicitly convert from '{}' to 'bool'", type_info_to_string(RHS->type)));
                    }
                }

                expr->type = &bool_type;
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
            case ExprKind::Error: break;
            case ExprKind::BooleanLiteral: resolve_boolean_literal_expr(expr); break;
            case ExprKind::CharacterLiteral: resolve_character_literal_expr(expr); break;
            case ExprKind::IntegerLiteral: resolve_integer_literal_expr(expr); break;
            case ExprKind::FloatingLiteral: resolve_floating_literal_expr(expr); break;
            case ExprKind::StringLiteral: resolve_string_literal_expr(expr); break;
            case ExprKind::Null: resolve_null_expr(expr); break;
            case ExprKind::DeclRef: resolve_decl_ref_expr(expr); break;
            case ExprKind::Member: resolve_member_expr(expr); break;
            case ExprKind::Self: break;
            case ExprKind::Call: resolve_call_expr(expr); break;
            case ExprKind::Construct: resolve_construct_expr(expr); break;
            case ExprKind::MethodCall: resolve_method_call_expr(expr); break;
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

    u64 SemanticAnalyzer::eval_expr_u64(Expr* expr) {
        ARIA_ASSERT(is_const_expr(expr), "Cannot evaulate a non-constant expression");
        ARIA_ASSERT(expr->type->is_integral(), "Expression must be of integral type");

        switch (expr->kind) {
            case ExprKind::IntegerLiteral: return expr->integer_literal.value;

            case ExprKind::Paren: return eval_expr_u64(expr->paren.expression);

            case ExprKind::BinaryOperator: {
                u64 lhs = eval_expr_u64(expr->binary_operator.lhs);
                u64 rhs = eval_expr_u64(expr->binary_operator.rhs);

                switch (expr->binary_operator.op) {
                    case BinaryOperatorKind::Add: return lhs + rhs;
                    case BinaryOperatorKind::Sub: return lhs - rhs;
                    case BinaryOperatorKind::Mul: return lhs * rhs;
                    case BinaryOperatorKind::Div: return lhs / rhs;
                    case BinaryOperatorKind::Mod: return lhs % rhs;
                    case BinaryOperatorKind::BitAnd: return lhs & rhs;
                    case BinaryOperatorKind::BitOr: return lhs | rhs;
                    case BinaryOperatorKind::BitXor: return lhs ^ rhs;
                    case BinaryOperatorKind::Shl: return lhs << rhs;
                    case BinaryOperatorKind::Shr: return lhs >> rhs;

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
            insert_implicit_cast(expr->type, expr->type, expr, CastKind::LValueToRValue);
        }
    }

    void SemanticAnalyzer::maybe_promote_to_int(Expr* expr) {
        switch (expr->type->kind) {
            case TypeKind::Char:
            case TypeKind::Short:
                insert_implicit_cast(&int_type, expr->type, expr, CastKind::Integral);
                break;

            case TypeKind::UChar:
            case TypeKind::UShort:
                insert_implicit_cast(&uint_type, expr->type, expr, CastKind::Integral);
                break;

            default: break;
        }
    }

    void SemanticAnalyzer::insert_arithmetic_promotion(Expr* lhs, Expr* rhs) {
        if (lhs->type->kind == TypeKind::Error || rhs->type->kind == TypeKind::Error) {
            return;
        }

        require_rvalue(lhs);
        require_rvalue(rhs);

        if (lhs->type->is_integral() && rhs->type->is_integral()) {
            // We want to keep the original types for error messages
            TypeInfo lhs_type = *lhs->type;
            TypeInfo rhs_type = *rhs->type;

            maybe_promote_to_int(lhs);
            maybe_promote_to_int(rhs);

            size_t l_size = type_get_size(lhs->type);
            size_t r_size = type_get_size(rhs->type);

            if (l_size > r_size) {
                insert_implicit_cast(lhs->type, rhs->type, rhs, CastKind::Integral);
            } else if (r_size > l_size) {
                insert_implicit_cast(rhs->type, lhs->type, lhs, CastKind::Integral);
            } else if (l_size == r_size) {
                if (lhs->type->is_signed() != rhs->type->is_signed()) {
                    m_context->report_compiler_diagnostic_with_notes(lhs->loc, SourceRange(lhs->range.start, rhs->range.end), 
                        fmt::format("Mismatched types '{}' and '{}'", type_info_to_string(&lhs_type), type_info_to_string(&rhs_type)),
                        { "implicit signedness conversions are not allowed here"} );
                }
            }

            return;
        }

        if (lhs->type->is_integral() && rhs->type->is_floating_point()) {
            insert_implicit_cast(rhs->type, lhs->type, lhs, CastKind::IntegralToFloating);
            return;
        }

        if (lhs->type->is_floating_point() && rhs->type->is_integral()) {
            insert_implicit_cast(lhs->type, rhs->type, rhs, CastKind::IntegralToFloating);
            return;
        }

        if (lhs->type->is_floating_point() && rhs->type->is_floating_point()) {
            size_t lSize = type_get_size(lhs->type);
            size_t rSize = type_get_size(rhs->type);

            if (lSize > rSize) {
                insert_implicit_cast(lhs->type, rhs->type, rhs, CastKind::Floating);
            } else if (rSize > lSize) {
                insert_implicit_cast(rhs->type, lhs->type, lhs, CastKind::Floating);
            }

            return;
        }

        if (!type_is_equal(lhs->type, rhs->type)) {
            m_context->report_compiler_diagnostic(lhs->loc, SourceRange(lhs->range.start, rhs->range.end),
                fmt::format("Mismatched types '{}' and '{}'", type_info_to_string(lhs->type), type_info_to_string(rhs->type)));
        }

        ARIA_UNREACHABLE();
    }

} // namespace Aria::Internal