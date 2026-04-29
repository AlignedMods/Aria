#include "aria/internal/compiler/semantic_analyzer/semantic_analyzer.hpp"

namespace Aria::Internal {

    void SemanticAnalyzer::ResolveType(SourceLocation loc, SourceRange range, TypeInfo* type) {
        if (type->Kind == TypeKind::Unresolved) {
            UnresolvedType& t = type->Unresolved;
            ResolveExpr(t.Ident);

            if (!t.Ident->DeclRef.ReferencedDecl) {
                return;
            }

            if (t.Ident->DeclRef.ReferencedDecl->Kind == DeclKind::Struct) {
                type->Kind = TypeKind::Structure;
                type->Struct = StructDeclaration(t.Ident->DeclRef.Identifier, t.Ident->DeclRef.ReferencedDecl);
            } else {
                m_Context->ReportCompilerDiagnostic(loc, range, fmt::format("'{}' is not a type", t.Ident->DeclRef.Identifier));
                return;
            }
        } else if (type->Kind == TypeKind::Ptr) {
            ResolveType(loc, range, type->Base);
        } else if (type->Kind == TypeKind::Array) {
            ARIA_TODO("Resolving array types");
        } else if (type->Kind == TypeKind::Function) {
            FunctionDeclaration& fn = type->Function;

            for (TypeInfo* param : fn.ParamTypes) {
                ResolveType(loc, range, param);
            }

            ResolveType(loc, range, fn.ReturnType);
        }
    }

    ConversionCost SemanticAnalyzer::GetConversionCost(TypeInfo* dst, TypeInfo* src) {
        ConversionCost cost{};
        cost.CastNeeded = true;
        cost.ExplicitCastPossible = true;
        cost.ImplicitCastPossible = true;

        if (TypeIsEqual(src, dst)) {
            cost.CastNeeded = false;
            return cost;
        }

        if (src->IsIntegral()) {
            if (dst->IsIntegral()) { // Int to int
                if (TypeGetSize(src) != TypeGetSize(dst)) {
                    cost.Kind = CastKind::Integral;
                } else {
                    if (src->IsSigned() == dst->IsSigned()) {
                        cost.CastNeeded = false;
                    } else {
                        cost.Kind = CastKind::Integral;
                        cost.CastNeeded = true;
                    }
                }
            } else if (dst->IsFloatingPoint()) { // Int to float
                cost.Kind = CastKind::IntegralToFloating;
            } else {
                cost.ImplicitCastPossible = false;
                cost.ExplicitCastPossible = false;
            }

            return cost;
        }

        if (src->IsFloatingPoint()) {
            if (dst->IsFloatingPoint()) { // Float to float
                if (TypeGetSize(src) != TypeGetSize(dst)) {
                    cost.Kind = CastKind::Floating;
                } else {
                    cost.CastNeeded = false;
                }
            } else if (dst->IsIntegral()) { // Float to int
                cost.ImplicitCastPossible = false;
                cost.Kind = CastKind::FloatingToIntegral;
            } else {
                cost.ExplicitCastPossible = false;
            }

            return cost;
        }

        if (src->IsPointer()) {
            if (dst->IsPointer()) { // Ptr to ptr
                if (src->Base->IsVoid() || dst->Base->IsVoid()) { // Allow void* conversions
                    cost.Kind = CastKind::BitCast;
                    return cost;
                }
            }
        }

        if (src->IsArray()) {
            if (dst->IsSlice()) {
                cost.Kind = CastKind::ArrayToSlice;
                return cost;
            }
        }

        cost.ExplicitCastPossible = false;
        cost.ImplicitCastPossible = false;
        return cost;
    }

    bool SemanticAnalyzer::TypeIsEqual(TypeInfo* lhs, TypeInfo* rhs) {
        if (lhs->IsPrimitive() && rhs->IsPrimitive()) {
            if (lhs->IsPointer() && rhs->IsPointer()) { return TypeIsEqual(lhs->Base, rhs->Base); }
            return lhs->Kind == rhs->Kind;
        }

        if (lhs->IsString() && rhs->IsString()) {
            return true;
        }

        if (lhs->IsFunction() && rhs->IsFunction()) {
            FunctionDeclaration& fLhs = lhs->Function;
            FunctionDeclaration& fRhs = rhs->Function;

            if (!TypeIsEqual(fLhs.ReturnType, fRhs.ReturnType)) { return false; }
            if (fLhs.ParamTypes.Size != fRhs.ParamTypes.Size) { return false; }

            for (size_t i = 0; i < fLhs.ParamTypes.Size; i++) {
                if (!TypeIsEqual(fLhs.ParamTypes.Items[i], fRhs.ParamTypes.Items[i])) { return false; }
            }

            return true;
        }

        if (lhs->IsStructure() && rhs->IsStructure()) {
            StructDeclaration& sLhs = lhs->Struct;
            StructDeclaration& sRhs = rhs->Struct;

            return sLhs.Identifier == sRhs.Identifier;
        }

        return false;
    }

    size_t SemanticAnalyzer::TypeGetSize(TypeInfo* t) {
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

            default: ARIA_ASSERT(false, "SemanticAnalyzer::TypeGetSize() only supports trivial (non structure) types");
        }
    }

    bool SemanticAnalyzer::TypeIsTrivial(TypeInfo* t) {
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