#include "ariac/semantic_analyzer/semantic_analyzer.hpp"

namespace ariac {

    void SemanticAnalyzer::resolve_translation_unit_decl(Decl* decl) {
        TranslationUnitDecl tu = decl->translation_unit;

        for (Stmt* stmt : tu.stmts) {
            resolve_stmt(stmt);
        }
    }

    void SemanticAnalyzer::resolve_var_decl(Decl* decl) {
        if (decl->resolve_status == ResolveStatus::Done || decl->resolve_status == ResolveStatus::InProgress) { return; }
        decl->resolve_status = ResolveStatus::InProgress;

        VarDecl& varDecl = decl->var;
        std::string_view ident = varDecl.identifier;

        if (varDecl.type) {
            resolve_type(varDecl.type);

            if (varDecl.type->is_void()) {
                m_context->report_compiler_diagnostic(decl->loc, "Cannot declare variable of 'void' type");
            }
        }

        resolve_var_initializer(decl);

        if (m_scopes.size() > 0) {
            if (m_scopes.back().declarations.contains(ident)) {
                m_context->report_compiler_diagnostic(decl->loc, fmt::format("Redeclaring symbol '{}'", ident));
            }

            m_scopes.back().declarations[ident] = { varDecl.type, decl, DeclKind::Var };
        }

        decl->resolve_status = ResolveStatus::Done;
    }

    void SemanticAnalyzer::resolve_param_decl(Decl* decl) {
        ParamDecl& paramDecl = decl->param;
        resolve_type(paramDecl.type);
        m_scopes.back().declarations[paramDecl.identifier] = { paramDecl.type, decl, DeclKind::Param };
    }

    void SemanticAnalyzer::resolve_function_decl(Decl* decl) {
        if (decl->resolve_status == ResolveStatus::Done || decl->resolve_status == ResolveStatus::InProgress) { return; }
        decl->resolve_status = ResolveStatus::InProgress;
        FunctionDecl fnDecl = decl->function;

        std::string ident = fmt::format("{}", fnDecl.identifier);
        
        if (fnDecl.linkage_kind == LinkageKind::Extern && fnDecl.body) {
            m_context->report_compiler_diagnostic(decl->loc, "Function marked 'extern' must not have body");
        }

        if (fnDecl.type->function.var_arg && fnDecl.linkage_kind != LinkageKind::Extern) {
            m_context->report_compiler_diagnostic(decl->loc, "Function with variable amount of parameters (vararg) must be marked 'extern'");
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
                    m_context->report_compiler_diagnostic(decl->loc, "Control flow reaches end of function with a non void return type");
                } else {
                    fnDecl.body->block.stmts.append(m_context, Stmt::Create(m_context, decl->loc, StmtKind::Return, ReturnStmt(nullptr)));
                }
            }

            pop_scope();
            m_active_return_type = nullptr;
        } else if (fnDecl.linkage_kind != LinkageKind::Extern) {
            m_context->report_compiler_diagnostic_with_notes(decl->loc, "Body for this function must be specified",
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
            resolve_type(field->field.type);

            ARIA_ASSERT(field->kind == DeclKind::Field, "Invalid field");

            if (s.field_lookup.contains(field->field.identifier)) {
                Decl* prev = s.field_lookup.at(field->field.identifier);
                m_context->report_compiler_diagnostic(field->loc, fmt::format("Redeclaring field '{}'", field->field.identifier));
                m_context->report_compiler_diagnostic(prev->loc, "Previous declaration here", CompilerDiagKind::Note);
            }

            if (field->field.type->is_void()) {
                m_context->report_compiler_diagnostic(field->loc, "Cannot declare field of 'void' type");
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
                            m_context->report_compiler_diagnostic(field->loc, fmt::format("Redeclaring method '{}'", field->method.identifier));
                            m_context->report_compiler_diagnostic(prev->loc, "Previous declaration here", CompilerDiagKind::Note);
                            continue;
                        } else {
                            m_context->report_compiler_diagnostic(field->loc, fmt::format("Redeclaring field '{}' as method", field->method.identifier));
                            m_context->report_compiler_diagnostic(prev->loc, "Previous declaration here", CompilerDiagKind::Note);
                            continue;
                        }
                    }

                    i.field_lookup.insert(m_context, field->method.identifier, field);
                    i.parent->struct_.field_lookup.insert(m_context, field->method.identifier, field);
                    break;
                }
            }
        }

        m_active_struct = TypeInfo::create_struct(m_context, i.parent);

        for (Decl* field : i.fields) {
            switch (field->kind) {
                case DeclKind::Method: {
                    resolve_type(field->method.type->function.return_type);
                    m_active_return_type = field->method.type->function.return_type;
                    push_scope();
                    for (Decl* p : field->method.parameters) {
                        resolve_param_decl(p);
                    }

                    resolve_block_stmt(field->method.body);

                    if (m_scopes.back().reaches_end) {
                        if (!field->method.type->function.return_type->is_void()) {
                            m_context->report_compiler_diagnostic(decl->loc, "Control flow reaches end of function with a non void return type");
                        } else {
                            field->method.body->block.stmts.append(m_context, Stmt::Create(m_context, decl->loc, StmtKind::Return, ReturnStmt(nullptr)));
                        }
                    }

                    pop_scope();
                    m_active_return_type = nullptr;
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

    void SemanticAnalyzer::resolve_enum_decl(Decl* decl) {
        if (decl->resolve_status == ResolveStatus::Done) { return; }
        decl->resolve_status = ResolveStatus::InProgress;

        EnumDecl& e = decl->enum_;
        u64 val = 0;

        for (Decl* field : e.fields) {
            ARIA_ASSERT(field->kind == DeclKind::EnumField, "Invalid enum field");
            EnumFieldDecl& f = field->enum_field;
            
            if (f.value) {
                resolve_expr(f.value);

                if (!is_const_expr(f.value)) {
                    m_context->report_compiler_diagnostic(f.value->loc, "Value of enum field must be a constant expression");
                } else {
                    ConversionCost cost = get_conversion_cost(TypeInfo::get_basic(m_context, TypeKind::Int), f.value->type);
                    if (cost.cast_needed) {
                        if (cost.implicit_cast_possible) {
                            insert_implicit_cast(TypeInfo::get_basic(m_context, TypeKind::Int), f.value->type, f.value, cost.kind);
                        } else {
                            m_context->report_compiler_diagnostic(f.value->loc, fmt::format("Type of expression must be convertable to 'int' but is '{}'", type_info_to_string(f.value->type)));
                        }
                    }

                    Expr* cons = eval_const_expr(f.value);
                    replace_expr(f.value, cons);

                    val = cons->const_.integer;
                    f.resolved_value = val;
                }
            } else {
                val++;
                f.resolved_value = val;
            }

            e.field_lookup.insert(m_context, f.identifier, field);
        }

        decl->resolve_status = ResolveStatus::Done;
    }

    void SemanticAnalyzer::resolve_generic_decl(Decl* decl) {
        GenericDecl& gen = decl->generic;
        
        size_t i = m_generic_types.size();
        for (Decl* t : gen.parameters) {
            m_generic_types.push_back(t);
        }

        resolve_decl(gen.decl);
        m_generic_types.erase(m_generic_types.begin() + i, m_generic_types.end());
    }

    void SemanticAnalyzer::resolve_decl_attributes(Decl* decl, TinyVector<DeclAttribute> attrs, bool* erase_decl) {
        for (auto& attr : attrs) {
            switch (attr.kind) {
                case DeclAttributeKind::If: {
                    resolve_expr(attr.arg);

                    if (!is_const_expr(attr.arg)) {
                        m_context->report_compiler_diagnostic(attr.arg->loc, "Expression must be a compile time constant");
                        break;
                    }

                    if (!attr.arg->type->is_boolean()) {
                        m_context->report_compiler_diagnostic(attr.arg->loc, "Expression must be of type 'bool'");
                        break;
                    }

                    bool result = eval_const_expr(attr.arg)->const_.boolean;
                    if (!result) { *erase_decl = true; }
                    break;
                }
            }
        }
    }

    void SemanticAnalyzer::resolve_decl(Decl* decl) {
        switch (decl->kind) {
            case DeclKind::Error:
            case DeclKind::Module:
            case DeclKind::Field:
            case DeclKind::Method: return;

            case DeclKind::TranslationUnit: return resolve_translation_unit_decl(decl);
            case DeclKind::Var: return resolve_var_decl(decl);
            case DeclKind::Param: return resolve_param_decl(decl);
            case DeclKind::Function: return resolve_function_decl(decl);
            case DeclKind::Struct: return resolve_struct_decl(decl);
            case DeclKind::Impl: return;
            case DeclKind::Typedef: return resolve_typedef_decl(decl);
            case DeclKind::Enum: return resolve_enum_decl(decl);
            case DeclKind::Generic: return resolve_generic_decl(decl);

            default: ARIA_UNREACHABLE();
        }
    }

} // namespace ariac