#include "ariac/ast/decl.hpp"
#include "ariac/ast/expr.hpp"

namespace ariac {

    Decl* Decl::dup(Decl* d) {
        Decl* copy = Decl::Create(d->loc, d->kind, d->visibility, ErrorDecl());
        copy->parent_unit = d->parent_unit;
        copy->parent_module = d->parent_module;

        switch (d->kind) {
            case DeclKind::Error: break;

            case DeclKind::Var: {
                VarDecl& v = d->var;
                copy->var.identifier = v.identifier;
                copy->var.type = TypeInfo::dup(v.type);
                if (v.initializer) { copy->var.initializer = Expr::dup(v.initializer); }
                copy->var.global_var = v.global_var;
                copy->var.const_var = v.const_var;
                copy->var.linkage_kind = v.linkage_kind;
                copy->var.dtor = v.dtor;
                break;
            }

            case DeclKind::Param: {
                ParamDecl& p = d->param;
                copy->param.identifier = p.identifier;
                copy->param.type = TypeInfo::dup(p.type);

                copy->param.resolved_default_arg = p.resolved_default_arg;
                if (p.default_arg) { copy->param.default_arg = Expr::dup(p.default_arg); }
                break;
            }

            case DeclKind::Function: {
                FunctionDecl& f = d->function;
                copy->function.identifier = f.identifier;
                copy->function.type = TypeInfo::dup(f.type);
                if (f.body) { copy->function.body = Stmt::dup(f.body); }
                copy->function.linkage_kind = f.linkage_kind;
                copy->function.is_deleted = f.is_deleted;
                copy->function.is_specilization = f.is_specilization;

                if (f.is_specilization) {
                    copy->function.specilization_info.is_explicit = f.specilization_info.is_explicit;
                    copy->function.specilization_info.instantiation_loc = f.specilization_info.instantiation_loc;

                    for (TypeInfo* t : f.specilization_info.types) {
                        copy->function.specilization_info.types.append(TypeInfo::dup(t));
                    }
                }

                break;
            }

            case DeclKind::Struct: {
                StructDecl& s = d->struct_;
                copy->struct_.identifier = s.identifier;
                
                for (Decl* f : s.fields) {
                    Decl* dupped = Decl::dup(f);
                    copy->struct_.fields.append(dupped);
                }

                break;
            }

            case DeclKind::Field: {
                FieldDecl& f = d->field;
                copy->field.identifier = f.identifier;
                copy->field.type = TypeInfo::dup(f.type);
                break;
            }

            case DeclKind::Method: {
                MethodDecl& m = d->method;
                copy->method.parent = m.parent;
                copy->method.identifier = m.identifier;
                copy->method.type = TypeInfo::dup(m.type);
                copy->method.body = Stmt::dup(m.body);
                break;
            }

            case DeclKind::Destructor: {
                DestructorDecl& dt = d->destructor;
                copy->destructor.body = Stmt::dup(dt.body);
                copy->destructor.type = dt.type; // Note that it's safe to do a shallow copy because dtor types never change
                copy->destructor.parent = dt.parent;
                break;
            }

            default: ARIA_UNREACHABLE("Invalid decl kind");
        }

        return copy;
    }

} // namespace ariac