#include "ariac/semantic_analyzer/semantic_analyzer.hpp"

namespace ariac {

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

        std::string ident = fmt::format("{}", fnDecl.identifier);
        
        if (fnDecl.linkage_kind == LinkageKind::Extern && fnDecl.body) {
            m_context->report_compiler_diagnostic(decl->loc, decl->range, "Function marked 'extern' must not have body");
        }

        if (fnDecl.type->function.var_arg && fnDecl.linkage_kind != LinkageKind::Extern) {
            m_context->report_compiler_diagnostic(decl->loc, decl->range, "Function with variable amount of parameters (vararg) must be marked 'extern'");
        }

        if (fnDecl.body) {
            m_active_return_type = fnDecl.type->function.return_type;
            push_scope();
            
            for (Decl* p : fnDecl.parameters) {
                resolve_param_decl(p);
            }
            
            if (fnDecl.body) {
                resolve_block_stmt(fnDecl.body);
            }

            if (m_scopes.back().reaches_end) {
                if (!fnDecl.type->function.return_type->is_void()) {
                    m_context->report_compiler_diagnostic(decl->loc, decl->range, "Control flow reaches end of function with a non void return type");
                } else {
                    fnDecl.body->block.stmts.append(m_context, Stmt::Create(m_context, decl->loc, decl->range, StmtKind::Return, ReturnStmt(nullptr)));
                }
            }

            pop_scope();
            m_active_return_type = nullptr;
        } else if (fnDecl.linkage_kind != LinkageKind::Extern) {
            m_context->report_compiler_diagnostic_with_notes(decl->loc, decl->range, "Body for this function must be specified",
                { "If this function is defined elsewhere, use 'extern'"} );
        }

        decl->resolve_status = ResolveStatus::Done;
    }

    void SemanticAnalyzer::resolve_struct_decl(Decl* decl) {
        if (decl->resolve_status == ResolveStatus::Done) { return; }
        decl->resolve_status = ResolveStatus::InProgress;

        StructDecl& s = decl->struct_;

        for (Decl* field : s.fields) {
            field->parent_unit = decl->parent_unit;
            field->parent_module = decl->parent_module;

            ARIA_ASSERT(field->kind == DeclKind::Field, "Invalid field");

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

            s.field_lookup.insert(m_context, field->field.identifier, field);
        }
        decl->resolve_status = ResolveStatus::Done;

        for (Decl* impl : s.impls) {
            resolve_impl_decl(impl);
        }
    }

    void SemanticAnalyzer::resolve_impl_decl(Decl* decl) {
        if (decl->resolve_status == ResolveStatus::Done) { return; }
        decl->resolve_status = ResolveStatus::InProgress;

        ImplDecl& i = decl->impl;

        for (Decl* field : i.fields) {
            switch (field->kind) {
                case DeclKind::Method: {
                    if (i.parent->struct_.field_lookup.contains(field->method.identifier)) {
                        Decl* prev = i.field_lookup.at(field->method.identifier);

                        if (prev->kind == DeclKind::Method) {
                            Decl* overloaded = Decl::Create(m_context, field->loc, field->range, DeclKind::OverloadedMethod, DeclVisibility::Public, OverloadedMethodDecl(field->method.identifier));
                            i.field_lookup.insert(m_context, field->method.identifier, overloaded);
                            i.parent->struct_.field_lookup.insert(m_context, field->method.identifier, overloaded);

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

                    i.field_lookup.insert(m_context, field->method.identifier, field);
                    i.parent->struct_.field_lookup.insert(m_context, field->method.identifier, field);
                    break;
                }

                case DeclKind::Constructor: 
                    i.parent->struct_.definition.ctors.append(m_context, field);
                    break;

                case DeclKind::Destructor:
                    i.parent->struct_.definition.dtor = field;
                    break;
            }
        }

        StructDeclaration sd;
        sd.identifier = i.identifier;
        sd.source_decl = i.parent;
        m_active_struct = TypeInfo::Create(m_context, TypeKind::Structure);
        m_active_struct->struct_ = sd;

        for (Decl* field : i.fields) {
            switch (field->kind) {
                case DeclKind::Method: {
                    resolve_type(field->loc, field->range, field->method.type->function.return_type);
                    m_active_return_type = field->method.type->function.return_type;
                    push_scope();
                    for (Decl* p : field->method.parameters) {
                        resolve_param_decl(p);
                    }

                    resolve_block_stmt(field->method.body);

                    if (m_scopes.back().reaches_end) {
                        if (!field->method.type->function.return_type->is_void()) {
                            m_context->report_compiler_diagnostic(decl->loc, decl->range, "Control flow reaches end of function with a non void return type");
                        } else {
                            field->method.body->block.stmts.append(m_context, Stmt::Create(m_context, decl->loc, decl->range, StmtKind::Return, ReturnStmt(nullptr)));
                        }
                    }

                    pop_scope();
                    m_active_return_type = nullptr;
                    break;
                }

                case DeclKind::Constructor: {
                    if (field->constructor.parameters.size == 0) { i.parent->struct_.definition.default_ctor = field; }

                    for (Decl* field : i.fields) {
                        if (field->kind == DeclKind::Field) {
                            if (!type_is_trivial(field->field.type)) {
                                ARIA_TODO("Propagating constructors");
                            }
                        }
                    }

                    if (field->constructor.kind != ConstructorKind::Deleted) {
                        push_scope();
                        for (Decl* param : field->constructor.parameters) {
                            resolve_param_decl(param);
                        }

                        resolve_stmt(field->constructor.body);
                        pop_scope();
                    }
                    break;
                }

                case DeclKind::Destructor: {
                    for (Decl* field : i.fields) {
                        if (field->kind == DeclKind::Field) {
                            if (!type_is_trivial(field->field.type)) {
                                ARIA_TODO("Propagating destructors");
                            }
                        }
                    }

                    push_scope();
                    resolve_block_stmt(field->destructor.body);

                    if (m_scopes.back().reaches_end) {
                        field->destructor.body->block.stmts.append(m_context, Stmt::Create(m_context, decl->loc, decl->range, StmtKind::Return, ReturnStmt(nullptr)));
                    }
                    pop_scope();

                    break;
                }

                default: ARIA_UNREACHABLE();
            }
        }

        m_active_struct = nullptr;
        decl->resolve_status = ResolveStatus::Done;
    }

    void SemanticAnalyzer::resolve_typedef_decl(Decl* decl) {
        TypedefDecl& td = decl->typedef_;
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
            case DeclKind::Impl: return;
            case DeclKind::Typedef: return resolve_typedef_decl(decl);

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

} // namespace ariac