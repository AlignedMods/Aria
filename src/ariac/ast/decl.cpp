#include "ariac/ast/decl.hpp"
#include "ariac/ast/expr.hpp"

namespace ariac {

    Decl* Decl::dup(CompilationContext* ctx, Decl* d) {
        Decl* copy = Decl::Create(ctx, d->loc, d->kind, d->visibility, ErrorDecl());
        copy->parent_unit = d->parent_unit;
        copy->parent_module = d->parent_module;

        switch (d->kind) {
            case DeclKind::Error: break;

            case DeclKind::Var: {
                VarDecl& v = d->var;
                copy->var.identifier = v.identifier;
                copy->var.type = TypeInfo::dup(ctx, v.type);
                copy->var.initializer = Expr::dup(ctx, v.initializer);
                copy->var.global_var = v.global_var;
                copy->var.const_var = v.const_var;
                copy->var.linkage_kind = v.linkage_kind;
                break;
            }

            case DeclKind::Param: {
                ParamDecl& p = d->param;
                copy->param.identifier = p.identifier;
                copy->param.type = TypeInfo::dup(ctx, p.type);
                break;
            }

            case DeclKind::Function: {
                FunctionDecl& f = d->function;
                copy->function.identifier = f.identifier;
                copy->function.type = TypeInfo::dup(ctx, f.type);

                for (Decl* p : f.parameters) {
                    copy->function.parameters.append(ctx, Decl::dup(ctx, p));
                }

                copy->function.body = Stmt::dup(ctx, f.body);
                copy->function.linkage_kind = f.linkage_kind;
                break;
            }

            case DeclKind::Struct: {
                StructDecl& s = d->struct_;
                copy->struct_.identifier = s.identifier;
                
                for (Decl* f : s.fields) {
                    Decl* dupped = Decl::dup(ctx, f);
                    copy->struct_.fields.append(ctx, dupped);
                }

                for (Decl* i : s.impls) {
                    Decl* dupped = Decl::dup(ctx, i);
                    dupped->impl.parent = copy;


                    copy->struct_.impls.append(ctx, dupped);
                }

                break;
            }

            case DeclKind::Impl: {
                ImplDecl& i = d->impl;
                copy->impl.identifier = i.identifier;

                for (Decl* f : i.fields) {
                    Decl* dupped = Decl::dup(ctx, f);
                    copy->impl.fields.append(ctx, dupped);
                    dupped->method.parent = copy;
                }

                break;
            }

            case DeclKind::Field: {
                FieldDecl& f = d->field;
                copy->field.identifier = f.identifier;
                copy->field.type = TypeInfo::dup(ctx, f.type);
                break;
            }

            case DeclKind::Method: {
                MethodDecl& m = d->method;
                copy->method.identifier = m.identifier;
                copy->method.type = TypeInfo::dup(ctx, m.type);

                for (Decl* p : m.parameters) {
                    copy->method.parameters.append(ctx, Decl::dup(ctx, p));
                }

                copy->method.body = Stmt::dup(ctx, m.body);
                copy->method.parent = m.parent;
                break;
            }

            default: ARIA_UNREACHABLE();
        }

        return copy;
    }

} // namespace ariac