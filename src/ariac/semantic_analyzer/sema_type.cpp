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
                        type->generic = GenericType(t.ident->decl_ref.referenced_decl->generic_parameter.identifier);
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

            case TypeKind::Pointer: resolve_type(type->base); break;
            case TypeKind::Slice: resolve_type(type->base); break;
            
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

        if (dst->kind == TypeKind::Generic || src->kind == TypeKind::Generic) {
            cost.cast_needed = false;
            return cost;
        }

        if (type_is_equal(src, dst)) {
            cost.cast_needed = false;
            return cost;
        }

        if (src->is_integral()) {
            if (dst->is_integral()) { // Int to int
                if (type_get_size(src) != type_get_size(dst)) {
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
                if (type_get_size(src) != type_get_size(dst)) {
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

        if (src->is_pointer()) {
            if (dst->is_pointer()) { // Ptr to ptr
                if (src->base->is_void() || dst->base->is_void()) { // Allow void* conversions
                    cost.kind = CastKind::BitCast;
                    return cost;
                } else if (src->base->kind == TypeKind::Generic || dst->base->kind == TypeKind::Generic) {
                    cost.cast_needed = false;
                    return cost;
                }
            }
        }

        if (src->is_array()) {
            if (dst->is_slice() && type_is_equal(dst->base, src->array.base)) {
                cost.kind = CastKind::ArrayToSlice;
                return cost;
            } else if (dst->is_pointer() && type_is_equal(dst->base, src->array.base)) {
                cost.kind = CastKind::ArrayToPointer;
                return cost;
            }
        }

        cost.explicit_cast_possible = false;
        cost.implicit_cast_possible = false;
        return cost;
    }

    bool SemanticAnalyzer::type_is_equal(TypeInfo* lhs, TypeInfo* rhs) {
        while (lhs->is_typedef()) { lhs = lhs->typedef_.base; }
        while (rhs->is_typedef()) { rhs = rhs->typedef_.base; }

        if (lhs->is_enum() && rhs->is_enum()) { return true; }

        if (lhs->is_array() && rhs->is_array()) {
            return type_is_equal(lhs->array.base, rhs->array.base) && lhs->array.size == rhs->array.size;
        }

        if (lhs->is_slice() && rhs->is_slice()) {
            return type_is_equal(lhs->base, rhs->base);
        }

        if (lhs->is_pointer() && rhs->is_pointer()) {
            return type_is_equal(lhs->base, rhs->base);
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

    size_t SemanticAnalyzer::type_get_size(TypeInfo* t) {
        switch (t->kind) {
            case TypeKind::Void:   return 0;

            case TypeKind::Bool:   return 1;

            case TypeKind::Char:   return 1;
            case TypeKind::IChar:  return 1;
            case TypeKind::Short:  return 2;
            case TypeKind::UShort: return 2;
            case TypeKind::Int:    return 4;
            case TypeKind::UInt:   return 4;
            case TypeKind::Long:   return 8;
            case TypeKind::ULong:  return 8;

            case TypeKind::Float:  return 4;
            case TypeKind::Double: return 8;

            default: ARIA_ASSERT(false, "SemanticAnalyzer::type_get_size() only supports trivial (non structure) types");
        }
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
            case CastKind::ArrayToSlice:
            case CastKind::ArrayToPointer:
            case CastKind::LValueToRValue: return false;

            default: return true;
        }
    }

} // namespace ariac