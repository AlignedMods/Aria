#include "internal/compiler/semantic_analyzer/type_checker.hpp"

namespace BlackLua::Internal {

    TypeChecker::TypeChecker(Context* ctx) {
        m_Context = ctx;
    }

    VariableType* TypeChecker::GetVariableTypeFromString(StringView str) {
        size_t bracket = str.Find('[');

        std::string isolatedType;
        bool array = false;

        if (bracket != StringView::npos) {
            isolatedType = fmt::format("{}", str.SubStr(0, bracket));
            array = true;
        } else {
            isolatedType = fmt::format("{}", str);
        }

        #define TYPE(str, t, _signed) if (isolatedType == str) { type->Type = PrimitiveType::t; type->Data = _signed; type->LValue = true; }

        VariableType* type = CreateVarType(m_Context, PrimitiveType::Invalid);
        TYPE("void",   Void, true);
        TYPE("bool",   Bool, true);
        TYPE("char",   Char, true);
        TYPE("uchar",  Char, false);
        TYPE("short",  Short, true);
        TYPE("ushort", Short, false);
        TYPE("int",    Int, true);
        TYPE("uint",   Int, false);
        TYPE("long",   Long, true);
        TYPE("ulong",  Long, false);
        TYPE("float",  Float, true);
        TYPE("double", Double, true);
        TYPE("string", String, false);

        #undef TYPE

        // Handle user defined type
        if (type->Type == PrimitiveType::Invalid) {
            BLUA_ASSERT(false, "todo");
            // if (m_DeclaredStructs.contains(isolatedType)) {
            //     type->Type = PrimitiveType::Structure;
            //     type->Data = m_DeclaredStructs.at(isolatedType);
            // } else {
            //     ErrorUndeclaredIdentifier(StringView(isolatedType.c_str(), isolatedType.size()), 0, 0);
            // }
        }

        if (array) {
            BLUA_ASSERT(false, "todo");
            // ArrayDeclaration decl;
            // decl.Type = type;
            // 
            // VariableType* arrType = CreateVarType(m_Context, PrimitiveType::Array, decl);
            // type = arrType;
        }

        return type;
    }

    ConversionCost TypeChecker::GetConversionCost(VariableType* dst, VariableType* src) {
        ConversionCost cost{};
        cost.CastNeeded = true;
        cost.ExplicitCastPossible = true;
        cost.ImplicitCastPossible = true;

        if (src->IsIntegral()) {
            if (dst->IsIntegral()) { // Int to int
                if (GetTypeSize(src) > GetTypeSize(dst)) {
                    cost.ConversionType = ConversionType::Narrowing;
                    cost.CastType = CastType::Integral;
                } else if (GetTypeSize(src) < GetTypeSize(dst)) {
                    cost.ConversionType = ConversionType::Promotion;
                    cost.CastType = CastType::Integral;
                } else {
                    if (src->IsSigned() == dst->IsSigned()) {
                        cost.ConversionType = ConversionType::None;
                        cost.CastNeeded = false;
                    } else {
                        cost.ConversionType = ConversionType::SignChange;
                        cost.CastNeeded = true;
                    }
                }
            } else if (dst->IsFloatingPoint()) { // Int to float
                cost.ConversionType = ConversionType::Promotion;
                cost.CastType = CastType::IntegralToFloating;
            } else {
                cost.ExplicitCastPossible = false;
            }
        }

        if (src->IsFloatingPoint()) {
            if (dst->IsFloatingPoint()) { // Float to float
                if (GetTypeSize(src) > GetTypeSize(dst)) {
                    cost.ConversionType = ConversionType::Narrowing;
                    cost.CastType = CastType::Floating;
                } else if (GetTypeSize(src) < GetTypeSize(dst)) {
                    cost.ConversionType = ConversionType::Promotion;
                    cost.CastType = CastType::Floating;
                } else {
                    cost.ConversionType = ConversionType::None;
                    cost.CastNeeded = false;
                }
            } else if (dst->IsIntegral()) { // Float to int
                cost.ImplicitCastPossible = false;
                cost.ConversionType = ConversionType::Narrowing;
                cost.CastType = CastType::FloatingToIntegral;
            } else {
                cost.ExplicitCastPossible = false;
            }
        }

        return cost;
    }

    void TypeChecker::InsertImplicitCast(VariableType* type1, VariableType* type2, NodeExpr* expr1, NodeExpr* expr2) {
        bool canCast1 = true;
        bool canCast2 = true;

        if (type1->LValue) {
            canCast1 = false;
        } else if (type2->LValue) {
            canCast2 = false;
        }

        // We check which conversion is the best
        if (canCast1 && !canCast2) { // Only "1" can be casted
            ConversionCost cost = GetConversionCost(type2, type1);
            if (!cost.ImplicitCastPossible) {
                m_Context->ReportCompilerError(expr1->Loc.Line, expr1->Loc.Column, 
                                               expr1->Range.Start.Line, expr1->Range.Start.Column,
                                               expr1->Range.End.Line, expr1->Range.End.Column,
                                               fmt::format("Cannot implicitly cast from {} to {}", VariableTypeToString(type1), VariableTypeToString(type2)));
                return;
            }

            if (cost.CastNeeded) {
                NodeExpr* copy = Allocate<NodeExpr>(*expr1);

                ExprImplicitCast* cast = Allocate<ExprImplicitCast>();
                cast->Expression = copy;
                cast->ResolvedCastType = cost.CastType;
                cast->ResolvedSrcType = type1;
                cast->ResolvedDstType = type2;

                expr1->Data = cast;
            }
        } else if (canCast2 && !canCast1) { // Only "2" can be casted
            ConversionCost cost = GetConversionCost(type1, type2);
            if (!cost.ImplicitCastPossible) {
                m_Context->ReportCompilerError(expr2->Loc.Line, expr2->Loc.Column, 
                                               expr2->Range.Start.Line, expr2->Range.Start.Column,
                                               expr2->Range.End.Line, expr2->Range.End.Column,
                                               fmt::format("Cannot implicitly cast from {} to {}", VariableTypeToString(type2), VariableTypeToString(type1)));
                return;
            }

            if (cost.CastNeeded) {
                NodeExpr* copy = Allocate<NodeExpr>(*expr2);

                ExprImplicitCast* cast = Allocate<ExprImplicitCast>();
                cast->Expression = copy;
                cast->ResolvedCastType = cost.CastType;
                cast->ResolvedSrcType = type2;
                cast->ResolvedDstType = type1;

                expr2->Data = cast;
            }
        } else if (!canCast1 && !canCast2) { // Neither can be casted, the types must be equal in this case
            ConversionCost cost = GetConversionCost(type1, type2);

            if (cost.CastNeeded) {
                m_Context->ReportCompilerError(expr2->Loc.Line, expr2->Loc.Column, 
                                               expr2->Range.Start.Line, expr2->Range.Start.Column,
                                               expr2->Range.End.Line, expr2->Range.End.Column,
                                               fmt::format("Cannot implicitly cast from {} to {}", VariableTypeToString(type2), VariableTypeToString(type1)));
                return;
            }
        } else { // Both can be casted, we choose whichever conversion is more desirable
            ConversionCost cost1 = GetConversionCost(type1, type2); // Try to cast to type1
            ConversionCost cost2 = GetConversionCost(type2, type1); // Try to cast to type2

            BLUA_ASSERT(cost1.CastNeeded == cost2.CastNeeded, "BOTH sides must either require a cast or not require one (must be the same)");

            if (cost1.CastNeeded) {
                if (cost1.ImplicitCastPossible && cost1.ConversionType == ConversionType::Promotion) { // Casting to type1 is desirable
                    NodeExpr* copy = Allocate<NodeExpr>(*expr2);

                    ExprImplicitCast* cast = Allocate<ExprImplicitCast>();
                    cast->Expression = copy;
                    cast->ResolvedCastType = cost1.CastType;
                    cast->ResolvedSrcType = type2;
                    cast->ResolvedDstType = type1;

                    expr2->Data = cast;
                    return;
                } else if (cost2.ImplicitCastPossible && cost2.ConversionType == ConversionType::Promotion) { // Casting to type2 is desirable
                    NodeExpr* copy = Allocate<NodeExpr>(*expr1);

                    ExprImplicitCast* cast = Allocate<ExprImplicitCast>();
                    cast->Expression = copy;
                    cast->ResolvedCastType = cost1.CastType;
                    cast->ResolvedSrcType = type1;
                    cast->ResolvedDstType = type2;

                    expr1->Data = cast;
                    return;
                }

                m_Context->ReportCompilerError(expr2->Loc.Line, expr2->Loc.Column, 
                                               expr2->Range.Start.Line, expr2->Range.Start.Column,
                                               expr2->Range.End.Line, expr2->Range.End.Column,
                                               fmt::format("Cannot implicitly cast from {} to {}", VariableTypeToString(type2), VariableTypeToString(type1)));
            }
        }
    }

    VariableType* TypeChecker::RequireRValue(VariableType* type, NodeExpr* expr) {
        // Perform an implicit cast if needed
        if (type->LValue) {
            NodeExpr* copy = Allocate<NodeExpr>(*expr);
            
            ExprImplicitCast* cast = Allocate<ExprImplicitCast>();
            cast->Expression = copy;
            cast->ResolvedCastType = CastType::LValueToRValue;
            cast->ResolvedSrcType = type;
            cast->ResolvedDstType = CreateVarType(m_Context, type->Type, false, type->Data);

            expr->Data = cast;

            return cast->ResolvedDstType;
        }

        return type;
    }

    VariableType* TypeChecker::RequireLValue(VariableType* type, NodeExpr* expr) {
        if (!type->LValue) {
            m_Context->ReportCompilerError(expr->Range.Start.Line, expr->Range.Start.Column,
                                           expr->Range.End.Line, expr->Range.End.Column,
                                           expr->Loc.Line, expr->Loc.Column,
                                           "Expression must be a modifiable lvalue");

            return nullptr;
        }

        return type;
    }

} // namespace BlackLua::Internal