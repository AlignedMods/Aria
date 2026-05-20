#include "ariac/semantic_analyzer/semantic_analyzer.hpp"

namespace Aria::Internal {

    void SemanticAnalyzer::resolve_var_initializer(Decl* decl) {
        ARIA_ASSERT(decl->kind == DeclKind::Var, "SemanticAnalyzer::resolve_var_initializer() only supports a variable declaration");
        VarDecl& var = decl->var;

        if (!var.initializer) {
            return create_default_initializer(&var.initializer, var.type, decl->loc, decl->range);
        } else {
            resolve_expr(var.initializer);
        }

        TypeInfo* init_type = var.initializer->type;
        if (init_type->is_structure()) {
            if (var.initializer->value_kind == ExprValueKind::LValue) { // Call copy constructor
                // if (init_type->struct_.source_decl->struct_.definition.copy_ctor) {
                //     TinyVector<Expr*> args;
                //     resolve_param_initializer(init_type->struct_.source_decl->struct_.definition.copy_ctor->parameters.items[0]->param.type, var.initializer);
                //     args.append(m_context, Expr::Dup(m_context, var.initializer));
                // 
                //     replace_expr(var.initializer, Expr::Create(m_context, var.initializer->loc, var.initializer->range, ExprKind::Construct,
                //         ExprValueKind::RValue, init_type,
                //         ConstructExpr(init_type->struct_.source_decl->struct_.definition.copy_ctor, args)));
                // }
            }
        }

        require_rvalue(var.initializer);

        // Handle type inferrence here
        if (!var.type) {
            if (init_type->is_void()) {
                m_context->report_compiler_diagnostic(decl->loc, decl->range, "Cannot create variable of void type");
            }
            var.type = init_type;
        }

        if (init_type->is_error() || var.type->is_error()) { return; }

        ConversionCost cost = get_conversion_cost(var.type, init_type);
        if (cost.cast_needed) {
            if (cost.implicit_cast_possible) {
                insert_implicit_cast(var.type, init_type, var.initializer, cost.kind);
            } else {
                m_context->report_compiler_diagnostic(var.initializer->loc, var.initializer->range, fmt::format("Cannot implicitly convert from '{}' to '{}'", type_info_to_string(init_type), type_info_to_string(var.type)));
            }
        }
    }

    void SemanticAnalyzer::resolve_param_initializer(TypeInfo* param_type, Expr* arg) {
        m_temporary_context = true;
        resolve_expr(arg);

        TypeInfo* argType = arg->type;
        if (!param_type->is_reference()) { require_rvalue(arg); }
        m_temporary_context = false;

        ConversionCost cost = get_conversion_cost(param_type, argType);
        if (cost.cast_needed) {
            if (cost.implicit_cast_possible) {
                insert_implicit_cast(param_type, argType, arg, cost.kind);
            } else {
                m_context->report_compiler_diagnostic(arg->loc, arg->range, fmt::format("Cannot implicitly convert from '{}' to '{}'", type_info_to_string(argType), type_info_to_string(param_type)));
            }
        }
    }

    void SemanticAnalyzer::create_default_initializer(Expr** expr, TypeInfo* type, SourceLocation loc, SourceRange range) {
        switch (type->kind) {
            case TypeKind::Bool:   *expr = Expr::Create(m_context, {}, {}, ExprKind::BooleanLiteral,   ExprValueKind::RValue, type, BooleanLiteralExpr(false)); break;
            case TypeKind::Char:   *expr = Expr::Create(m_context, {}, {}, ExprKind::CharacterLiteral, ExprValueKind::RValue, type, CharacterLiteralExpr('\0')); break;
            case TypeKind::UChar:  *expr = Expr::Create(m_context, {}, {}, ExprKind::CharacterLiteral, ExprValueKind::RValue, type, CharacterLiteralExpr('\0')); break;
            case TypeKind::Short:  *expr = Expr::Create(m_context, {}, {}, ExprKind::IntegerLiteral,   ExprValueKind::RValue, type, IntegerLiteralExpr(0)); break;
            case TypeKind::UShort: *expr = Expr::Create(m_context, {}, {}, ExprKind::IntegerLiteral,   ExprValueKind::RValue, type, IntegerLiteralExpr(0)); break;
            case TypeKind::Int:    *expr = Expr::Create(m_context, {}, {}, ExprKind::IntegerLiteral,   ExprValueKind::RValue, type, IntegerLiteralExpr(0)); break;
            case TypeKind::UInt:   *expr = Expr::Create(m_context, {}, {}, ExprKind::IntegerLiteral,   ExprValueKind::RValue, type, IntegerLiteralExpr(0)); break;
            case TypeKind::Long:   *expr = Expr::Create(m_context, {}, {}, ExprKind::IntegerLiteral,   ExprValueKind::RValue, type, IntegerLiteralExpr(0)); break;
            case TypeKind::ULong:  *expr = Expr::Create(m_context, {}, {}, ExprKind::IntegerLiteral,   ExprValueKind::RValue, type, IntegerLiteralExpr(0)); break;
                                                                                                                               
            case TypeKind::Float:  *expr = Expr::Create(m_context, {}, {}, ExprKind::FloatingLiteral,  ExprValueKind::RValue, type, FloatingLiteralExpr(0.0)); break;
            case TypeKind::Double: *expr = Expr::Create(m_context, {}, {}, ExprKind::FloatingLiteral,  ExprValueKind::RValue, type, FloatingLiteralExpr(0.0)); break;
                                                                                                                               
            case TypeKind::Ptr:    *expr = Expr::Create(m_context, {}, {}, ExprKind::Null,             ExprValueKind::RValue, type, ErrorExpr()); break;

            case TypeKind::Array:  {
                Expr* base_initializer = nullptr;
                create_default_initializer(&base_initializer, type->array.type, loc, range);
                *expr = Expr::Create(m_context, {}, {}, ExprKind::ArrayFiller, ExprValueKind::RValue, type, ArrayFillerExpr(base_initializer));
                break;
            }
                                                                                                           
            case TypeKind::String: *expr = Expr::Create(m_context, {}, {}, ExprKind::StringLiteral,    ExprValueKind::RValue, type, StringLiteralExpr("")); break;

            case TypeKind::Structure: {
                const StructDeclaration& sDecl = type->struct_;

                // if (sDecl.source_decl) {
                //     ARIA_ASSERT(sDecl.source_decl->kind == DeclKind::Struct, "Invalid source decl");
                //     if (sDecl.source_decl->struct_.definition.default_ctor) {
                //         ConstructorDecl* ctor = sDecl.source_decl->struct_.definition.default_ctor;
                // 
                //         *expr = Expr::Create(m_context, {}, {}, ExprKind::Construct, ExprValueKind::RValue, type, ConstructExpr(ctor, {}));
                //         break;
                //     } else {
                //         m_context->report_compiler_diagnostic(loc, range, fmt::format("Cannot default initialize variable of type '{}'", type_info_to_string(type)));
                //         break;
                //     }
                // }

                break;
            }

            case TypeKind::Unresolved: break;

            default: m_context->report_compiler_diagnostic(loc, range, fmt::format("Cannot default initialize variable of type '{}'", type_info_to_string(type)), CompilerDiagKind::Warning); break;
        }
    }

} // namespace Aria::Internal