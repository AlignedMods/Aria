#include "ariac/semantic_analyzer/semantic_analyzer.hpp"

namespace ariac {

    void SemanticAnalyzer::resolve_var_initializer(Decl* decl) {
        ARIA_ASSERT(decl->kind == DeclKind::Var, "SemanticAnalyzer::resolve_var_initializer() only supports a variable declaration");
        VarDecl& var = decl->var;

        if (var.initializer) {
            resolve_expr(var.initializer);

            // Handle type inferrence here
            if (!var.type) {
                if (var.initializer->type->is_void()) {
                    m_context->report_compiler_diagnostic(decl->loc, "Cannot create variable of void type");
                }
                var.type = var.initializer->type;
            }

            if (var.initializer->type->is_error() || var.type->is_error()) { return; }

            ConversionCost cost = get_conversion_cost(var.type, var.initializer->type);
            if (cost.cast_needed) {
                if (cost.implicit_cast_possible) {
                    insert_implicit_cast(var.type, var.initializer->type, var.initializer, cost.kind);
                } else {
                    m_context->report_compiler_diagnostic(var.initializer->loc, fmt::format("Cannot implicitly convert from '{}' to '{}'", type_info_to_string(var.initializer->type), type_info_to_string(var.type)));
                }
            }

            if (!var.type->is_reference()) {
                require_rvalue(var.initializer);
            }

            if (var.const_var) {
                if (!is_const_expr(var.initializer)) {
                    m_context->report_compiler_diagnostic(var.initializer->loc, "Initializier of const variable must be a constant expression");
                } else if (type_is_equal(var.type, var.initializer->type)) {
                    var.initializer = eval_const_expr(var.initializer);
                }
            }
        }
    }

    void SemanticAnalyzer::resolve_param_initializer(TypeInfo* param_type, Expr* arg) {
        m_temporary_context = true;
        resolve_expr(arg);

        TypeInfo* argType = arg->type;

        ConversionCost cost = get_conversion_cost(param_type, argType);
        if (cost.cast_needed) {
            if (cost.implicit_cast_possible) {
                insert_implicit_cast(param_type, argType, arg, cost.kind);
            } else {
                m_context->report_compiler_diagnostic(arg->loc, fmt::format("Cannot implicitly convert from '{}' to '{}'", type_info_to_string(argType), type_info_to_string(param_type)));
            }
        }

        if (!param_type->is_reference()) { require_rvalue(arg); }
        m_temporary_context = false;
    }

} // namespace ariac