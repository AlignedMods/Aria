#include "ariac/semantic_analyzer/semantic_analyzer.hpp"

namespace ariac {

    void SemanticAnalyzer::resolve_type(TypeInfo* type) {
        switch (type->kind) {
            case TypeKind::Unresolved: {
                UnresolvedType& t = type->unresolved;
                resolve_expr(t.ident);

                if (t.ident->decl_ref.referenced_decl->kind == DeclKind::Error) {
                    type->kind = TypeKind::Error;
                    break;
                }

                switch (t.ident->decl_ref.referenced_decl->kind) {
                    case DeclKind::Struct: {
                        if (t.ident->decl_ref.referenced_decl->resolve_status == ResolveStatus::NotStarted) {
                            CompilationUnit* old_unit = context.active_comp_unit;
                            context.active_comp_unit = t.ident->decl_ref.referenced_decl->parent_unit;
                            resolve_struct_decl(t.ident->decl_ref.referenced_decl);
                            context.active_comp_unit = old_unit;
                        } else if (t.ident->decl_ref.referenced_decl->resolve_status == ResolveStatus::InProgress) {
                            context.report_compiler_diagnostic(type->loc, "Recursive definition of struct");
                            type->kind = TypeKind::Error;
                            break;
                        }

                        type->kind = TypeKind::Structure;
                        type->struct_ = StructType(t.ident->decl_ref.identifier, t.ident->decl_ref.referenced_decl);
                        break;
                    }

                    case DeclKind::Typedef: {
                        if (t.ident->decl_ref.referenced_decl->resolve_status == ResolveStatus::NotStarted) {
                            CompilationUnit* old_unit = context.active_comp_unit;
                            context.active_comp_unit = t.ident->decl_ref.referenced_decl->parent_unit;
                            resolve_typedef_decl(t.ident->decl_ref.referenced_decl);
                            context.active_comp_unit = old_unit;
                        } else if (t.ident->decl_ref.referenced_decl->resolve_status == ResolveStatus::InProgress) {
                            context.report_compiler_diagnostic(type->loc, "Recursive definition of typedef");
                            type->kind = TypeKind::Error;
                            break;
                        }

                        type->kind = TypeKind::Typedef;
                        type->typedef_ = TypedefType(t.ident->decl_ref.identifier, t.ident->decl_ref.referenced_decl->typedef_.type, t.ident->decl_ref.referenced_decl);
                        break;
                    }

                    case DeclKind::Enum: {
                        if (t.ident->decl_ref.referenced_decl->resolve_status == ResolveStatus::NotStarted) {
                            CompilationUnit* old_unit = context.active_comp_unit;
                            context.active_comp_unit = t.ident->decl_ref.referenced_decl->parent_unit;
                            resolve_enum_decl(t.ident->decl_ref.referenced_decl);
                            context.active_comp_unit = old_unit;
                        } else if (t.ident->decl_ref.referenced_decl->resolve_status == ResolveStatus::InProgress) {
                            context.report_compiler_diagnostic(type->loc, "Recursive definition of enum");
                            type->kind = TypeKind::Error;
                            break;
                        }

                        type->kind = TypeKind::Enum;
                        type->enum_ = EnumType(t.ident->decl_ref.identifier, t.ident->decl_ref.referenced_decl);
                        break;
                    }

                    case DeclKind::GenericParameter: {
                        type->kind = TypeKind::Generic;
                        type->generic = GenericType(t.ident->decl_ref.referenced_decl->generic_parameter.identifier, t.ident->decl_ref.referenced_decl);
                        break;
                    }

                    case DeclKind::Generic: {
                        ARIA_ASSERT(t.ident->decl_ref.referenced_decl->kind == DeclKind::Generic, "Invalid generic");

                        if (t.ident->decl_ref.referenced_decl->generic.decl->kind == DeclKind::Struct) {
                            if (!m_search_generics) {
                                context.report_compiler_diagnostic(type->loc, "Cannot reference generic type without an instantiation");
                            }

                            type->kind = TypeKind::GenericDecl;
                            type->generic_decl = GenericDeclType(t.ident->decl_ref.identifier, t.ident->decl_ref.referenced_decl);
                        } else {
                            context.report_compiler_diagnostic(type->loc, fmt::format("'{}' is not a type", t.ident->decl_ref.identifier));
                        }

                        break;
                    }

                    default: context.report_compiler_diagnostic(type->loc, fmt::format("'{}' is not a type", t.ident->decl_ref.identifier)); break;
                }

                break;
            }

            case TypeKind::Pointer: resolve_type(type->pointer.base); break;
            case TypeKind::Slice: resolve_type(type->slice.base); break;
            
            case TypeKind::Array: {
                resolve_type(type->array.base);
                resolve_expr(type->array.expression);

                if (!is_const_expr(type->array.expression)) {
                    context.report_compiler_diagnostic(type->array.expression->loc, "Size of array must be a compile time constant");
                    break;
                }

                ConversionCost cost = get_conversion_cost(TypeInfo::get_basic(TypeKind::ULong), type->array.expression->type);
                if (cost.cast_needed && !cost.implicit_cast_possible) {
                    context.report_compiler_diagnostic(type->array.expression->loc, "Size of array must be convertable to 'ulong'");
                    break;
                }

                Expr* cexpr = eval_const_expr(type->array.expression);
                type->array.size = cexpr->const_.integer;
                break;
            }

            case TypeKind::Function:
            case TypeKind::Method: {
                FunctionType& fn = type->function;

                for (TypeInfo* param : fn.param_types) {
                    resolve_type(param);
                }

                resolve_type(fn.return_type);
                break;
            }

            case TypeKind::Generic: {
                GenericType& g = type->generic;
                if (m_replace_generic_types) {
                    ARIA_ASSERT(m_specialized_generic_types.contains(g.identifier), "Invalid generic specilization type");
                    *type = *m_specialized_generic_types.at(g.identifier);
                }
                break;
            }

            case TypeKind::GenericInstantiation: {
                GenericInstantiationType& gi = type->generic_instantiation;

                m_search_generics = true;
                resolve_type(gi.base);
                m_search_generics = false;

                for (TypeInfo* t : gi.arguments) {
                    resolve_type(t);
                }

                if (gi.base->kind != TypeKind::GenericDecl) {
                    context.report_compiler_diagnostic(gi.base->loc, "Non generic type cannot be used for generic instantiation");
                    break;
                }

                Decl* g = gi.base->generic_decl.generic;
                ARIA_ASSERT(g->kind == DeclKind::Generic, "Invalid generic");
                ARIA_ASSERT(g->generic.decl->kind == DeclKind::Struct, "Invalid generic");

                if (gi.arguments.size != g->generic.parameters.size) {
                    context.report_compiler_diagnostic(type->loc, fmt::format("Mismatched generic instantiation, generic expects {} arguments but got {}", g->generic.parameters.size, gi.arguments.size));
                    break;
                }

                Decl* specilization = nullptr;
                for (Decl* i : g->generic.specilizations) {
                    ARIA_ASSERT(i->kind == DeclKind::StructSpecilization, "Invalid generic specilization");

                    bool failed = false;
                    for (size_t idx = 0; idx < gi.arguments.size; idx++) {
                        if (!type_is_equal(gi.arguments.items[idx], i->struct_specilization.types.items[idx])) { failed = true; break; }
                    }

                    if (!failed) { specilization = i; }
                }

                if (!specilization) {
                    Decl* struc = Decl::dup(g->generic.decl);
                    struc->parent_module = g->parent_module;
                    struc->parent_unit = g->parent_unit;
                    specilization = Decl::Create(g->loc, DeclKind::StructSpecilization, g->visibility, StructSpecilizationDecl(gi.arguments, struc, struc->struct_.impls));
                    specilization->parent_module = g->parent_module;
                    specilization->parent_unit = g->parent_unit;
                    g->generic.specilizations.append(specilization);

                    for (size_t i = 0; i < gi.arguments.size; i++) { m_specialized_generic_types[g->generic.parameters.items[i]->generic_parameter.identifier] = gi.arguments.items[i]; }
                    bool prev_val = m_replace_generic_types;
                    m_replace_generic_types = true;
                    resolve_struct_decl(struc);
                    m_replace_generic_types = prev_val;
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

        if (dst->get_bottom_type()->is_generic() || src->get_bottom_type()->is_generic()) {
            cost.cast_needed = false;
            return cost;
        }

        if (dst->is_error() || src->is_error()) {
            cost.cast_needed = false;
            return cost;
        }

        if (dst->is_void() && src->is_void()) {
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
                    cost.kind = CastKind::Integral;
                } else {
                    if (src->is_signed() == dst->is_signed()) {
                        cost.cast_needed = false;
                    } else {
                        cost.kind = CastKind::Integral;
                        cost.cast_needed = true;
                    }
                }
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
                    cost.kind = CastKind::Floating;
                } else {
                    cost.cast_needed = false;
                }
            } else if (dst->is_integral()) { // Float to int
                cost.implicit_cast_possible = false;
                cost.kind = CastKind::FloatingToIntegral;
            } else {
                cost.explicit_cast_possible = false;
            }

            return cost;
        }

        if (dst->is_typeinfo() && src->is_typeinfo()) {
            cost.cast_needed = false;
            return cost;
        }

        if (dst->is_any() && src->is_any()) {
            cost.cast_needed = false;
            return cost;
        }

        if (src->is_pointer()) {
            if (dst->is_pointer()) { // Ptr to ptr
                if (src->pointer.is_const && !dst->pointer.is_const) { // *const int -> *int is not allowed
                    cost.implicit_cast_possible = false;
                    cost.explicit_cast_possible = false;
                    return cost;
                }

                if (src->pointer.base->is_void() || dst->pointer.base->is_void()) { // Allow void* conversions
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
            if (dst->is_pointer() && type_is_equal(dst->pointer.base, src->array.base)) {
                cost.implicit_cast_possible = false; // Allow only explicit casts here
                cost.kind = CastKind::ArrayToPointer;
                return cost;
            }

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

        if (src->is_slice() && dst->is_slice()) {
            return get_conversion_cost(dst->slice.base, src->slice.base);
        }

        if (src->is_structure() && dst->is_structure()) {
            if (src->struct_.source_decl != dst->struct_.source_decl) {
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

        if (lhs->is_enum() && rhs->is_enum()) { return true; }

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
            if (fLhs.param_types.size != fRhs.param_types.size) { return false; }

            for (size_t i = 0; i < fLhs.param_types.size; i++) {
                if (!type_is_equal(fLhs.param_types.items[i], fRhs.param_types.items[i])) { return false; }
            }

            return true;
        }

        if (lhs->is_structure() && rhs->is_structure()) {
            StructType& sLhs = lhs->struct_;
            StructType& sRhs = rhs->struct_;

            return sLhs.identifier == sRhs.identifier;
        }

        return false;
    }

    bool SemanticAnalyzer::type_is_trivial(TypeInfo* t) {
        switch (t->kind) {
            case TypeKind::Structure: {
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
            case CastKind::ArrayToPointer:
            case CastKind::LValueToRValue: return false;

            default: return true;
        }
    }

} // namespace ariac