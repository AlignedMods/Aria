#include "ariac/semantic_analyzer/semantic_analyzer.hpp"

namespace ariac {

    void SemanticAnalyzer::resolve_type(TypeInfo* type) {
        switch (type->kind) {
            case TypeKind::Unresolved: {
                UnresolvedType& t = type->unresolved;
                resolve_expr(t.ident);

                if (t.ident->kind == ExprKind::Error) {
                    type->kind = TypeKind::Error;
                    break;
                }

                if (t.ident->kind != ExprKind::TypeInfo) {
                    report_diag(type->loc, fmt::format("'{}' is not a type", t.ident->decl_ref.identifier));
                    type->kind = TypeKind::Error;
                    break;
                }

                *type = *t.ident->type_info.type;
                break;
            }

            case TypeKind::Typeof: {
                TypeofType& t = type->typeof;
                resolve_expr(t.expr);
                *type = *t.expr->type;
                break;
            }

            case TypeKind::Pointer: resolve_type(type->pointer.base); break;
            case TypeKind::Slice: resolve_type(type->slice.base); break;
            
            case TypeKind::Array: {
                resolve_type(type->array.base);
                resolve_expr(type->array.expression);

                if (!is_const_expr(type->array.expression)) {
                    report_diag(type->array.expression->loc, "Size of array must be a compile time constant");
                    break;
                }

                ConversionCost cost = get_conversion_cost(TypeInfo::get_basic(TypeKind::ULong), type->array.expression->type);
                if (cost.cast_needed && !cost.implicit_cast_possible) {
                    report_diag(type->array.expression->loc, "Size of array must be convertable to 'ulong'");
                    break;
                }

                Expr* cexpr = eval_const_expr(type->array.expression);
                type->array.size = cexpr->const_.integer;
                break;
            }

            case TypeKind::Function:
            case TypeKind::Method: {
                FunctionType& fn = type->function;

                for (Decl* param : fn.params) {
                    resolve_type(param->param.type);
                }

                resolve_type(fn.return_type);
                break;
            }

            case TypeKind::Template: {
                TemplateType& t = type->template_;

                auto ctx = m_inlines.get<TemplateInstantationContext>();
                if (!ctx) { break; }
                if (!ctx->template_types.contains(t.resolved_decl)) { break; }

                *type = *ctx->template_types.at(t.resolved_decl);
                break;
            }

            case TypeKind::StructSpecilization: {
                StructSpecilizationType& gi = type->struct_specilization;

                {
                    bool gi_prev_val = m_sema_context.struct_specilization;
                    m_sema_context.struct_specilization = true;
                    resolve_type(gi.base);
                    m_sema_context.struct_specilization = gi_prev_val;
                }

                for (TypeInfo* t : gi.arguments) {
                    resolve_type(t);
                    if (t->is_error()) {
                        type->kind = TypeKind::Error;
                        return;
                    }
                }

                if (!gi.base->is_template_decl()) {
                    report_error(gi.base->loc, fmt::format("'{}' is not a template", gi.base->to_string()));
                    break;
                }

                if (!gi.needs_specilization()) {
                    break;
                }

                Decl* g = gi.base->struct_.source_decl->struct_.parent;
                ARIA_ASSERT(g->kind == DeclKind::Template, "Invalid template");
                ARIA_ASSERT(g->template_.template_decl->kind == DeclKind::Struct, "Invalid template");

                if (gi.arguments.size != g->template_.parameters.size) {
                    report_diag(type->loc, fmt::format("Mismatched template instantiation, template expects {} arguments but got {}", g->template_.parameters.size, gi.arguments.size));
                    break;
                }

                Decl* specilization = nullptr;
                for (Decl* i : g->template_.specilizations) {
                    ARIA_ASSERT(i->kind == DeclKind::StructSpecilization, "Invalid template specilization");

                    bool failed = false;
                    for (size_t idx = 0; idx < gi.arguments.size; idx++) {
                        if (!type_is_equal(gi.arguments.items[idx], i->struct_specilization.types.items[idx])) { failed = true; break; }
                    }

                    if (!failed) { specilization = i; }
                }

                if (!specilization) {
                    TemplateInstantationContext ctx;
                    ctx.template_decl = g;
                    ctx.loc = type->loc;

                    for (size_t i = 0; i < gi.arguments.size; i++) {
                        Decl* gen_param = g->template_.parameters.items[i];
                        TypeInfo* gen_arg = gi.arguments.items[i];
                        ARIA_ASSERT(gen_param->kind == DeclKind::TemplateParam, "Invalid template parameter");
                        ctx.template_types[gen_param] = gen_arg;
                    }

                    m_inlines.push(ctx);

                    if (g->template_.template_decl->struct_.body_resolve_status == ResolveStatus::NotStarted) {
                        TemplateContext ctx;
                        for (Decl* p : g->template_.parameters) {
                            ctx[p->template_param.identifier] = p;
                        }
                        m_generics.push_back(ctx);
        
                        CompilationUnit* unit = context.active_comp_unit;
                        context.active_comp_unit = g->parent_unit;
                        resolve_struct_body(g->template_.template_decl);
                        context.active_comp_unit = unit;
        
                        m_generics.pop_back();
                    }

                    Decl* struc = Decl::dup(g->template_.template_decl);
                    struc->parent_module = g->parent_module;
                    struc->parent_unit = g->parent_unit;

                    specilization = Decl::Create(g->loc, DeclKind::StructSpecilization, g->visibility, StructSpecilizationDecl(gi.arguments, struc, type->loc));
                    specilization->parent_module = g->parent_module;
                    specilization->parent_unit = g->parent_unit;
                    g->template_.specilizations.append(specilization);
                    struc->struct_.parent = specilization;

                    resolve_struct_decl(struc);
                    resolve_struct_body(struc);

                    m_inlines.pop();
                }

                gi.resolved_decl = specilization;
                break;
            }

            default: break;
        }
    }

    ConversionCost SemanticAnalyzer::get_conversion_cost(TypeInfo* dst, TypeInfo* src) {
        ConversionCost cost{};
        cost.cast_needed = true;
        cost.explicit_cast_possible = true;
        cost.implicit_cast_possible = true;

        if (dst->is_typedef()) { dst = dst->typedef_.base; }
        if (src->is_typedef()) { src = src->typedef_.base; }

        if (dst->get_bottom_type()->is_template() || src->get_bottom_type()->is_template()) {
            cost.cast_needed = false;
            return cost;
        }

        if (dst->is_error() || src->is_error()) {
            cost.cast_needed = false;
            return cost;
        }

        if (dst->is_void() && (src->is_void() || src->is_never())) {
            cost.cast_needed = false;
            return cost;
        }

        if (dst->is_never() && src->is_never()) {
            cost.cast_needed = false;
            return cost;
        }

        if (dst->is_boolean() && src->is_boolean()) {
            cost.cast_needed = false;
            return cost;
        }

        if (src->is_integral()) {
            if (dst->is_integral()) { // Int to int
                if (src->get_bit_size() != dst->get_bit_size()) {
                    cost.kind = CastKind::IntegralCast;
                } else {
                    if (src->is_signed() == dst->is_signed()) {
                        cost.cast_needed = false;
                    } else {
                        cost.kind = CastKind::IntegralCast;
                        cost.cast_needed = true;
                    }
                }
            } else if (dst->is_boolean()) { // Int to bool
                cost.kind = CastKind::IntegralToBoolean;
                cost.implicit_cast_possible = false;
            } else if (dst->is_floating_point()) { // Int to float
                cost.kind = CastKind::IntegralToFloating;
            } else {
                cost.implicit_cast_possible = false;
                cost.explicit_cast_possible = false;
            }

            return cost;
        }

        if (src->is_floating_point()) {
            if (dst->is_floating_point()) { // Float to float
                if (src->get_bit_size() != dst->get_bit_size()) {
                    cost.kind = CastKind::FloatingCast;
                } else {
                    cost.cast_needed = false;
                }
            } else if (dst->is_integral()) { // Float to int
                cost.kind = CastKind::FloatingToIntegral;
                cost.implicit_cast_possible = false;
            } else if (dst->is_boolean()) { // Float to bool
                cost.kind = CastKind::FloatingToBoolean;
                cost.implicit_cast_possible = false;
            } else {
                cost.explicit_cast_possible = false;
            }

            return cost;
        }

        if (src->is_string()) {
            if (dst->is_string()) {
                cost.cast_needed = false;
                return cost;
            }

            if (dst->is_pointer()) {
                if (dst->pointer.base->kind == TypeKind::Char) {
                    cost.kind = CastKind::SliceToPointer;
                    cost.implicit_cast_possible = true;
                    cost.explicit_cast_possible = true;
                    return cost;
                }
            }
        }

        if (dst->is_typeid() && src->is_typeid()) {
            cost.cast_needed = false;
            return cost;
        }

        if (src->is_any()) {
            if (dst->is_any()) {
                cost.cast_needed = false;
                return cost;
            }

            cost.kind = CastKind::AnyCast;
            cost.implicit_cast_possible = false;
            return cost;
        }

        if (src->is_pointer()) {
            if (dst->is_pointer()) { // Ptr to ptr
                if (src->pointer.is_const && !dst->pointer.is_const) { // *const int -> *int is not allowed
                    cost.implicit_cast_possible = false;
                    cost.explicit_cast_possible = false;
                    return cost;
                }

                // Check for *[100]int -> *int
                if (src->pointer.base->is_array()) {
                    if (dst->pointer.base->is_void()) {
                        cost.kind = CastKind::BitCast;
                        return cost;
                    }

                    ConversionCost base_cost = get_conversion_cost(dst->pointer.base, src->pointer.base->array.base);
                    if (!base_cost.cast_needed) {
                        cost.kind = CastKind::BitCast;
                        return cost;
                    }
                }

                if ((src->pointer.base->is_void() && !dst->pointer.base->is_void()) || (dst->pointer.base->is_void() && !src->pointer.base->is_void())) { // Allow void* conversions
                    cost.kind = CastKind::BitCast;
                    return cost;
                }

                ConversionCost base_cost = get_conversion_cost(dst->pointer.base, src->pointer.base);
                if (base_cost.cast_needed) {
                    cost.explicit_cast_possible = false;
                    cost.implicit_cast_possible = false;
                    return cost;
                }

                cost.cast_needed = false;
                return cost;
            } else if (dst->is_any() && !src->pointer.base->is_void()) { // Ptr to any
                cost.kind = CastKind::PointerToAny;
                return cost;
            }
        }

        if (src->is_array()) {
            if (dst->is_array()) {
                if (src->array.size != dst->array.size) {
                    cost.implicit_cast_possible = false;
                    cost.explicit_cast_possible = false;
                    return cost;
                }

                ConversionCost base_cost = get_conversion_cost(dst->array.base, src->array.base);
                if (base_cost.cast_needed) {
                    cost.implicit_cast_possible = false;
                    cost.explicit_cast_possible = false;
                    return cost;
                }

                cost.cast_needed = false;
                return cost;
            }
        }

        if (src->is_slice()) {
            if (dst->is_pointer()) {
                if (dst->pointer.base->is_void()) {
                    cost.kind = CastKind::SliceToPointer;
                    cost.implicit_cast_possible = true;
                    cost.explicit_cast_possible = true;
                    return cost;
                } else {
                    ConversionCost base_cost = get_conversion_cost(dst->slice.base, src->slice.base);

                    if (base_cost.cast_needed) {
                        cost.implicit_cast_possible = false;
                        cost.explicit_cast_possible = false;
                        return cost;
                    }

                    cost.kind = CastKind::SliceToPointer;
                    cost.implicit_cast_possible = true;
                    cost.explicit_cast_possible = true;
                    return cost;
                }
            }

            if (dst->is_slice()) {
                ConversionCost base_cost = get_conversion_cost(dst->slice.base, src->slice.base);

                if (base_cost.cast_needed) {
                    cost.implicit_cast_possible = false;
                    cost.explicit_cast_possible = false;
                    return cost;
                }
                
                cost.cast_needed = false;
                return cost;
            }

            if (dst->is_string()) {
                if (src->slice.base->kind == TypeKind::Char) {
                    cost.cast_needed = false;
                    return cost;
                }
            }
        }

        if (src->is_function() && dst->is_function()) {
            if (src->function.variadic != dst->function.variadic) {
                cost.implicit_cast_possible = false;
                cost.explicit_cast_possible = false;
                return cost;
            }

            if (src->function.params.size != dst->function.params.size) {
                cost.implicit_cast_possible = false;
                cost.explicit_cast_possible = false;
                return cost;
            }

            for (size_t i = 0; i < src->function.params.size; i++) {
                ConversionCost pcost = get_conversion_cost(dst->function.params.items[i]->param.type, src->function.params.items[i]->param.type);

                if (pcost.cast_needed) {
                    cost.implicit_cast_possible = false;
                    cost.explicit_cast_possible = false;
                    return cost;
                }
            }

            ConversionCost rcost = get_conversion_cost(dst->function.return_type, src->function.return_type);
            if (rcost.cast_needed) {
                cost.implicit_cast_possible = false;
                cost.explicit_cast_possible = false;
                return cost;
            }

            cost.cast_needed = false;
            return cost;
        }

        if (src->is_struct() && dst->is_struct()) {
            if (src->struct_.source_decl != dst->struct_.source_decl) {
                cost.explicit_cast_possible = false;
                cost.implicit_cast_possible = false;
                return cost;
            }

            cost.cast_needed = false;
            return cost;
        }

        if (src->is_enum()) {
            if (dst->is_enum()) {
                if (src->enum_.source_decl != dst->enum_.source_decl) {
                    cost.explicit_cast_possible = false;
                    cost.implicit_cast_possible = false;
                    return cost;
                }

                cost.cast_needed = false;
                return cost;
            }
        }

        if (src->is_struct_specilization()) {
            if (src->struct_specilization.resolved_decl != dst->struct_specilization.resolved_decl) {
                cost.explicit_cast_possible = false;
                cost.implicit_cast_possible = false;
                return cost;
            }

            cost.cast_needed = false;
            return cost;
        }

        cost.explicit_cast_possible = false;
        cost.implicit_cast_possible = false;
        return cost;
    }

    bool SemanticAnalyzer::type_is_equal(TypeInfo* lhs, TypeInfo* rhs) {
        if (lhs->is_typedef()) { lhs = lhs->typedef_.base; }
        if (rhs->is_typedef()) { rhs = rhs->typedef_.base; }

        if (lhs->is_enum() && rhs->is_enum()) {
            return lhs->enum_.source_decl == rhs->enum_.source_decl;
        }

        if (lhs->is_array() && rhs->is_array()) {
            return type_is_equal(lhs->array.base, rhs->array.base) && lhs->array.size == rhs->array.size;
        }

        if (lhs->is_slice() && rhs->is_slice()) {
            return type_is_equal(lhs->slice.base, rhs->slice.base);
        }

        if (lhs->is_pointer() && rhs->is_pointer()) {
            return type_is_equal(lhs->pointer.base, rhs->pointer.base);
        }

        if (lhs->is_primitive() && rhs->is_primitive()) {
            return lhs->kind == rhs->kind;
        }

        if (lhs->is_function() && rhs->is_function()) {
            FunctionType& fLhs = lhs->function;
            FunctionType& fRhs = rhs->function;

            if (!type_is_equal(fLhs.return_type, fRhs.return_type)) { return false; }
            if (fLhs.params.size != fRhs.params.size) { return false; }

            for (size_t i = 0; i < fLhs.params.size; i++) {
                if (!type_is_equal(fLhs.params.items[i]->param.type, fRhs.params.items[i]->param.type)) { return false; }
            }

            return true;
        }

        if (lhs->is_struct() && rhs->is_struct()) {
            return lhs->struct_.source_decl == rhs->struct_.source_decl;
        }

        return false;
    }

    bool SemanticAnalyzer::type_is_trivial(TypeInfo* t) {
        switch (t->kind) {
            case TypeKind::Struct: {
                StructType& sDecl = t->struct_;

                if (sDecl.source_decl) {
                    ARIA_ASSERT(sDecl.source_decl->kind == DeclKind::Struct, "Invalid source decl");
                    // return sDecl.source_decl->struct_.definition.dtor == nullptr;
                    return false;
                }

                return true;
            }

            default: return true;
        }
    }

    bool SemanticAnalyzer::cast_needs_rvalue(CastKind kind) {
        switch (kind) {
            case CastKind::SliceToPointer:
            case CastKind::AnyCast:
            case CastKind::LValueToRValue: return false;

            default: return true;
        }
    }

    TypeInfo* SemanticAnalyzer::type_from_decl(Decl* decl) {
        switch (decl->kind) {
            case DeclKind::Struct: {
                if (decl->resolve_status == ResolveStatus::InProgress) {
                    report_diag(decl->loc, "Recursive definition of struct");
                    return TypeInfo::get_error();
                } else {
                    CompilationUnit* old_unit = context.active_comp_unit;
                    context.active_comp_unit = decl->parent_unit;
                    resolve_struct_decl(decl);
                    context.active_comp_unit = old_unit;
                }

                return TypeInfo::create_struct(decl);
            }

            case DeclKind::Typedef: {
                if (decl->resolve_status == ResolveStatus::InProgress) {
                    report_diag(decl->loc, "Recursive definition of typedef");
                    return TypeInfo::get_error();
                } else {
                    CompilationUnit* old_unit = context.active_comp_unit;
                    context.active_comp_unit = decl->parent_unit;
                    resolve_typedef_decl(decl);
                    context.active_comp_unit = old_unit;
                }

                return TypeInfo::create_typedef(decl);
            }

            case DeclKind::Enum: {
                if (decl->resolve_status == ResolveStatus::InProgress) {
                    report_diag(decl->loc, "Recursive definition of enum");
                    return TypeInfo::get_error();
                } else {
                    CompilationUnit* old_unit = context.active_comp_unit;
                    context.active_comp_unit = decl->parent_unit;
                    resolve_enum_decl(decl);
                    context.active_comp_unit = old_unit;
                }

                return TypeInfo::create_enum(decl);
            }

            case DeclKind::Template: {
                if (decl->resolve_status == ResolveStatus::InProgress) {
                    report_diag(decl->loc, "Recursive definition of generic");
                    return TypeInfo::get_error();
                } else {
                    CompilationUnit* old_unit = context.active_comp_unit;
                    context.active_comp_unit = decl->parent_unit;
                    resolve_template_decl(decl);
                    context.active_comp_unit = old_unit;
                }

                return TypeInfo::create_struct(decl->template_.template_decl);
            }

            case DeclKind::TemplateParam: {
                return TypeInfo::create_template(decl);
            }

            default: ARIA_UNREACHABLE("Invalid decl");
        }
    }

    TypeInfo* SemanticAnalyzer::type_for_self(Decl* decl) {
        ARIA_ASSERT(decl->kind == DeclKind::Struct, "Invalid self type");

        StructDecl& s = decl->struct_;
        TypeInfo* base = type_from_decl(decl);

        if (s.parent) {
            switch (s.parent->kind) {
                case DeclKind::StructSpecilization: {
                    base = TypeInfo::create_struct_instantation(TypeInfo::dup(base), s.parent->struct_specilization.types);
                    break;
                }

                case DeclKind::Template: {
                    TinyVector<TypeInfo*> types;
                    for (Decl* p : s.parent->template_.parameters) {
                        ARIA_ASSERT(p->kind == DeclKind::TemplateParam, "Invalid template parameter");
                        types.append(TypeInfo::create_template(p));
                    }
                    base = TypeInfo::create_struct_instantation(TypeInfo::dup(base), types);
                    break;
                }

                default: ARIA_UNREACHABLE("Invalid struct parent");
            }
        }

        return base;
    }

    Decl* SemanticAnalyzer::type_get_destructor(TypeInfo* t) {
        t = TypeInfo::get_flattened(t);

        switch (t->kind) {
            case TypeKind::Struct: {
                StructDecl& s = t->struct_.source_decl->struct_;
                if (s.field_lookup.contains("~")) {
                    return s.field_lookup.at("~");
                }

                return nullptr;
            }

            default: return nullptr;
        }
    }

} // namespace ariac