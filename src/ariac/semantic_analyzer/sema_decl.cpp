#include "ariac/semantic_analyzer/semantic_analyzer.hpp"

namespace ariac {

    void SemanticAnalyzer::resolve_var_decl(Decl* decl) {
        if (decl->resolve_status == ResolveStatus::Done || decl->resolve_status == ResolveStatus::InProgress) { return; }
        decl->resolve_status = ResolveStatus::InProgress;

        VarDecl& varDecl = decl->var;
        std::string_view ident = varDecl.identifier;

        if (varDecl.type) {
            resolve_type(varDecl.type);

            if (varDecl.type->is_void()) {
                context.report_compiler_diagnostic(decl->loc, "Cannot declare variable of 'void' type");
            }
        }

        resolve_var_initializer(decl);

        if (m_scopes.size() > 0) {
            if (m_scopes.back().declarations.contains(ident)) {
                context.report_compiler_diagnostic(decl->loc, fmt::format("Redeclaring symbol '{}'", ident));
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
        FunctionDecl fn = decl->function;
        resolve_type(fn.type);

        std::string ident = fmt::format("{}", fn.identifier);
        
        if (fn.linkage_kind == LinkageKind::Extern && fn.body) {
            context.report_compiler_diagnostic(decl->loc, "Function marked 'extern' must not have body");
        }

        if (fn.type->function.variadic == VariadicKind::Unnamed && fn.linkage_kind != LinkageKind::Extern) {
            context.report_compiler_diagnostic(decl->loc, "C style variadic functions must be marked 'extern'");
        }

        if (!fn.body && fn.linkage_kind != LinkageKind::Extern) {
            context.report_compiler_diagnostic_with_notes(decl->loc, "Body for this function must be specified",
                { "If this function is defined elsewhere, use 'extern'"} );
        }

        decl->resolve_status = ResolveStatus::Done;
    }

    void SemanticAnalyzer::resolve_struct_decl(Decl* decl) {
        if (decl->resolve_status == ResolveStatus::Done) { return; }
        decl->resolve_status = ResolveStatus::InProgress;

        StructDecl& s = decl->struct_;

        if (s.fields.size == 0) {
            context.report_compiler_diagnostic(decl->loc, "Empty structs are not allowed");
        }

        for (Decl* field : s.fields) {
            field->parent_unit = decl->parent_unit;
            field->parent_module = decl->parent_module;
            resolve_type(field->field.type);

            ARIA_ASSERT(field->kind == DeclKind::Field, "Invalid field");

            if (s.field_lookup.contains(field->field.identifier)) {
                Decl* prev = s.field_lookup.at(field->field.identifier);
                context.report_compiler_diagnostic(field->loc, fmt::format("Redeclaring field '{}'", field->field.identifier));
                context.report_compiler_diagnostic(prev->loc, "Previous declaration here", CompilerDiagKind::Note);
            }

            if (field->field.type->is_void()) {
                context.report_compiler_diagnostic(field->loc, "Cannot declare field of 'void' type");
                field->kind = DeclKind::Error;
                continue;
            }

            s.field_lookup.insert(field->field.identifier, field);
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
        StructDecl& s = i.parent->kind == DeclKind::Struct ? i.parent->struct_ : i.parent->generic.decl->struct_;

        for (Decl* field : i.fields) {
            switch (field->kind) {
                case DeclKind::Method: {
                    resolve_type(field->method.type);

                    if (s.field_lookup.contains(field->method.identifier)) {
                        Decl* prev = i.field_lookup.at(field->method.identifier);

                        if (prev->kind == DeclKind::Method) {
                            context.report_compiler_diagnostic(field->loc, fmt::format("Redeclaring method '{}'", field->method.identifier));
                            context.report_compiler_diagnostic(prev->loc, "Previous declaration here", CompilerDiagKind::Note);
                            continue;
                        } else {
                            context.report_compiler_diagnostic(field->loc, fmt::format("Redeclaring field '{}' as method", field->method.identifier));
                            context.report_compiler_diagnostic(prev->loc, "Previous declaration here", CompilerDiagKind::Note);
                            continue;
                        }
                    }

                    i.field_lookup.insert(field->method.identifier, field);
                    s.field_lookup.insert(field->method.identifier, field);
                    break;
                }
            }
        }

        if (!s.type) { s.type = TypeInfo::create_struct(i.parent); }
        decl->resolve_status = ResolveStatus::Done;
    }

    void SemanticAnalyzer::resolve_typedef_decl(Decl* decl) {
        TypedefDecl& td = decl->typedef_;
        resolve_type(td.type);

        while (td.type->is_typedef()) {
            td.type = td.type->typedef_.base;
        }
    }

    void SemanticAnalyzer::resolve_enum_decl(Decl* decl) {
        if (decl->resolve_status == ResolveStatus::Done) { return; }
        decl->resolve_status = ResolveStatus::InProgress;

        EnumDecl& e = decl->enum_;
        u64 val = 0;

        for (Decl* field : e.fields) {
            ARIA_ASSERT(field->kind == DeclKind::EnumConstant, "Invalid enum constant");
            EnumConstantDecl& c = field->enum_constant;
            
            if (c.value) {
                resolve_expr(c.value);

                if (!is_const_expr(c.value)) {
                    context.report_compiler_diagnostic(c.value->loc, "Value of enum constant must be a constant expression");
                } else {
                    try_insert_implicit_cast(TypeInfo::get_basic(TypeKind::Int), c.value);
                    Expr* cons = eval_const_expr(c.value);
                    replace_expr(c.value, cons);

                    val = cons->const_.integer;
                    c.resolved_value = val;
                }
            } else {
                val++;
                c.resolved_value = val;
            }

            e.field_lookup.insert(c.identifier, field);
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

    void SemanticAnalyzer::resolve_function_body(Decl* decl) {
        FunctionDecl& fn = decl->function;

        m_active_return_type = fn.type->function.return_type;
        push_scope();
        
        for (Decl* p : fn.parameters) {
            resolve_param_decl(p);
        }
        
        resolve_block_stmt(fn.body);

        if (m_scopes.back().reaches_end) {
            if (!fn.type->function.return_type->is_void()) {
                context.report_compiler_diagnostic(decl->loc, "Control flow reaches end of function with a non void return type");
            } else {
                fn.body->block.stmts.append(Stmt::Create(decl->loc, StmtKind::Return, ReturnStmt(nullptr)));
            }
        }

        pop_scope();
        m_active_return_type = nullptr;
    }

    void SemanticAnalyzer::resolve_generic_body(Decl* decl) {
        GenericDecl& gen = decl->generic;

        if (gen.decl->kind == DeclKind::Struct) { return; }

        for (Decl* t : gen.parameters) {
            m_generic_types.push_back(t);
        }

        resolve_function_body(gen.decl);
        m_generic_types.clear();

        for (Decl* spec : gen.specilizations) {
            ARIA_ASSERT(spec->kind == DeclKind::FunctionSpecilization, "Invalid function specilization");

            Decl* func = Decl::dup(gen.decl);
            func->parent_module = decl->parent_module;
            func->parent_unit = decl->parent_unit;

            func->function.type = spec->function_specilization.type;
            spec->function_specilization.source = func;

            for (size_t i = 0; i < spec->function_specilization.types.size; i++) {
                m_specialized_generic_types[gen.parameters.items[i]->generic_parameter.identifier] = spec->function_specilization.types.items[i];
            }

            m_replace_generic_types = true;
            resolve_function_body(spec->function_specilization.source);
            m_replace_generic_types = false;
        }
    }

    void SemanticAnalyzer::resolve_method_body(Decl* decl) {
        MethodDecl& m = decl->method;

        m_active_return_type = m.type->function.return_type;
        resolve_struct_decl(m.parent->impl.parent);
        if (!m.is_static) { m_active_struct = TypeInfo::create_struct(m.parent->impl.parent); }
        push_scope();
        
        for (Decl* p : m.parameters) {
            resolve_param_decl(p);
        }
        
        resolve_block_stmt(m.body);

        if (m_scopes.back().reaches_end) {
            if (!m.type->function.return_type->is_void()) {
                context.report_compiler_diagnostic(decl->loc, "Control flow reaches end of method with a non void return type");
            } else {
                m.body->block.stmts.append(Stmt::Create(decl->loc, StmtKind::Return, ReturnStmt(nullptr)));
            }
        }

        pop_scope();
        m_active_struct = nullptr;
        m_active_return_type = nullptr;
    }

    void SemanticAnalyzer::resolve_decl_attributes(Decl* decl, TinyVector<DeclAttribute> attrs, bool* erase_decl) {
        for (auto& attr : attrs) {
            switch (attr.kind) {
                case DeclAttributeKind::If: {
                    resolve_expr(attr.expr);

                    if (!is_const_expr(attr.expr)) {
                        context.report_compiler_diagnostic(attr.expr->loc, "Expression must be a compile time constant");
                        break;
                    }

                    if (!attr.expr->type->is_boolean()) {
                        context.report_compiler_diagnostic(attr.expr->loc, "Expression must be of type 'bool'");
                        break;
                    }

                    bool result = eval_const_expr(attr.expr)->const_.boolean;
                    if (!result) { *erase_decl = true; }
                    break;
                }

                case DeclAttributeKind::Builtin: {
                    if (attr.string == "string") {
                        if (context.string_type) {
                            context.report_compiler_diagnostic(decl->loc, "A type for string is already declared, please remove '@builtin(\"string\")'");
                        } else {
                            if (decl->kind == DeclKind::Typedef && decl->typedef_.type->is_slice() && decl->typedef_.type->slice.base->kind == TypeKind::Char) {
                                context.string_type = decl;
                            } else {
                                context.report_compiler_diagnostic(decl->loc, "Builtin for 'string' must be a typedef for '[]char'");
                            }
                        }
                    } else if (attr.string == "assert") {
                        if (context.assert_func) {
                            context.report_compiler_diagnostic(decl->loc, "A function for assert is already declared, please remove '@builtin(\"assert\")'");
                        } else {
                            if (decl->kind != DeclKind::Function) {
                                context.report_compiler_diagnostic(decl->loc, "Builtin for 'assert' must be a function");
                                break;
                            }

                            FunctionDecl& fn = decl->function;

                            TinyVector<TypeInfo*> param_types;
                            param_types.append(TypeInfo::get_basic(TypeKind::Bool));
                            param_types.append(TypeInfo::get_string());
                            param_types.append(TypeInfo::get_basic(TypeKind::ULong));
                            param_types.append(TypeInfo::get_string());
                            param_types.append(TypeInfo::get_basic(TypeKind::Any));

                            TypeInfo* fn_ty = TypeInfo::create_function(TypeKind::Function, TypeInfo::get_void(), param_types, VariadicKind::Named);

                            if (!type_is_equal(fn_ty, fn.type)) {
                                context.report_compiler_diagnostic(decl->loc, fmt::format("Builtin for 'assert' must have signature '{}'", type_info_to_string(fn_ty)));
                                break;
                            }

                            context.assert_func = decl;
                        }
                    } else {
                        context.report_compiler_diagnostic(decl->loc, fmt::format("Unknown builtin '{}'", attr.string));
                    }
                    break;
                }

                default: ARIA_UNREACHABLE("Invalid decl attribute");
            }
        }
    }

    void SemanticAnalyzer::resolve_decl(Decl* decl) {
        switch (decl->kind) {
            case DeclKind::Error:
            case DeclKind::Module:
            case DeclKind::Field:
            case DeclKind::Method: return;

            case DeclKind::Var: return resolve_var_decl(decl);
            case DeclKind::Param: return resolve_param_decl(decl);
            case DeclKind::Function: return resolve_function_decl(decl);
            case DeclKind::Struct: return resolve_struct_decl(decl);
            case DeclKind::Impl: return;
            case DeclKind::Typedef: return resolve_typedef_decl(decl);
            case DeclKind::Enum: return resolve_enum_decl(decl);
            case DeclKind::Generic: return resolve_generic_decl(decl);

            default: ARIA_UNREACHABLE("Invalid decl kind");
        }
    }

} // namespace ariac