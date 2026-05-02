#include "aria/internal/compiler/semantic_analyzer/semantic_analyzer.hpp"

namespace Aria::Internal {

    void SemanticAnalyzer::resolve_type(SourceLocation loc, SourceRange range, TypeInfo* type) {
        if (type->Kind == TypeKind::Unresolved) {
            UnresolvedType& t = type->Unresolved;
            resolve_expr(t.Ident);

            if (!t.Ident->DeclRef.ReferencedDecl) {
                return;
            }

            if (t.Ident->DeclRef.ReferencedDecl->Kind == DeclKind::Struct) {
                type->Kind = TypeKind::Structure;
                type->Struct = StructDeclaration(t.Ident->DeclRef.Identifier, t.Ident->DeclRef.ReferencedDecl);
            } else {
                m_Context->report_compiler_diagnostic(loc, range, fmt::format("'{}' is not a type", t.Ident->DeclRef.Identifier));
                return;
            }
        } else if (type->Kind == TypeKind::Ptr) {
            resolve_type(loc, range, type->Base);
        } else if (type->Kind == TypeKind::Array) {
            ARIA_TODO("Resolving array types");
        } else if (type->Kind == TypeKind::Function) {
            FunctionDeclaration& fn = type->Function;

            for (TypeInfo* param : fn.ParamTypes) {
                resolve_type(loc, range, param);
            }

            resolve_type(loc, range, fn.ReturnType);
        }
    }

    ConversionCost SemanticAnalyzer::get_conversion_cost(TypeInfo* dst, TypeInfo* src) {
        ConversionCost cost{};
        cost.CastNeeded = true;
        cost.ExplicitCastPossible = true;
        cost.ImplicitCastPossible = true;

        if (type_is_equal(src, dst)) {
            cost.CastNeeded = false;
            return cost;
        }

        if (src->is_integral()) {
            if (dst->is_integral()) { // Int to int
                if (type_get_size(src) != type_get_size(dst)) {
                    cost.Kind = CastKind::Integral;
                } else {
                    if (src->is_signed() == dst->is_signed()) {
                        cost.CastNeeded = false;
                    } else {
                        cost.Kind = CastKind::Integral;
                        cost.CastNeeded = true;
                    }
                }
            } else if (dst->is_floating_point()) { // Int to float
                cost.Kind = CastKind::IntegralToFloating;
            } else {
                cost.ImplicitCastPossible = false;
                cost.ExplicitCastPossible = false;
            }

            return cost;
        }

        if (src->is_floating_point()) {
            if (dst->is_floating_point()) { // Float to float
                if (type_get_size(src) != type_get_size(dst)) {
                    cost.Kind = CastKind::Floating;
                } else {
                    cost.CastNeeded = false;
                }
            } else if (dst->is_integral()) { // Float to int
                cost.ImplicitCastPossible = false;
                cost.Kind = CastKind::FloatingToIntegral;
            } else {
                cost.ExplicitCastPossible = false;
            }

            return cost;
        }

        if (src->is_pointer()) {
            if (dst->is_pointer()) { // Ptr to ptr
                if (src->Base->is_void() || dst->Base->is_void()) { // Allow void* conversions
                    cost.Kind = CastKind::BitCast;
                    return cost;
                }
            }
        }

        if (src->is_array()) {
            if (dst->is_slice()) {
                cost.Kind = CastKind::ArrayToSlice;
                return cost;
            }
        }

        cost.ExplicitCastPossible = false;
        cost.ImplicitCastPossible = false;
        return cost;
    }

    bool SemanticAnalyzer::type_is_equal(TypeInfo* lhs, TypeInfo* rhs) {
        if (lhs->is_primitive() && rhs->is_primitive()) {
            if (lhs->is_pointer() && rhs->is_pointer()) { return type_is_equal(lhs->Base, rhs->Base); }
            return lhs->Kind == rhs->Kind;
        }

        if (lhs->is_string() && rhs->is_string()) {
            return true;
        }

        if (lhs->is_function() && rhs->is_function()) {
            FunctionDeclaration& fLhs = lhs->Function;
            FunctionDeclaration& fRhs = rhs->Function;

            if (!type_is_equal(fLhs.ReturnType, fRhs.ReturnType)) { return false; }
            if (fLhs.ParamTypes.Size != fRhs.ParamTypes.Size) { return false; }

            for (size_t i = 0; i < fLhs.ParamTypes.Size; i++) {
                if (!type_is_equal(fLhs.ParamTypes.Items[i], fRhs.ParamTypes.Items[i])) { return false; }
            }

            return true;
        }

        if (lhs->is_structure() && rhs->is_structure()) {
            StructDeclaration& sLhs = lhs->Struct;
            StructDeclaration& sRhs = rhs->Struct;

            return sLhs.Identifier == sRhs.Identifier;
        }

        return false;
    }

    size_t SemanticAnalyzer::type_get_size(TypeInfo* t) {
        switch (t->Kind) {
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
        switch (t->Kind) {
            case TypeKind::String:
                return false;

            case TypeKind::Structure: {
                StructDeclaration& sDecl = t->Struct;

                if (sDecl.SourceDecl) {
                    ARIA_ASSERT(sDecl.SourceDecl->Kind == DeclKind::Struct, "Invalid source decl");
                    return sDecl.SourceDecl->Struct.Definition.TrivialDtor;
                }

                return true;
            }

            default: return true;
        }
    }

} // namespace Aria::Internal