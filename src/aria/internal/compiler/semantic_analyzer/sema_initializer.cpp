#include "aria/internal/compiler/semantic_analyzer/semantic_analyzer.hpp"

namespace Aria::Internal {

    void SemanticAnalyzer::resolve_var_initializer(Decl* decl) {
        ARIA_ASSERT(decl->Kind == DeclKind::Var, "SemanticAnalyzer::resolve_var_initializer() only supports a variable declaration");
        VarDecl& var = decl->Var;

        if (!var.Initializer) {
            return create_default_initializer(&var.Initializer, var.Type, decl->Loc, decl->Range);
        } else {
            resolve_expr(var.Initializer);
        }

        TypeInfo* initType = var.Initializer->Type;
        require_rvalue(var.Initializer);

        // Handle type inferrence here
        if (!var.Type) {
            if (initType->is_void()) {
                m_Context->report_compiler_diagnostic(decl->Loc, decl->Range, "Cannot create variable of void type");
            }
            var.Type = initType;
        }

        if (initType->is_error() || var.Type->is_error()) { return; }

        ConversionCost cost = get_conversion_cost(var.Type, initType);
        if (cost.CastNeeded) {
            if (cost.ImplicitCastPossible) {
                insert_implicit_cast(var.Type, initType, var.Initializer, cost.Kind);
            } else {
                m_Context->report_compiler_diagnostic(var.Initializer->Loc, var.Initializer->Range, fmt::format("Cannot implicitly convert from '{}' to '{}'", type_info_to_string(initType), type_info_to_string(var.Type)));
            }
        }
    }

    void SemanticAnalyzer::resolve_param_initializer(TypeInfo* paramType, Expr* arg) {
        m_TemporaryContext = true;
        resolve_expr(arg);

        TypeInfo* argType = arg->Type;
        require_rvalue(arg);
        m_TemporaryContext = false;

        ConversionCost cost = get_conversion_cost(paramType, argType);
        if (cost.CastNeeded) {
            if (cost.ImplicitCastPossible) {
                insert_implicit_cast(paramType, argType, arg, cost.Kind);
            } else {
                m_Context->report_compiler_diagnostic(arg->Loc, arg->Range, fmt::format("Cannot implicitly convert from '{}' to '{}'", type_info_to_string(argType), type_info_to_string(paramType)));
            }
        }
    }

    void SemanticAnalyzer::create_default_initializer(Expr** expr, TypeInfo* type, SourceLocation loc, SourceRange range) {
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
                        m_Context->report_compiler_diagnostic(loc, range, fmt::format("Cannot default initialize variable of type '{}'", type_info_to_string(type)));
                        break;
                    }
                }

                break;
            }

            case TypeKind::Unresolved: break;

            default: m_Context->report_compiler_diagnostic(loc, range, fmt::format("Cannot default initialize variable of type '{}'", type_info_to_string(type)), CompilerDiagKind::Warning); break;
        }
    }

} // namespace Aria::Internal