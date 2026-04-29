#include "aria/internal/compiler/semantic_analyzer/semantic_analyzer.hpp"

namespace Aria::Internal {

    void SemanticAnalyzer::ResolveVarInitializer(Decl* decl) {
        ARIA_ASSERT(decl->Kind == DeclKind::Var, "SemanticAnalyzer::ResolveVarInitializer() only supports a variable declaration");
        VarDecl& var = decl->Var;

        if (!var.Initializer) {
            return CreateDefaultInitializer(&var.Initializer, var.Type, decl->Loc, decl->Range);
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

        if (initType->IsError() || var.Type->IsError()) { return; }

        ConversionCost cost = GetConversionCost(var.Type, initType);
        if (cost.CastNeeded) {
            if (cost.ImplicitCastPossible) {
                InsertImplicitCast(var.Type, initType, var.Initializer, cost.Kind);
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
                InsertImplicitCast(paramType, argType, arg, cost.Kind);
            } else {
                m_Context->ReportCompilerDiagnostic(arg->Loc, arg->Range, fmt::format("Cannot implicitly convert from '{}' to '{}'", TypeInfoToString(argType), TypeInfoToString(paramType)));
            }
        }
    }

    void SemanticAnalyzer::CreateDefaultInitializer(Expr** expr, TypeInfo* type, SourceLocation loc, SourceRange range) {
        switch (type->Kind) {
            case TypeKind::Bool:   *expr = Expr::Create(m_Context, {}, {}, ExprKind::BooleanConstant,   ExprValueKind::RValue, type, BooleanConstantExpr(false)); break;
            case TypeKind::Char:   *expr = Expr::Create(m_Context, {}, {}, ExprKind::CharacterConstant, ExprValueKind::RValue, type, CharacterConstantExpr('\0')); break;
            case TypeKind::UChar:  *expr = Expr::Create(m_Context, {}, {}, ExprKind::CharacterConstant, ExprValueKind::RValue, type, CharacterConstantExpr('\0')); break;
            case TypeKind::Short:  *expr = Expr::Create(m_Context, {}, {}, ExprKind::IntegerConstant,   ExprValueKind::RValue, type, IntegerConstantExpr(0)); break;
            case TypeKind::UShort: *expr = Expr::Create(m_Context, {}, {}, ExprKind::IntegerConstant,   ExprValueKind::RValue, type, IntegerConstantExpr(0)); break;
            case TypeKind::Int:    *expr = Expr::Create(m_Context, {}, {}, ExprKind::IntegerConstant,   ExprValueKind::RValue, type, IntegerConstantExpr(0)); break;
            case TypeKind::UInt:   *expr = Expr::Create(m_Context, {}, {}, ExprKind::IntegerConstant,   ExprValueKind::RValue, type, IntegerConstantExpr(0)); break;
            case TypeKind::Long:   *expr = Expr::Create(m_Context, {}, {}, ExprKind::IntegerConstant,   ExprValueKind::RValue, type, IntegerConstantExpr(0)); break;
            case TypeKind::ULong:  *expr = Expr::Create(m_Context, {}, {}, ExprKind::IntegerConstant,   ExprValueKind::RValue, type, IntegerConstantExpr(0)); break;
                                                                                                                               
            case TypeKind::Float:  *expr = Expr::Create(m_Context, {}, {}, ExprKind::FloatingConstant,  ExprValueKind::RValue, type, FloatingConstantExpr(0.0)); break;
            case TypeKind::Double: *expr = Expr::Create(m_Context, {}, {}, ExprKind::FloatingConstant,  ExprValueKind::RValue, type, FloatingConstantExpr(0.0)); break;
                                                                                                                               
            case TypeKind::Ptr:    *expr = Expr::Create(m_Context, {}, {}, ExprKind::Null,              ExprValueKind::RValue, type, ErrorExpr()); break;
                                                                                                                               
            case TypeKind::String: *expr = Expr::Create(m_Context, {}, {}, ExprKind::StringConstant,    ExprValueKind::RValue, type, StringConstantExpr("")); break;

            case TypeKind::Structure: {
                const StructDeclaration& sDecl = type->Struct;

                if (sDecl.SourceDecl) {
                    ARIA_ASSERT(sDecl.SourceDecl->Kind == DeclKind::Struct, "Invalid source decl");
                    if (sDecl.SourceDecl->Struct.Definition.HasDefaultCtor) {
                        ConstructorDecl* ctor = nullptr;
                        for (auto& field : sDecl.SourceDecl->Struct.Fields) {
                            if (field->Kind == DeclKind::Constructor && field->Constructor.Parameters.Size == 0) {
                                ctor = &field->Constructor;
                            }
                        }

                        *expr = Expr::Create(m_Context, {}, {}, ExprKind::Construct, ExprValueKind::RValue, type, ConstructExpr(ctor, {}));
                        break;
                    } else {
                        m_Context->ReportCompilerDiagnostic(loc, range, fmt::format("Cannot default initialize variable of type '{}'", TypeInfoToString(type)));
                        break;
                    }
                }

                break;
            }

            case TypeKind::Unresolved: break;

            default: m_Context->ReportCompilerDiagnostic(loc, range, fmt::format("Cannot default initialize variable of type '{}'", TypeInfoToString(type)), CompilerDiagKind::Warning); break;
        }
    }

} // namespace Aria::Internal