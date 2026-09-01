#include "ariac/semantic_analyzer/semantic_analyzer.hpp"

namespace ariac {

    void SemanticAnalyzer::resolve_var_initializer(Decl* decl) {
        ARIA_ASSERT(decl->kind == DeclKind::Var, "SemanticAnalyzer::resolve_var_initializer() only supports a variable declaration");
        VarDecl& var = decl->var;

        if (var.initializer) {
            resolve_expr(var.initializer);

            // Handle type inferrence here
            if (!var.type) { var.type = var.initializer->type; }
            resolve_type(var.type);
            if (var.initializer->type->is_error() || var.type->is_error()) { return; }

            try_insert_implicit_cast(var.type, var.initializer);

            if (var.ref_var) {
                if (!var.initializer->is_lvalue()) {
                    report_error(var.initializer->loc, "Initializer for reference variable must be an lvalue");
                }
            } else {
                require_rvalue(var.initializer);
            }

            if (var.const_var) {
                if (!is_const_expr(var.initializer)) {
                    report_diag(var.initializer->loc, "Initializier of const variable must be a constant expression");
                } else if (type_is_equal(var.type, var.initializer->type)) {
                    var.initializer = eval_const_expr(var.initializer);
                }
            }
        }
    }

    void SemanticAnalyzer::resolve_param_initializer(Decl* decl, Expr* arg) {
        bool prev_val = m_sema_context.temporary;
        m_sema_context.temporary = true;
        resolve_expr(arg);
        try_insert_implicit_cast(decl->param.type, arg);
        require_rvalue(arg);
        m_sema_context.temporary = prev_val;
    }

    void SemanticAnalyzer::resolve_param_default_arg(Decl* decl) {
        ParamDecl& p = decl->param;

        if (p.default_arg && !p.resolved_default_arg) {
            resolve_expr(p.default_arg);
            try_insert_implicit_cast(p.type, p.default_arg, "parameter");
            require_rvalue(p.default_arg);
            p.resolved_default_arg = true;
        }
    }

} // namespace ariac