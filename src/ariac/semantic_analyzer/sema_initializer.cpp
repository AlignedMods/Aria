#include "ariac/semantic_analyzer/semantic_analyzer.hpp"

namespace Aria::Internal {

    void SemanticAnalyzer::resolve_var_initializer(Decl* decl) {
        ARIA_ASSERT(decl->kind == DeclKind::Var, "SemanticAnalyzer::resolve_var_initializer() only supports a variable declaration");
        VarDecl& var = decl->var;

        if (var.initializer) {
            resolve_expr(var.initializer);

            if (var.type) {
                if (var.type->is_structure()) {
                    Decl* ctor = nullptr;

                    for (Decl* d : var.type->struct_.source_decl->struct_.definition.ctors) {
                        if (d->constructor.parameters.size == 1) {
                            if (type_is_equal(var.initializer->type, d->constructor.parameters.items[0]->param.type)) {
                                ctor = d;
                                break;
                            }
                        }
                    }

                    if (!ctor) {
                        m_context->report_compiler_diagnostic(var.initializer->loc, var.initializer->range, fmt::format("Cannot implicitly convert from '{}' to '{}'", type_info_to_string(var.initializer->type), type_info_to_string(var.type)));
                        return;
                    }
                }
            }

            // Handle type inferrence here
            if (!var.type) {
                if (var.initializer->type->is_void()) {
                    m_context->report_compiler_diagnostic(decl->loc, decl->range, "Cannot create variable of void type");
                }
                var.type = var.initializer->type;
            }

            if (!var.type->is_reference()) {
                require_rvalue(var.initializer);
            }

            if (var.initializer->type->is_error() || var.type->is_error()) { return; }

            ConversionCost cost = get_conversion_cost(var.type, var.initializer->type);
            if (cost.cast_needed) {
                if (cost.implicit_cast_possible) {
                    insert_implicit_cast(var.type, var.initializer->type, var.initializer, cost.kind);
                } else {
                    m_context->report_compiler_diagnostic(var.initializer->loc, var.initializer->range, fmt::format("Cannot implicitly convert from '{}' to '{}'", type_info_to_string(var.initializer->type), type_info_to_string(var.type)));
                }
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

} // namespace Aria::Internal