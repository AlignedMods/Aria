#include "aria/internal/compiler/semantic_analyzer/semantic_analyzer.hpp"

namespace Aria::Internal {

    void SemanticAnalyzer::resolve_type(SourceLocation loc, SourceRange range, TypeInfo* type) {
        if (type->kind == TypeKind::Unresolved) {
            UnresolvedType& t = type->unresolved;
            resolve_expr(t.ident);

            if (!t.ident->decl_ref.referenced_decl) {
                return;
            }

            if (t.ident->decl_ref.referenced_decl->kind == DeclKind::Struct) {
                type->kind = TypeKind::Structure;
                type->struct_ = StructDeclaration(t.ident->decl_ref.identifier, t.ident->decl_ref.referenced_decl);
            } else {
                m_context->report_compiler_diagnostic(loc, range, fmt::format("'{}' is not a type", t.ident->decl_ref.identifier));
                return;
            }
        } else if (type->kind == TypeKind::Ptr) {
            resolve_type(loc, range, type->base);
        } else if (type->kind == TypeKind::Array) {
            ARIA_TODO("Resolving array types");
        } else if (type->kind == TypeKind::Function) {
            FunctionDeclaration& fn = type->function;

            for (TypeInfo* param : fn.param_types) {
                resolve_type(loc, range, param);
            }

            resolve_type(loc, range, fn.return_type);
        }
    }

    ConversionCost SemanticAnalyzer::get_conversion_cost(TypeInfo* dst, TypeInfo* src) {
        ConversionCost cost{};
        cost.cast_needed = true;
        cost.explicit_cast_possible = true;
        cost.implicit_cast_possible = true;

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
                }
            }
        }

        if (src->is_array()) {
            if (dst->is_slice()) {
                cost.kind = CastKind::ArrayToSlice;
                return cost;
            }
        }

        cost.explicit_cast_possible = false;
        cost.implicit_cast_possible = false;
        return cost;
    }

    bool SemanticAnalyzer::type_is_equal(TypeInfo* lhs, TypeInfo* rhs) {
        if (lhs->is_primitive() && rhs->is_primitive()) {
            if (lhs->is_pointer() && rhs->is_pointer()) { return type_is_equal(lhs->base, rhs->base); }
            return lhs->kind == rhs->kind;
        }

        if (lhs->is_string() && rhs->is_string()) {
            return true;
        }

        if (lhs->is_function() && rhs->is_function()) {
            FunctionDeclaration& fLhs = lhs->function;
            FunctionDeclaration& fRhs = rhs->function;

            if (!type_is_equal(fLhs.return_type, fRhs.return_type)) { return false; }
            if (fLhs.param_types.size != fRhs.param_types.size) { return false; }

            for (size_t i = 0; i < fLhs.param_types.size; i++) {
                if (!type_is_equal(fLhs.param_types.items[i], fRhs.param_types.items[i])) { return false; }
            }

            return true;
        }

        if (lhs->is_structure() && rhs->is_structure()) {
            StructDeclaration& sLhs = lhs->struct_;
            StructDeclaration& sRhs = rhs->struct_;

            return sLhs.identifier == sRhs.identifier;
        }

        return false;
    }

    size_t SemanticAnalyzer::type_get_size(TypeInfo* t) {
        switch (t->kind) {
            case TypeKind::Void:   return 0;

            case TypeKind::Bool:   return 1;

            case TypeKind::Char:   return 1;
            case TypeKind::UChar:  return 1;
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
            case TypeKind::String:
                return false;

            case TypeKind::Structure: {
                StructDeclaration& sDecl = t->struct_;

                if (sDecl.source_decl) {
                    ARIA_ASSERT(sDecl.source_decl->kind == DeclKind::Struct, "Invalid source decl");
                    return sDecl.source_decl->struct_.definition.trivial_dtor;
                }

                return true;
            }

            default: return true;
        }
    }

} // namespace Aria::Internal