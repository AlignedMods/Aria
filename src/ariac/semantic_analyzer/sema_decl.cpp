#include "ariac/semantic_analyzer/semantic_analyzer.hpp"

namespace ariac {

    void SemanticAnalyzer::resolve_import_decl(Decl* decl) {
        ImportDecl& i = decl->import;

        auto handle_import = [&](Module* mod) {
            decl->import.resolved_module = mod;
            context.active_comp_unit->imported_modules[mod->name] = mod;

            for (Module* c : mod->children) {
                context.active_comp_unit->local_modules[c->name] = c;
            }

            {
                Module* m = mod->parent;
                while (m) {
                    context.active_comp_unit->imported_modules[m->name] = m;

                    // Add their children too
                    for (Module* c : m->children) {
                        context.active_comp_unit->local_modules[c->name] = c;
                    }

                    m = m->parent;
                }
            }
        };

        if (i.parent) {
            resolve_import_decl(i.parent);

            if (i.parent->kind == DeclKind::Error) {
                decl->kind = DeclKind::Error;
                return;
            }

            if (i.parent->import.resolved_module->child_lookup.contains(i.name)) {
                handle_import(i.parent->import.resolved_module->child_lookup.at(i.name));
                return;
            }

            context.report_compiler_diagnostic(decl->loc, fmt::format("No such module '{}' in '{}'", i.name, i.parent->import.name));
            decl->kind = DeclKind::Error;
            return;
        }

        if (!context.module_lookup.contains(i.name)) {
            context.report_compiler_diagnostic(decl->loc, fmt::format("No such module '{}' in global scope", i.name));
            decl->kind = DeclKind::Error;
            return;
        }

        handle_import(context.module_lookup.at(i.name));
    }

    void SemanticAnalyzer::resolve_var_decl(Decl* decl) {
        if (decl->resolve_status == ResolveStatus::Done || decl->resolve_status == ResolveStatus::InProgress) { return; }
        decl->resolve_status = ResolveStatus::InProgress;

        VarDecl& var = decl->var;
        std::string_view ident = var.identifier;

        resolve_var_initializer(decl);

        resolve_type(var.type);
        if (var.type->is_void()) {
            report_diag(decl->loc, "Cannot declare variable of type 'void'");
        } else if (var.type->is_function()) {
            report_diag(decl->loc, fmt::format("Cannot declare variable of function type '{}'", type_info_to_string(var.type)));
        }

        if (Decl* dtor = type_get_destructor(var.type)) {
            ARIA_ASSERT(dtor->kind == DeclKind::Destructor, "Invalid destructor");

            // If we are in a function, we can insert a defer
            if (m_functions.size() > 0) {
                TypeInfo* type = TypeInfo::get_void_method();
                Expr* ref = Expr::Create(decl->loc, ExprKind::DeclRef, ExprValueKind::LValue, var.type, DeclRefExpr(var.identifier, nullptr, decl));
                Expr* mem = Expr::Create(decl->loc, ExprKind::Member, ExprValueKind::LValue, type, MemberExpr("~", ref, dtor));
                Expr* call = Expr::Create(decl->loc, ExprKind::MethodCall, ExprValueKind::RValue, type->function.return_type, CallExpr(mem, {}));

                Stmt* defer = Stmt::Create(decl->loc, StmtKind::Expr, call);
                m_functions.back().scopes.back().defers.push_back(defer);
            } else { // Otherwise just inform the codegen
                var.dtor = dtor;
            }
        }

        if (m_functions.size() > 0) {
            if (m_functions.back().scopes.back().declarations.contains(ident)) {
                report_diag(decl->loc, fmt::format("Redeclaring symbol '{}'", ident));
            }

            m_functions.back().scopes.back().declarations[ident] = { var.type, decl, DeclKind::Var };
        }

        decl->resolve_status = ResolveStatus::Done;
    }

    void SemanticAnalyzer::resolve_param_decl(Decl* decl) {
        ParamDecl& param = decl->param;
        resolve_type(param.type);

        resolve_param_default_arg(decl);

        if (param.type->is_void()) {
            report_diag(decl->loc, "Cannot declare parameter of type 'void'");
        } else if (param.type->is_function()) {
            report_diag(decl->loc, fmt::format("Cannot declare parameter of function type '{}'", type_info_to_string(param.type)));
        }

        if (m_functions.back().scopes.back().declarations.contains(param.identifier)) {
            report_diag(decl->loc, fmt::format("Redeclaring symbol '{}'", param.identifier));
        }

        m_functions.back().scopes.back().declarations[param.identifier] = { param.type, decl, DeclKind::Param };
    }

    void SemanticAnalyzer::resolve_function_decl(Decl* decl) {
        if (decl->resolve_status == ResolveStatus::Done || decl->resolve_status == ResolveStatus::InProgress) { return; }
        decl->resolve_status = ResolveStatus::InProgress;
        FunctionDecl fn = decl->function;
        resolve_type(fn.type);

        for (Decl* p : fn.type->function.params) {
            resolve_param_default_arg(p);
        }

        std::string ident = fmt::format("{}", fn.identifier);
        
        if (fn.linkage_kind == LinkageKind::Extern && fn.body) {
            report_diag(decl->loc, "Function marked 'extern' must not have a body");
        }

        if (fn.type->function.variadic == VariadicKind::Unnamed && fn.linkage_kind != LinkageKind::Extern) {
            report_diag(decl->loc, "C style variadic functions must be marked 'extern'");
        }

        if (!fn.body && !fn.is_deleted && fn.linkage_kind != LinkageKind::Extern) {
            report_diag_with_notes(decl->loc, "Body for this function must be specified",
                { "If this function is defined elsewhere, use 'extern'"} );
        }

        decl->resolve_status = ResolveStatus::Done;
    }

    void SemanticAnalyzer::resolve_struct_decl(Decl* decl) {
        if (decl->resolve_status == ResolveStatus::Done) { return; }
        decl->resolve_status = ResolveStatus::InProgress;

        StructDecl& s = decl->struct_;
        if (s.fields.size == 0) { report_diag(decl->loc, "Empty structs are not allowed"); }
        
        // First resolve all the fields
        for (Decl* field : s.fields) {
            if (field->kind != DeclKind::Field) { continue; }

            field->parent_unit = decl->parent_unit;
            field->parent_module = decl->parent_module;
            resolve_type(field->field.type);
        
            if (s.field_lookup.contains(field->field.identifier)) {
                Decl* prev = s.field_lookup.at(field->field.identifier);
                report_diag(field->loc, fmt::format("Redeclaring field '{}'", field->field.identifier));
                report_diag(prev->loc, "Previous declaration here", CompilerDiagKind::Note);
            }
        
            if (field->field.type->is_void()) {
                report_diag(field->loc, "Cannot declare field of 'void' type");
                field->kind = DeclKind::Error;
                continue;
            }
        
            s.field_lookup.insert(field->field.identifier, field);
        }

        decl->resolve_status = ResolveStatus::Done;

        // Then do further analysis
        for (Decl* field : s.fields) {
            switch (field->kind) {
                case DeclKind::Field: {
                    if (Decl* dtor = type_get_destructor(field->field.type)) {
                        if (!decl->struct_.field_lookup.contains("~")) {
                            report_diag_with_notes(decl->loc, fmt::format("Field '{}' has a destructor, but the struct does not", field->field.identifier),
                                { "Did you mean to provide an empty destructor for this struct?" });
                            return;
                        }
                    }

                    break;
                }

                case DeclKind::Method: {
                    resolve_type(field->method.type);

                    if (s.field_lookup.contains(field->method.identifier)) {
                        Decl* prev = s.field_lookup.at(field->method.identifier);

                        if (prev->kind == DeclKind::Method) {
                            report_error(field->loc, fmt::format("Redeclaring method '{}'", field->method.identifier));
                            report_note(prev->loc, "Previous declaration here");
                            break;
                        } else {
                            report_error(field->loc, fmt::format("Redeclaring field '{}' as method", field->method.identifier));
                            report_note(prev->loc, "Previous declaration here");
                            break;
                        }
                    }

                    s.field_lookup.insert(field->method.identifier, field);
                    break;
                }

                case DeclKind::Destructor: {
                    if (s.field_lookup.contains("~")) {
                        Decl* prev = s.field_lookup.at("~");

                        report_error(field->loc, "Redeclaring destructor");
                        report_note(prev->loc, "Previous declaration here");
                        continue;
                    }

                    s.field_lookup.insert("~", field);
                    break;
                }

                default: ARIA_UNREACHABLE("Invalid field kind");
            }
        }
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
        resolve_type(e.backing_type);

        if (!TypeInfo::get_flattened(e.backing_type)->is_integral()) {
            report_diag(decl->loc, fmt::format("Backing type for enum must be an integral type but is '{}'", type_info_to_string(e.backing_type)));
        }

        u64 val = -1;

        for (Decl* field : e.fields) {
            ARIA_ASSERT(field->kind == DeclKind::EnumConstant, "Invalid enum constant");
            EnumConstantDecl& c = field->enum_constant;
            
            if (c.value) {
                resolve_expr(c.value);

                if (!is_const_expr(c.value)) {
                    report_diag(c.value->loc, "Value of enum constant must be a constant expression");
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

    void SemanticAnalyzer::resolve_template_decl(Decl* decl) {
        TemplateDecl& t = decl->template_;
        
        TemplateContext ctx;
        for (Decl* p : t.parameters) {
            ctx[p->template_param.identifier] = p;
        }
        m_generics.push_back(ctx);
        resolve_decl(t.template_decl);
        m_generics.pop_back();
    }

    void SemanticAnalyzer::resolve_function_body(Decl* decl) {
        FunctionDecl& fn = decl->function;
        resolve_function_decl(decl);

        if (!decl->function.body) { return; }

        decl->resolve_status = ResolveStatus::InProgress;

        FunctionContext ctx;
        ctx.return_type = fn.type->function.return_type;
        m_functions.push_back(ctx);

        push_scope();
        
        for (Decl* p : fn.type->function.params) {
            resolve_param_decl(p);
        }
        
        resolve_compound_stmt(fn.body);

        if (m_functions.back().scopes.back().reaches_end) {
            if (fn.type->function.return_type->is_never()) {
                report_diag(decl->loc, "Function with return type '!' should not return");
            } else if (!fn.type->function.return_type->is_void()) {
                report_diag(decl->loc, "Missing return statement in function");
            }
        }

        pop_scope();
        m_functions.pop_back();

        for (Decl* p : fn.type->function.params) {
            if (!p->used && p->param.identifier[0] != '_') {
                report_warning(p->loc, fmt::format("Unused parameter '{}'", p->param.identifier));
                report_note(p->loc, "If this is intentional, prefix the name with '_'");
            }
        }

        decl->resolve_status = ResolveStatus::Done;
    }

    void SemanticAnalyzer::resolve_struct_body(Decl* decl) {
        StructDecl& s = decl->struct_;
        s.body_resolve_status = ResolveStatus::InProgress;

        for (Decl* f : s.fields) {
            switch (f->kind) {
                case DeclKind::Field: break;
                case DeclKind::Method: resolve_method_body(f); break;
                case DeclKind::Destructor: resolve_destructor_body(f); break;

                default: ARIA_UNREACHABLE("Invalid struct field");
            }
        }

        s.body_resolve_status = ResolveStatus::Done;
    }

    void SemanticAnalyzer::resolve_method_body(Decl* decl) {
        MethodDecl& m = decl->method;
        
        FunctionContext ctx;
        ctx.return_type = m.type->function.return_type;
        ctx.struct_type = type_for_self(m.parent);
        m_functions.push_back(ctx);
        
        push_scope();
        
        for (Decl* p : m.type->function.params) {
            resolve_param_decl(p);
        }
        
        resolve_compound_stmt(m.body);
        
        if (m_functions.back().scopes.back().reaches_end) {
            if (!m.type->function.return_type->is_void()) {
                report_diag(decl->loc, "Missing return statement in method");
            }
        }
        
        pop_scope();
        m_functions.pop_back();
    }

    void SemanticAnalyzer::resolve_destructor_body(Decl* decl) {
        DestructorDecl& d = decl->destructor;

        FunctionContext ctx;
        ctx.return_type = TypeInfo::get_void();
        ctx.struct_type = type_for_self(d.parent);
        m_functions.push_back(ctx);

        push_scope();

        TinyVector<Decl*> fields;

        switch (ctx.struct_type->kind) {
            case TypeKind::Struct: fields = ctx.struct_type->struct_.get_fields(); break;
            case TypeKind::StructSpecilization: fields = ctx.struct_type->struct_specilization.get_fields(); break;

            default: ARIA_UNREACHABLE("Invalid self type");
        }

        for (Decl* field : fields) {
            if (field->kind != DeclKind::Field) { continue; }

            if (Decl* dtor = type_get_destructor(field->field.type)) {
                Expr* self = Expr::Create(decl->loc, ExprKind::Self, ExprValueKind::LValue,
                    TypeInfo::create_pointer(ctx.struct_type, false), ErrorExpr());

                Expr* field_member = Expr::Create(decl->loc, ExprKind::Member, ExprValueKind::LValue, field->field.type,
                    MemberExpr(field->field.identifier, self, field));
                field_member->member.implicit_deref = true;

                Expr* field_dtor = Expr::Create(decl->loc, ExprKind::Member, ExprValueKind::LValue,
                    TypeInfo::get_void_method(),
                    MemberExpr("~", field_member, dtor));

                Expr* dtor_call = Expr::Create(decl->loc, ExprKind::MethodCall, ExprValueKind::RValue, TypeInfo::get_void(),
                    CallExpr(field_dtor, {}));

                Stmt* defer = Stmt::Create(decl->loc, StmtKind::Expr, dtor_call);
                ctx.scopes.back().defers.push_back(defer);
            }
        }

        resolve_compound_stmt(d.body);
        pop_scope();
        m_functions.pop_back();
    }

    Decl* SemanticAnalyzer::specialize_template_func(SourceLoc loc, Decl* t, TinyVector<TypeInfo*> args) {
        Decl* specilization = nullptr;
        for (Decl* i : t->template_.specilizations) {
            ARIA_ASSERT(i->kind == DeclKind::Function, "Invalid template specilization");
            ARIA_ASSERT(i->function.is_specilization, "Function should be a specilization");
        
            bool failed = false;
            for (size_t idx = 0; idx < args.size; idx++) {
                if (!type_is_equal(args.items[idx], i->function.specilization_info.types.items[idx])) { failed = true; break; }
            }
        
            if (!failed) { specilization = i; }
        }
        
        // Create specilization if needed
        if (!specilization) {
            // Resolve the body for the generic function if it isn't yet resolved
            if (t->template_.template_decl->resolve_status == ResolveStatus::NotStarted) {
                TemplateContext ctx;
                for (Decl* p : t->template_.parameters) {
                    ctx[p->template_param.identifier] = p;
                }
                m_generics.push_back(ctx);
        
                CompilationUnit* unit = context.active_comp_unit;
                context.active_comp_unit = t->parent_unit;
                resolve_function_body(t->template_.template_decl);
                context.active_comp_unit = unit;
        
                m_generics.pop_back();
            }
        
            TemplateInstantationContext ctx;
            ctx.loc = loc;
        
            for (size_t i = 0; i < args.size; i++) {
                Decl* gen_param = t->template_.parameters.items[i];
                TypeInfo* gen_arg = args.items[i];
                ARIA_ASSERT(gen_param->kind == DeclKind::TemplateParam, "Invalid template parameter");
                ctx.template_types[gen_param] = gen_arg;
            }
        
            m_generic_instantations.push_back(ctx);
        
            TypeInfo* new_type = TypeInfo::dup(t->template_.template_decl->function.type);
            resolve_type(new_type);
            specilization = Decl::dup(t->template_.template_decl);
            specilization->parent_module = t->parent_module;
            specilization->parent_unit = t->parent_unit;
            specilization->function.type = new_type;
            specilization->function.is_specilization = true;
            specilization->function.specilization_info.types = args;
            specilization->function.specilization_info.instantiation_loc = loc;
        
            CompilationUnit* unit = context.active_comp_unit;
            context.active_comp_unit = specilization->parent_unit;
            resolve_function_body(specilization);
            context.active_comp_unit = unit;
        
            t->template_.specilizations.append(specilization);
            m_generic_instantations.pop_back();
        }

        return specilization;
    }

    void SemanticAnalyzer::resolve_decl_attributes(Decl* decl, TinyVector<DeclAttribute> attrs, bool* erase_decl) {
        for (auto& attr : attrs) {
            switch (attr.kind) {
                case DeclAttributeKind::If: {
                    resolve_expr(attr.expr);

                    if (!is_const_expr(attr.expr)) {
                        report_diag(attr.expr->loc, "Expression must be a compile time constant");
                        break;
                    }

                    if (!attr.expr->type->is_boolean()) {
                        report_diag(attr.expr->loc, "Expression must be of type 'bool'");
                        break;
                    }

                    bool result = eval_const_expr(attr.expr)->const_.boolean;
                    if (!result) { *erase_decl = true; }
                    break;
                }

                case DeclAttributeKind::Builtin: {
                    if (attr.string == "panic") {
                        if (context.panic_func) {
                            report_diag(decl->loc, "A function for panic is already declared, please remove '@builtin(\"panic\")'");
                            break;
                        }

                        if (decl->kind != DeclKind::Function) {
                            report_diag(decl->loc, "Builtin for 'panic' must be a function");
                            break;
                        }

                        FunctionDecl& fn = decl->function;

                        TinyVector<Decl*> params;
                        params.append(Decl::Create({}, DeclKind::Param, DeclVisibility::Public, ParamDecl("file", TypeInfo::get_string(), nullptr, false)));
                        params.append(Decl::Create({}, DeclKind::Param, DeclVisibility::Public, ParamDecl("line", TypeInfo::get_basic(TypeKind::ULong), nullptr, false)));
                        params.append(Decl::Create({}, DeclKind::Param, DeclVisibility::Public, ParamDecl("format", TypeInfo::get_string(), nullptr, false)));
                        params.append(Decl::Create({}, DeclKind::Param, DeclVisibility::Public, ParamDecl("args", TypeInfo::get_basic(TypeKind::Any), nullptr, true)));

                        TypeInfo* fn_ty = TypeInfo::create_function(TypeKind::Function, TypeInfo::get_basic(TypeKind::Never), params, params.size, VariadicKind::Named);

                        if (!type_is_equal(fn_ty, fn.type)) {
                            report_diag(decl->loc, fmt::format("Builtin for 'panic' must have signature '{}'", type_info_to_string(fn_ty)));
                            break;
                        }

                        context.panic_func = decl;
                    } else if (attr.string == "TypeKind") {
                        if (context.typekind_type) {
                            report_diag(decl->loc, "A type for TypeKind is already declared, please remove '@builtin(\"TypeKind\")'");
                            break;
                        } 

                        if (decl->kind != DeclKind::Enum) {
                            report_diag(decl->loc, "Builtin for 'TypeInfo' must be an enum");
                            break;
                        }

                        EnumDecl& e = decl->enum_;
                        resolve_type(e.backing_type);

                        if (e.backing_type->kind != TypeKind::Char) {
                            report_diag(decl->loc, "Builtin for 'TypeKind' must have backing type 'char'");
                            break;
                        }

                        context.typekind_type = decl;
                    } else if (attr.string == "memcpy") {
                        if (decl->kind != DeclKind::Function) {
                            report_diag(decl->loc, "Builtin for 'memcpy' must be a function");
                            break;
                        }

                        FunctionDecl& fn = decl->function;

                        TinyVector<Decl*> params;
                        params.append(Decl::Create({}, DeclKind::Param, DeclVisibility::Public, ParamDecl("dst", TypeInfo::get_void_ptr(), nullptr, false)));
                        params.append(Decl::Create({}, DeclKind::Param, DeclVisibility::Public, ParamDecl("src", TypeInfo::get_void_ptr(), nullptr, false)));
                        params.append(Decl::Create({}, DeclKind::Param, DeclVisibility::Public, ParamDecl("len", TypeInfo::get_sz(), nullptr, false)));

                        TypeInfo* fn_ty = TypeInfo::create_function(TypeKind::Function, TypeInfo::get_basic(TypeKind::Void), params, params.size, VariadicKind::None);

                        if (!type_is_equal(fn_ty, fn.type)) {
                            report_diag(decl->loc, fmt::format("Builtin for 'memcpy' must have signature '{}'", type_info_to_string(fn_ty)));
                            break;
                        }

                        fn.builtin_func = BuiltinFuncKind::Memcpy;
                    } else if (attr.string == "memset") {
                        if (decl->kind != DeclKind::Function) {
                            report_diag(decl->loc, "Builtin for 'memset' must be a function");
                            break;
                        }

                        FunctionDecl& fn = decl->function;

                        TinyVector<Decl*> params;
                        params.append(Decl::Create({}, DeclKind::Param, DeclVisibility::Public, ParamDecl("ptr", TypeInfo::get_void_ptr(), nullptr, false)));
                        params.append(Decl::Create({}, DeclKind::Param, DeclVisibility::Public, ParamDecl("val", TypeInfo::get_basic(TypeKind::Char), nullptr, false)));
                        params.append(Decl::Create({}, DeclKind::Param, DeclVisibility::Public, ParamDecl("len", TypeInfo::get_sz(), nullptr, false)));

                        TypeInfo* fn_ty = TypeInfo::create_function(TypeKind::Function, TypeInfo::get_basic(TypeKind::Void), params, params.size, VariadicKind::None);

                        if (!type_is_equal(fn_ty, fn.type)) {
                            report_diag(decl->loc, fmt::format("Builtin for 'memset' must have signature '{}'", type_info_to_string(fn_ty)));
                            break;
                        }

                        fn.builtin_func = BuiltinFuncKind::Memset;
                    } else {
                        report_diag(decl->loc, fmt::format("Unknown builtin '{}'", attr.string));
                    }

                    break;
                }

                case DeclAttributeKind::Init: {
                    if (decl->kind != DeclKind::Function) {
                        report_diag(decl->loc, "Only functions may have '@init'");
                        break;
                    }

                    FunctionDecl& fn = decl->function;
                    TypeInfo* fn_ty = TypeInfo::create_function(TypeKind::Function, TypeInfo::get_void(), {}, 0, VariadicKind::None);

                    if (!type_is_equal(fn_ty, fn.type)) {
                        report_diag(decl->loc, fmt::format("Function marked '@init' must have signature '{}'", type_info_to_string(fn_ty)));
                        break;
                    }

                    break;
                }

                case DeclAttributeKind::Set: {
                    if (decl->kind != DeclKind::Function) {
                        report_diag(decl->loc, "Only functions may have '@set'");
                        break;
                    }

                    bool call = m_sema_context.call;
                    m_sema_context.call = true;
                    resolve_expr(attr.expr);
                    m_sema_context.call = call;

                    if (attr.expr->kind == ExprKind::Error) { break; }
                    ARIA_ASSERT(attr.expr->kind == ExprKind::DeclRef, "Invalid expr");

                    if (attr.expr->decl_ref.referenced_decl->kind != DeclKind::FunctionOverloadSet) {
                        report_error(decl->loc, "'@set' must reference a function overload set");
                        break;
                    }
                    
                    attr.expr->decl_ref.referenced_decl->function_overload_set.funcs.append(decl);
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
            case DeclKind::Typedef: return resolve_typedef_decl(decl);
            case DeclKind::Enum: return resolve_enum_decl(decl);
            case DeclKind::Template: return resolve_template_decl(decl);

            default: ARIA_UNREACHABLE("Invalid decl kind");
        }
    }

} // namespace ariac