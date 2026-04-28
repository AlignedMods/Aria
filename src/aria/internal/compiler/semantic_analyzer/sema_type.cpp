#include "aria/internal/compiler/semantic_analyzer/semantic_analyzer.hpp"

namespace Aria::Internal {

    void SemanticAnalyzer::ResolveType(SourceLocation loc, SourceRange range, TypeInfo* type) {
        if (type->Type == PrimitiveType::Unresolved) {
            UnresolvedType& t = std::get<UnresolvedType>(type->Data);
            ResolveExpr(t.Ident);

            if (!t.Ident->DeclRef.ReferencedDecl) {
                return;
            }

            if (t.Ident->DeclRef.ReferencedDecl->Kind == DeclKind::Struct) {
                type->Type = PrimitiveType::Structure;
                type->Data = StructDeclaration(t.Ident->DeclRef.Identifier, t.Ident->DeclRef.ReferencedDecl);
            } else {
                m_Context->ReportCompilerDiagnostic(loc, range, fmt::format("'{}' is not a type", t.Ident->DeclRef.Identifier));
                return;
            }
        } else if (type->Type == PrimitiveType::Ptr) {
            ResolveType(loc, range, std::get<TypeInfo*>(type->Data));
        } else if (type->Type == PrimitiveType::Function) {
            FunctionDeclaration& fn = std::get<FunctionDeclaration>(type->Data);

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
                if (TypeGetSize(src) > TypeGetSize(dst)) {
                    cost.CoKind = ConversionKind::Narrowing;
                    cost.CaKind = CastKind::Integral;
                } else if (TypeGetSize(src) < TypeGetSize(dst)) {
                    cost.CoKind = ConversionKind::Promotion;
                    cost.CaKind = CastKind::Integral;
                } else {
                    if (src->IsSigned() == dst->IsSigned()) {
                        cost.CoKind = ConversionKind::None;
                        cost.CastNeeded = false;
                    } else {
                        cost.CoKind = ConversionKind::SignChange;
                        cost.CaKind = CastKind::Integral;
                        cost.CastNeeded = true;
                    }
                }
            } else if (dst->IsFloatingPoint()) { // Int to float
                cost.CoKind = ConversionKind::Promotion;
                cost.CaKind = CastKind::IntegralToFloating;
            } else {
                cost.ImplicitCastPossible = false;
                cost.ExplicitCastPossible = false;
            }

            return cost;
        }

        if (src->IsFloatingPoint()) {
            if (dst->IsFloatingPoint()) { // Float to float
                if (TypeGetSize(src) > TypeGetSize(dst)) {
                    cost.CoKind = ConversionKind::Narrowing;
                    cost.CaKind = CastKind::Floating;
                } else if (TypeGetSize(src) < TypeGetSize(dst)) {
                    cost.CoKind = ConversionKind::Promotion;
                    cost.CaKind = CastKind::Floating;
                } else {
                    cost.CoKind = ConversionKind::None;
                    cost.CastNeeded = false;
                }
            } else if (dst->IsIntegral()) { // Float to int
                cost.ImplicitCastPossible = false;
                cost.CoKind = ConversionKind::Narrowing;
                cost.CaKind = CastKind::FloatingToIntegral;
            } else {
                cost.ExplicitCastPossible = false;
            }

            return cost;
        }

        if (src->IsPointer()) {
            if (dst->IsPointer()) { // Ptr to ptr
                if (std::get<TypeInfo*>(src->Data)->IsVoid() || std::get<TypeInfo*>(dst->Data)->IsVoid()) { // Allow void* conversions
                    cost.CoKind = ConversionKind::None;
                    cost.CaKind = CastKind::BitCast;

                    return cost;
                }
            }
        }

        cost.ExplicitCastPossible = false;
        cost.ImplicitCastPossible = false;
        return cost;
    }

    bool SemanticAnalyzer::TypeIsEqual(TypeInfo* lhs, TypeInfo* rhs) {
        if (lhs->IsTrivial() && rhs->IsTrivial()) {
            if (lhs->IsPointer() && rhs->IsPointer()) { return TypeIsEqual(std::get<TypeInfo*>(lhs->Data), std::get<TypeInfo*>(rhs->Data)); }
            return lhs->Type == rhs->Type;
        }

        if (lhs->IsString() && rhs->IsString()) {
            return true;
        }

        if (lhs->IsFunction() && rhs->IsFunction()) {
            FunctionDeclaration& fLhs = std::get<FunctionDeclaration>(lhs->Data);
            FunctionDeclaration& fRhs = std::get<FunctionDeclaration>(rhs->Data);

            if (!TypeIsEqual(fLhs.ReturnType, fRhs.ReturnType)) { return false; }
            if (fLhs.ParamTypes.Size != fRhs.ParamTypes.Size) { return false; }

            for (size_t i = 0; i < fLhs.ParamTypes.Size; i++) {
                if (!TypeIsEqual(fLhs.ParamTypes.Items[i], fRhs.ParamTypes.Items[i])) { return false; }
            }

            return true;
        }

        if (lhs->IsStructure() && rhs->IsStructure()) {
            StructDeclaration& sLhs = std::get<StructDeclaration>(lhs->Data);
            StructDeclaration& sRhs = std::get<StructDeclaration>(rhs->Data);

            return sLhs.Identifier == sRhs.Identifier;
        }

        return false;
    }

    size_t SemanticAnalyzer::TypeGetSize(TypeInfo* t) {
        switch (t->Type) {
            case PrimitiveType::Void:   return 0;

            case PrimitiveType::Bool:   return 1;

            case PrimitiveType::Char:   return 1;
            case PrimitiveType::UChar:  return 1;
            case PrimitiveType::Short:  return 2;
            case PrimitiveType::UShort: return 2;
            case PrimitiveType::Int:    return 4;
            case PrimitiveType::UInt:   return 4;
            case PrimitiveType::Long:   return 8;
            case PrimitiveType::ULong:  return 8;

            case PrimitiveType::Float:  return 4;
            case PrimitiveType::Double: return 8;

            default: ARIA_ASSERT(false, "SemanticAnalyzer::TypeGetSize() only supports trivial (non structure) types");
        }
    }

    bool SemanticAnalyzer::TypeIsTrivial(TypeInfo* t) {
        switch (t->Type) {
            case PrimitiveType::String:
                return false;

            case PrimitiveType::Structure: {
                StructDeclaration& sDecl = std::get<StructDeclaration>(t->Data);

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