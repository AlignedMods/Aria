#include "aria/internal/compiler/semantic_analyzer/semantic_analyzer.hpp"

namespace Aria::Internal {

    void SemanticAnalyzer::ResolveVarInitializer(Decl* decl) {
        ARIA_ASSERT(decl->Kind == DeclKind::Var, "SemanticAnalyzer::ResolveVarInitializer() only supports a variable declaration");
        VarDecl& var = decl->Var;

        if (!var.Initializer) {
            return CreateDefaultInitializer(decl);
        } else {
            ResolveExpr(var.Initializer);
        }

        TypeInfo* initType = var.Initializer->Type;
        RequireRValue(var.Initializer);

        // Handle type inferrence here
        if (!var.Type) {
            if (initType->IsVoid()) {
                m_Context->ReportCompilerDiagnostic(decl->Loc, decl->Range, "Cannot create variable of void type");
            }
            var.Type = initType;
        }

        ConversionCost cost = GetConversionCost(var.Type, initType);
        if (cost.CastNeeded) {
            if (cost.ImplicitCastPossible) {
                InsertImplicitCast(var.Type, initType, var.Initializer, cost.CaKind);
            } else {
                m_Context->ReportCompilerDiagnostic(var.Initializer->Loc, var.Initializer->Range, fmt::format("Cannot implicitly convert from '{}' to '{}'", TypeInfoToString(initType), TypeInfoToString(var.Type)));
            }
        }
    }

    void SemanticAnalyzer::ResolveParamInitializer(TypeInfo* paramType, Expr* arg) {
        m_TemporaryContext = true;
        ResolveExpr(arg);

        TypeInfo* argType = arg->Type;
        RequireRValue(arg);
        m_TemporaryContext = false;

        ConversionCost cost = GetConversionCost(paramType, argType);
        if (cost.CastNeeded) {
            if (cost.ImplicitCastPossible) {
                InsertImplicitCast(paramType, argType, arg, cost.CaKind);
            } else {
                m_Context->ReportCompilerDiagnostic(arg->Loc, arg->Range, fmt::format("Cannot implicitly convert from '{}' to '{}'", TypeInfoToString(argType), TypeInfoToString(paramType)));
            }
        }
    }

    void SemanticAnalyzer::CreateDefaultInitializer(Decl* decl) {
        ARIA_ASSERT(decl->Kind == DeclKind::Var, "SemanticAnalyzer::CreateDefaultInitializer() only supports a variable declaration");
        VarDecl& var = decl->Var;

        switch (var.Type->Type) {
            case PrimitiveType::Bool:   var.Initializer = Expr::Create(m_Context, {}, {}, ExprKind::BooleanConstant,   ExprValueKind::RValue, var.Type, BooleanConstantExpr(false)); break;
            case PrimitiveType::Char:   var.Initializer = Expr::Create(m_Context, {}, {}, ExprKind::CharacterConstant, ExprValueKind::RValue, var.Type, CharacterConstantExpr('\0')); break;
            case PrimitiveType::UChar:  var.Initializer = Expr::Create(m_Context, {}, {}, ExprKind::CharacterConstant, ExprValueKind::RValue, var.Type, CharacterConstantExpr('\0')); break;
            case PrimitiveType::Short:  var.Initializer = Expr::Create(m_Context, {}, {}, ExprKind::IntegerConstant,   ExprValueKind::RValue, var.Type, IntegerConstantExpr(0)); break;
            case PrimitiveType::UShort: var.Initializer = Expr::Create(m_Context, {}, {}, ExprKind::IntegerConstant,   ExprValueKind::RValue, var.Type, IntegerConstantExpr(0)); break;
            case PrimitiveType::Int:    var.Initializer = Expr::Create(m_Context, {}, {}, ExprKind::IntegerConstant,   ExprValueKind::RValue, var.Type, IntegerConstantExpr(0)); break;
            case PrimitiveType::UInt:   var.Initializer = Expr::Create(m_Context, {}, {}, ExprKind::IntegerConstant,   ExprValueKind::RValue, var.Type, IntegerConstantExpr(0)); break;
            case PrimitiveType::Long:   var.Initializer = Expr::Create(m_Context, {}, {}, ExprKind::IntegerConstant,   ExprValueKind::RValue, var.Type, IntegerConstantExpr(0)); break;
            case PrimitiveType::ULong:  var.Initializer = Expr::Create(m_Context, {}, {}, ExprKind::IntegerConstant,   ExprValueKind::RValue, var.Type, IntegerConstantExpr(0)); break;

            case PrimitiveType::Float:  var.Initializer = Expr::Create(m_Context, {}, {}, ExprKind::FloatingConstant,  ExprValueKind::RValue, var.Type, FloatingConstantExpr(0.0)); break;
            case PrimitiveType::Double: var.Initializer = Expr::Create(m_Context, {}, {}, ExprKind::FloatingConstant,  ExprValueKind::RValue, var.Type, FloatingConstantExpr(0.0)); break;

            case PrimitiveType::String: var.Initializer = Expr::Create(m_Context, {}, {}, ExprKind::StringConstant,    ExprValueKind::RValue, var.Type, StringConstantExpr("")); break;

            case PrimitiveType::Structure: {
                const StructDeclaration& sDecl = std::get<StructDeclaration>(var.Type->Data);

                if (sDecl.SourceDecl) {
                    ARIA_ASSERT(sDecl.SourceDecl->Kind == DeclKind::Struct, "Invalid source decl");
                    if (sDecl.SourceDecl->Struct.Definition.HasDefaultCtor) {
                        ConstructorDecl* ctor = nullptr;
                        for (auto& field : sDecl.SourceDecl->Struct.Fields) {
                            if (field->Kind == DeclKind::Constructor && field->Constructor.Parameters.Size == 0) {
                                ctor = &field->Constructor;
                            }
                        }

                        var.Initializer = Expr::Create(m_Context, {}, {}, ExprKind::Construct, ExprValueKind::RValue, var.Type, ConstructExpr(ctor, {}));
                        break;
                    } else {
                        m_Context->ReportCompilerDiagnostic(decl->Loc, decl->Range, fmt::format("Cannot default initialize variable of type '{}'", TypeInfoToString(var.Type)));
                        break;
                    }
                }

                break;
            }

            case PrimitiveType::Unresolved: break;

            default: m_Context->ReportCompilerDiagnostic(decl->Loc, decl->Range, fmt::format("Cannot default initialize variable of type '{}'", TypeInfoToString(var.Type)), CompilerDiagKind::Warning); break;
        }
    }

} // namespace Aria::Internal