#include "aria/internal/compiler/semantic_analyzer/semantic_analyzer.hpp"

namespace Aria::Internal {

    void SemanticAnalyzer::ResolveInitializer(Expr** initializer, TypeInfo* type, bool temporary) {
        if (!*initializer) {
            return CreateDefaultInitializer(initializer, type);
        } else {
            ResolveExpr(*initializer);
        }

        TypeInfo* initType = (*initializer)->Type;

        if ((*initializer)->ValueKind == ExprValueKind::LValue) {
            if (initType->Type == PrimitiveType::String) {
                ReplaceExpr(*initializer, Expr::Create(m_Context, (*initializer)->Loc, (*initializer)->Range, 
                    ExprKind::Copy, ExprValueKind::LValue, (*initializer)->Type, 
                    CopyExpr(Expr::Dup(m_Context, *initializer), m_BuiltInStringCopyConstructor)));
            }
        }

        // Do not keep going further if we do not have a type
        if (!type) { return; }

        ConversionCost cost = GetConversionCost(type, initType, (*initializer)->ValueKind);
        if (cost.CastNeeded) {
            if (cost.ImplicitCastPossible) {
                InsertImplicitCast(type, initType, (*initializer), cost.CaKind);
            } else {
                m_Context->ReportCompilerDiagnostic((*initializer)->Loc, (*initializer)->Range, fmt::format("Cannot implicitly convert from '{}' to '{}'", TypeInfoToString(initType), TypeInfoToString(type)));
            }
        }

        if (temporary) {
            if ((*initializer)->Type->Type == PrimitiveType::String) {
                ReplaceExpr((*initializer), Expr::Create(m_Context, (*initializer)->Loc, (*initializer)->Range, 
                    ExprKind::Temporary, ExprValueKind::RValue, (*initializer)->Type, 
                    TemporaryExpr(Expr::Dup(m_Context, (*initializer)), m_BuiltInStringDestructor)));
            }
        }
    }

    void SemanticAnalyzer::CreateDefaultInitializer(Expr** initializer, TypeInfo* type) {
        switch (type->Type) {
            case PrimitiveType::Bool:   *initializer = Expr::Create(m_Context, {}, {}, ExprKind::BooleanConstant,   ExprValueKind::RValue, type, BooleanConstantExpr(false)); break;
            case PrimitiveType::Char:   *initializer = Expr::Create(m_Context, {}, {}, ExprKind::CharacterConstant, ExprValueKind::RValue, type, CharacterConstantExpr('\0')); break;
            case PrimitiveType::UChar:  *initializer = Expr::Create(m_Context, {}, {}, ExprKind::CharacterConstant, ExprValueKind::RValue, type, CharacterConstantExpr('\0')); break;
            case PrimitiveType::Short:  *initializer = Expr::Create(m_Context, {}, {}, ExprKind::IntegerConstant,   ExprValueKind::RValue, type, IntegerConstantExpr(0)); break;
            case PrimitiveType::UShort: *initializer = Expr::Create(m_Context, {}, {}, ExprKind::IntegerConstant,   ExprValueKind::RValue, type, IntegerConstantExpr(0)); break;
            case PrimitiveType::Int:    *initializer = Expr::Create(m_Context, {}, {}, ExprKind::IntegerConstant,   ExprValueKind::RValue, type, IntegerConstantExpr(0)); break;
            case PrimitiveType::UInt:   *initializer = Expr::Create(m_Context, {}, {}, ExprKind::IntegerConstant,   ExprValueKind::RValue, type, IntegerConstantExpr(0)); break;
            case PrimitiveType::Long:   *initializer = Expr::Create(m_Context, {}, {}, ExprKind::IntegerConstant,   ExprValueKind::RValue, type, IntegerConstantExpr(0)); break;
            case PrimitiveType::ULong:  *initializer = Expr::Create(m_Context, {}, {}, ExprKind::IntegerConstant,   ExprValueKind::RValue, type, IntegerConstantExpr(0)); break;

            case PrimitiveType::Float:  *initializer = Expr::Create(m_Context, {}, {}, ExprKind::FloatingConstant,  ExprValueKind::RValue, type, FloatingConstantExpr(0.0)); break;
            case PrimitiveType::Double: *initializer = Expr::Create(m_Context, {}, {}, ExprKind::FloatingConstant,  ExprValueKind::RValue, type, FloatingConstantExpr(0.0)); break;

            case PrimitiveType::String: *initializer = Expr::Create(m_Context, {}, {}, ExprKind::StringConstant,    ExprValueKind::RValue, type, StringConstantExpr("")); break;

            default: ARIA_UNREACHABLE();
        }
    }

} // namespace Aria::Internal