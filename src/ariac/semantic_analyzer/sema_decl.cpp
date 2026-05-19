#include "ariac/semantic_analyzer/semantic_analyzer.hpp"

namespace Aria::Internal {

    void SemanticAnalyzer::resolve_translation_unit_decl(Decl* decl) {
        TranslationUnitDecl tu = decl->translation_unit;

        for (Stmt* stmt : tu.stmts) {
            resolve_stmt(stmt);
        }
    }

    void SemanticAnalyzer::resolve_var_decl(Decl* decl) {
        VarDecl& varDecl = decl->var;
        std::string_view ident = varDecl.identifier;

        if (varDecl.type) {
            resolve_type(decl->loc, decl->range, varDecl.type);

            if (varDecl.type->is_void()) {
                m_context->report_compiler_diagnostic(decl->loc, decl->range, "Cannot declare variable of 'void' type");
            }
        }

        resolve_var_initializer(decl);

        if (m_scopes.size() > 0) {
            if (m_scopes.back().declarations.contains(ident)) {
                m_context->report_compiler_diagnostic(decl->loc, decl->range, fmt::format("Redeclaring symbol '{}'", ident));
            }

            m_scopes.back().declarations[ident] = { varDecl.type, decl, DeclKind::Var };
        }
    }

    void SemanticAnalyzer::resolve_param_decl(Decl* decl) {
        ParamDecl& paramDecl = decl->param;
        resolve_type(decl->loc, decl->range, paramDecl.type);
        m_scopes.back().declarations[paramDecl.identifier] = { paramDecl.type, decl, DeclKind::Param };
    }

    void SemanticAnalyzer::resolve_function_decl(Decl* decl) {
        if (decl->resolve_status == ResolveStatus::Done || decl->resolve_status == ResolveStatus::InProgress) { return; }
        decl->resolve_status = ResolveStatus::InProgress;
        FunctionDecl fnDecl = decl->function;

        for (auto& attr : fnDecl.attributes) {
            if (attr.kind == FunctionDecl::AttributeKind::Unsafe) {
                m_unsafe_context = true;
            }
        }

        std::string ident = fmt::format("{}", fnDecl.identifier);
        
        if (fnDecl.body) {
            m_active_return_type = fnDecl.type->function.return_type;
            push_scope();
            
            for (Decl* p : fnDecl.parameters) {
                resolve_param_decl(p);
            }
            
            if (fnDecl.body) {
                resolve_block_stmt(fnDecl.body);
            }

            if (m_scopes.back().reaches_end && !fnDecl.type->function.return_type->is_void()) {
                m_context->report_compiler_diagnostic(decl->loc, decl->range, "Control flow reaches end of function with a non void return type");
            }

            pop_scope();
            m_active_return_type = nullptr;
        }

        m_unsafe_context = false;
        decl->resolve_status = ResolveStatus::Done;
    }

    void SemanticAnalyzer::resolve_struct_decl(Decl* decl) {
        if (decl->resolve_status == ResolveStatus::Done) { return; }
        decl->resolve_status = ResolveStatus::InProgress;

        StructDecl& s = decl->struct_;
        std::string ident = fmt::format("{}", s.identifier);

        StructDeclaration d;
        d.identifier = s.identifier;
        d.source_decl = decl;
        
        TypeInfo* structType = TypeInfo::Create(m_context, TypeKind::Structure);
        structType->struct_ = d;

        std::vector<Decl*> methods;

        for (Decl* field : s.fields) {
            field->parent_unit = decl->parent_unit;
            field->parent_module = decl->parent_module;

            switch (field->kind) {
                case DeclKind::Field: {
                    if (s.field_lookup.contains(field->field.identifier)) {
                        Decl* prev = s.field_lookup.at(field->field.identifier);
                        m_context->report_compiler_diagnostic(field->loc, field->range, fmt::format("Redeclaring field '{}'", field->field.identifier));
                        m_context->report_compiler_diagnostic(prev->loc, prev->range, "Previous declaration here", CompilerDiagKind::Note);
                    }

                    if (field->field.type->is_void()) {
                        m_context->report_compiler_diagnostic(field->loc, field->range, "Cannot declare field of 'void' type");
                        field->kind = DeclKind::Error;
                        continue;
                    }

                    if (!type_is_trivial(field->field.type)) { ARIA_TODO("Proper destructors"); }
                    s.field_lookup.insert(m_context, field->field.identifier, field);
                    break;
                }

                case DeclKind::Constructor:
                    methods.push_back(field);
                    s.field_lookup.insert(m_context, "<ctor>", field);
                    break;

                case DeclKind::Destructor:
                    methods.push_back(field);
                    s.field_lookup.insert(m_context, "<dtor>", field);
                    break;

                case DeclKind::Method: {
                    methods.push_back(field);

                    if (s.field_lookup.contains(field->method.identifier)) {
                        Decl* prev = s.field_lookup.at(field->method.identifier);

                        if (prev->kind == DeclKind::Method) {
                            Decl* overloaded = Decl::Create(m_context, field->loc, field->range, DeclKind::OverloadedMethod, DeclVisibility::Public, OverloadedMethodDecl(field->method.identifier));
                            s.field_lookup.insert(m_context, field->method.identifier, overloaded);

                            std::string old_mangle = mangle_method(&prev->method);
                            std::string new_mangle = mangle_method(&field->method);

                            if (old_mangle == new_mangle) {
                                m_context->report_compiler_diagnostic(field->loc, field->range, fmt::format("Redefining method '{}'", field->method.identifier));
                                m_context->report_compiler_diagnostic(prev->loc, prev->range, "Previous declaration here", CompilerDiagKind::Note);
                                field->kind = DeclKind::Error;
                                continue;
                            }

                            overloaded->overloaded_method.funcs.append(m_context, prev);
                            overloaded->overloaded_method.funcs.append(m_context, field);
                            continue;
                        } else if (prev->kind == DeclKind::OverloadedMethod) {
                            for (Decl* overload : prev->overloaded_function.funcs) {
                                std::string oldMangle = mangle_method(&overload->method);
                                std::string newMangle = mangle_method(&field->method);

                                if (oldMangle == newMangle) {
                                    m_context->report_compiler_diagnostic(field->loc, field->range, fmt::format("Redefining overloaded method '{}'", field->method.identifier));
                                    m_context->report_compiler_diagnostic(prev->loc, prev->range, "Previous declaration here", CompilerDiagKind::Note);
                                    field->kind = DeclKind::Error;
                                    continue;
                                }
                            }

                            prev->overloaded_method.funcs.append(m_context, field);
                            continue;
                        } else {
                            m_context->report_compiler_diagnostic(field->loc, field->range, fmt::format("Redeclaring field '{}' as method", field->method.identifier));
                            m_context->report_compiler_diagnostic(prev->loc, prev->range, "Previous declaration here", CompilerDiagKind::Note);
                            continue;
                        }
                    }

                    s.field_lookup.insert(m_context, field->method.identifier, field);
                    break;
                }

                default: ARIA_UNREACHABLE();
            }
        }

        decl->resolve_status = ResolveStatus::Done;

        m_active_struct = TypeInfo::Dup(m_context, structType);

        for (Decl* method : methods) {
            if (method->kind == DeclKind::Constructor) {
                TinyVector<Stmt*> newBody;

                for (Decl* field : s.fields) {
                    if (field->kind == DeclKind::Field) {
                        if (!type_is_trivial(field->field.type)) {
                            ARIA_TODO("Propagating constructors");
                        }
                    }
                }

                if (!method->constructor.disabled) {
                    push_scope();
                    for (Decl* param : method->constructor.parameters) {
                        resolve_param_decl(param);
                    }

                    resolve_stmt(method->constructor.body);

                    if (method->constructor.parameters.size == 0) { s.definition.default_ctor = &method->constructor; }
                    pop_scope();
                }
            } else if (method->kind == DeclKind::Destructor) {
                TinyVector<Stmt*> newBody;

                for (Decl* field : s.fields) {
                    if (field->kind == DeclKind::Field) {
                        if (!type_is_trivial(field->field.type)) {
                            ARIA_TODO("Propagating destructors");
                        }
                    }
                }
                resolve_stmt(method->destructor.body);
            } else if (method->kind == DeclKind::Method) {
                m_active_return_type = method->method.type->function.return_type;
                push_scope();
                for (Decl* p : method->method.parameters) {
                    resolve_param_decl(p);
                }

                resolve_block_stmt(method->method.body);
                pop_scope();
                m_active_return_type = nullptr;
            }
        }
        
        m_active_struct = nullptr;
    }

    void SemanticAnalyzer::resolve_decl(Decl* decl) {
        switch (decl->kind) {
            case DeclKind::Error:
            case DeclKind::Module:
            case DeclKind::OverloadedFunction:
            case DeclKind::Field:
            case DeclKind::Constructor:
            case DeclKind::Destructor:
            case DeclKind::Method:
            case DeclKind::OverloadedMethod:
            case DeclKind::BuiltinCopyConstructor:
            case DeclKind::BuiltinDestructor: return;

            case DeclKind::TranslationUnit: return resolve_translation_unit_decl(decl);
            case DeclKind::Var: return resolve_var_decl(decl);
            case DeclKind::Param: return resolve_param_decl(decl);
            case DeclKind::Function: return resolve_function_decl(decl);
            case DeclKind::Struct: return resolve_struct_decl(decl);

            default: ARIA_UNREACHABLE();
        }
    }

    std::string SemanticAnalyzer::mangle_function(FunctionDecl* fn) {
        std::string ident = fmt::format("{}(", fn->identifier);

        for (size_t i = 0; i < fn->parameters.size; i++) {
            ident += type_info_to_string(fn->parameters.items[i]->param.type);

            if (i != fn->parameters.size - 1) {
                ident += ", ";
            }
        }

        ident += ")";

        return ident;
    }

    std::string SemanticAnalyzer::mangle_method(MethodDecl* m) {
        std::string ident = fmt::format("{}(", m->identifier);

        for (size_t i = 0; i < m->parameters.size; i++) {
            ident += type_info_to_string(m->parameters.items[i]->param.type);

            if (i != m->parameters.size - 1) {
                ident += ", ";
            }
        }

        ident += ")";

        return ident;
    }

} // namespace Aria::Internal