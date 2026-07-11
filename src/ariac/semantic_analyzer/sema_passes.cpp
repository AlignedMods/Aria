#include "ariac/semantic_analyzer/semantic_analyzer.hpp"

namespace ariac {

    void SemanticAnalyzer::pass_imports() {
        for (CompilationUnit* unit : context.compilation_units) {
            if (!unit->parent) { continue; }
            resolve_unit_imports(unit->parent, unit);
        }
    }

    void SemanticAnalyzer::pass_decls() {
        for (Module* mod : context.modules) {
            resolve_module_type_decls(mod);
        }

        for (Module* mod : context.modules) {
            resolve_module_decls(mod);
        }

        if (!context.main_func) {
            fmt::println(stderr, "No main function was found for this executable, please add it.\n");
        }
    }

    void SemanticAnalyzer::pass_code() {
        for (Module* mod : context.modules) {
            resolve_module_code(mod);
        }
    }

    void SemanticAnalyzer::add_unit_to_module(Module* module, CompilationUnit* unit) {
        for (CompilationUnit* comp : module->units) {
            if (comp == unit) {
                return;
            }
        }

        module->units.push_back(unit);
    }

    void SemanticAnalyzer::resolve_unit_imports(Module* module, CompilationUnit* unit) {
        context.active_module = module;
        context.active_comp_unit = unit;

        bool imports_std_core = false;

        for (size_t i = 0; i < unit->imports.size(); i++) {
            Decl* decl = unit->imports[i];

            if (decl->kind == DeclKind::Error) { return; }
            ARIA_ASSERT(decl->kind == DeclKind::Import, "Invalid stmt in Imports");

            if (decl->import.name == module->name) {
                context.report_compiler_diagnostic(decl->loc, "Including self is not allowed");
                decl->kind = DeclKind::Error;
                return;
            }

            Module* resolvedModule = nullptr;

            for (size_t i = 0; i < context.modules.size(); i++) {
                Module* mod = context.modules[i];

                if (mod->name == decl->import.name) {
                    resolvedModule = mod;
                    break;
                }
            }

            if (!resolvedModule) {
                context.report_compiler_diagnostic(decl->loc, fmt::format("Could not find module '{}'", decl->import.name));
                continue;
            }

            if (resolvedModule == context.std_core_module) { imports_std_core = true; }
            decl->import.resolved_module = resolvedModule;
        }

        if (!imports_std_core && context.std_core_module) { // Implicitly import std::core if it is avaliable
            Decl* imp = Decl::Create({}, DeclKind::Import, DeclVisibility::Public, ImportDecl("std::core", context.std_core_module, true));
            unit->imports.push_back(imp);
        }

        add_unit_to_module(module, unit);
    }

    void SemanticAnalyzer::resolve_module_type_decls(Module* module) {
        context.active_module = module;

        for (size_t i = 0; i < module->units.size(); i++) {
            if (module->units[i]->if_attr) {
                resolve_expr(module->units[i]->if_attr);

                if (!is_const_expr(module->units[i]->if_attr)) {
                    context.report_compiler_diagnostic(module->units[i]->if_attr->loc, "Expression must be a compile time constant");
                    break;
                }

                if (!module->units[i]->if_attr->type->is_boolean()) {
                    context.report_compiler_diagnostic(module->units[i]->if_attr->loc, "Expression must be of type 'bool'");
                    break;
                }

                bool result = eval_const_expr(module->units[i]->if_attr)->const_.boolean;
                if (!result) {
                    module->units.erase(module->units.begin() + i);
                    i--;
                    continue;
                }
            }
            
            resolve_unit_type_decls(module, module->units[i]);
        }
    }

    void SemanticAnalyzer::resolve_module_decls(Module* module) {
        context.active_module = module;

        for (CompilationUnit* unit : module->units) {
            resolve_unit_decls(module, unit);
        }
    }

    void SemanticAnalyzer::resolve_unit_type_decls(Module* module, CompilationUnit* unit) {
        for (Decl* struc : unit->structs) {
            struc->parent_module = module;
            struc->parent_unit = unit;

            StructDecl& s = struc->struct_;
            std::string_view ident = s.identifier;

            module->symbols[ident] = struc;
            unit->local_symbols[ident] = struc;
        }

        for (Decl* td : unit->typedefs) {
            td->parent_module = module;
            td->parent_unit = unit;

            TypedefDecl& t = td->typedef_;
            std::string_view ident = t.identifier;

            module->symbols[ident] = td;
            unit->local_symbols[ident] = td;
        }

        for (Decl* en : unit->enums) {
            en->parent_module = module;
            en->parent_unit = unit;

            EnumDecl& e = en->enum_;
            std::string_view ident = e.identifier;

            module->symbols[ident] = en;
            unit->local_symbols[ident] = en;
        }

        for (Decl* g : unit->generics) {
            g->parent_module = module;
            g->parent_unit = unit;

            ARIA_ASSERT(g->kind == DeclKind::Generic, "Invalid generic");
            GenericDecl& gen = g->generic;
            
            switch (gen.decl->kind) {
                case DeclKind::Function: break;

                case DeclKind::Struct: {
                    std::string_view ident = gen.decl->struct_.identifier;

                    gen.decl->struct_.type = TypeInfo::create_basic(TypeKind::GenericInstantiation);
                    gen.decl->struct_.type->generic_instantiation.base = TypeInfo::create_generic_decl(g);
                    gen.decl->struct_.type->generic_instantiation.resolved_decl = g;

                    for (Decl* p : gen.parameters) {
                        gen.decl->struct_.type->generic_instantiation.arguments.append(TypeInfo::create_generic(p->generic_parameter.identifier));
                    }

                    module->symbols[ident] = g;
                    unit->local_symbols[ident] = g;
                    break;
                }

                default: ARIA_UNREACHABLE("Invalid generic decl");
            }
        }
    }

    void SemanticAnalyzer::resolve_unit_decls(Module* module, CompilationUnit* unit) {
        context.active_comp_unit = unit;

        for (Decl* impl : unit->impls) {
            impl->parent_module = module;
            impl->parent_unit = unit;

            ImplDecl& i = impl->impl;
            if (!module->symbols.contains(i.identifier)) {
                context.report_compiler_diagnostic(impl->loc, fmt::format("No such struct '{}' to create an implementation for", i.identifier));
            } else {
                Decl* sym = module->symbols.at(i.identifier);

                switch (sym->kind) {
                    case DeclKind::Struct: {
                        impl->impl.parent = sym;
                        sym->struct_.impls.append(impl);
                        break;
                    }

                    case DeclKind::Generic: {
                        ARIA_ASSERT(sym->generic.decl->kind == DeclKind::Struct, "Invalid generic");

                        impl->impl.parent = sym;
                        sym->generic.decl->struct_.impls.append(impl);
                        break;
                    }

                    default: {
                        context.report_compiler_diagnostic(impl->loc, fmt::format("'{}' is not a struct", i.identifier));
                        context.report_compiler_diagnostic(sym->loc, "Declared here", CompilerDiagKind::Note, sym->parent_unit);
                        break;
                    }
                }
            }
        }

        for (Decl* global : unit->globals) {
            global->parent_module = module;
            global->parent_unit = unit;

            ARIA_ASSERT(global->kind == DeclKind::Var, "Invalid global in globals");

            VarDecl& var = global->var;

            module->symbols[var.identifier] = global;
            unit->local_symbols[var.identifier] = global;       
        }

        for (size_t i = 0; i < unit->generics.size(); i++) {
            Decl* gen = unit->generics[i];
            ARIA_ASSERT(gen->kind == DeclKind::Generic, "Invalid generic decl");

            switch (gen->generic.decl->kind) {
                case DeclKind::Function: {
                    Decl* func = gen->generic.decl;
                    func->parent_module = module;
                    func->parent_unit = unit;

                    ARIA_ASSERT(func->kind == DeclKind::Function, "Invalid generic func");
                    FunctionDecl& f = func->function;

                    size_t i = m_generic_types.size();
                    for (Decl* t : gen->generic.parameters) {
                        m_generic_types.push_back(t);
                    }

                    resolve_type(f.type);
                    m_generic_types.erase(m_generic_types.begin() + i, m_generic_types.end());

                    for (size_t i = 0; i < f.parameters.size; i++) {
                        f.parameters.items[i]->param.type = f.type->function.param_types.items[i];
                    }

                    bool erase = false;
                    resolve_decl_attributes(func, func->attributes, &erase);
                    
                    if (erase) {
                        context.active_comp_unit->generics.erase(context.active_comp_unit->funcs.begin() + i);
                        i--;
                        replace_decl(func, &error_decl);
                        continue;
                    }

                    if (f.identifier == "main") {
                        context.report_compiler_diagnostic(func->loc, "'main' function cannot be generic");
                        continue;
                    }

                    if (module->symbols.contains(f.identifier)) {
                        Decl* d = module->symbols.at(f.identifier);
                    
                        if (d->kind == DeclKind::Function) {
                            context.report_compiler_diagnostic(func->loc, fmt::format("Redefining function '{}' as generic", f.identifier));
                            context.report_compiler_diagnostic(func->loc, "Previous declaration here", CompilerDiagKind::Note, func->parent_unit);
                        } else if (d->kind == DeclKind::Var) {
                            context.report_compiler_diagnostic(func->loc, fmt::format("Redefining global variable '{}' as function", f.identifier));
                            context.report_compiler_diagnostic(func->loc, "Previous declaration here", CompilerDiagKind::Note, func->parent_unit);
                        } else if (d->kind == DeclKind::Struct) {
                            context.report_compiler_diagnostic(func->loc, fmt::format("Redefining struct '{}' as function", f.identifier));
                            context.report_compiler_diagnostic(func->loc, "Previous declaration here", CompilerDiagKind::Note, func->parent_unit);
                        } else {
                            ARIA_UNREACHABLE("Invalid decl kind");
                        }
                    
                        func->kind = DeclKind::Error;
                        continue;
                    }

                    module->symbols[f.identifier] = gen;
                    unit->local_symbols[f.identifier] = gen;
                    break;
                }

                case DeclKind::Struct: break;

                default: ARIA_UNREACHABLE("Invalid generic kind");
            }
        }

        for (size_t i = 0; i < unit->funcs.size(); i++) {
            Decl* func = unit->funcs[i];
            func->parent_module = module;
            func->parent_unit = unit;

            ARIA_ASSERT(func->kind == DeclKind::Function, "Invalid func in funcs");
            FunctionDecl& f = func->function;
            resolve_type(f.type);

            for (size_t i = 0; i < f.parameters.size; i++) {
                f.parameters.items[i]->param.type = f.type->function.param_types.items[i];
            }

            bool erase = false;
            resolve_decl_attributes(func, func->attributes, &erase);
            
            if (erase) {
                context.active_comp_unit->funcs.erase(context.active_comp_unit->funcs.begin() + i);
                i--;
                replace_decl(func, &error_decl);
                continue;
            }

            if (f.identifier == "main") {
                if (context.main_func) {
                    context.report_compiler_diagnostic(func->loc, "Redefining main function");
                    context.report_compiler_diagnostic(context.main_func->loc, "Previous declaration here", CompilerDiagKind::Note, context.main_func->parent_unit);
                    func->kind = DeclKind::Error;
                    continue;
                }

                if (f.parameters.size > 1) {
                    context.report_compiler_diagnostic(func->loc, "Main function must have one or zero parameters");
                }

                if (f.parameters.size >= 1) {
                    TypeInfo* type = TypeInfo::create_with_base(TypeKind::Slice, TypeInfo::get_string());
                    if (!type_is_equal(f.parameters.items[0]->param.type, type)) {
                        context.report_compiler_diagnostic(f.parameters.items[0]->loc, fmt::format("First parameter of 'main' function must be of type '{}'", type_info_to_string(type)));
                    }
                }

                if (!f.type->function.return_type->is_void() && f.type->function.return_type->kind != TypeKind::Int) {
                    context.report_compiler_diagnostic(f.type->loc, "Return type of 'main' function must be 'void' or 'int'");
                }

                module->symbols[f.identifier] = func;
                unit->local_symbols[f.identifier] = func;
                context.main_func = func;
                continue;
            }

            if (module->symbols.contains(f.identifier)) {
                Decl* d = module->symbols.at(f.identifier);

                if (d->kind == DeclKind::Function) {
                    context.report_compiler_diagnostic(func->loc, fmt::format("Redefining function '{}'", f.identifier));
                    context.report_compiler_diagnostic(func->loc, "Previous declaration here", CompilerDiagKind::Note, func->parent_unit);
                } else if (d->kind == DeclKind::Var) {
                    context.report_compiler_diagnostic(func->loc, fmt::format("Redefining global variable '{}' as function", f.identifier));
                    context.report_compiler_diagnostic(func->loc, "Previous declaration here", CompilerDiagKind::Note, func->parent_unit);
                } else if (d->kind == DeclKind::Struct) {
                    context.report_compiler_diagnostic(func->loc, fmt::format("Redefining struct '{}' as function", f.identifier));
                    context.report_compiler_diagnostic(func->loc, "Previous declaration here", CompilerDiagKind::Note, func->parent_unit);
                } else {
                    ARIA_UNREACHABLE("Invalid decl kind");
                }

                func->kind = DeclKind::Error;
                return;
            }

            module->symbols[f.identifier] = func;
            unit->local_symbols[f.identifier] = func;
        }
    }

    void SemanticAnalyzer::resolve_module_code(Module* module) {
        context.active_module = module;

        for (CompilationUnit* unit : module->units) {
            resolve_unit_code(module, unit);
        }
    }

    void SemanticAnalyzer::resolve_unit_code(Module* module, CompilationUnit* unit) {
        context.active_comp_unit = unit;

        for (Decl* struc : unit->structs) {
            ARIA_ASSERT(struc->kind == DeclKind::Struct, "Invalid struct decl");
            resolve_struct_decl(struc);
        }

        for (Decl* var : unit->globals) {
            resolve_var_decl(var);
        }

        for (Decl* impl : unit->impls) {
            ARIA_ASSERT(impl->kind == DeclKind::Impl, "Invalid impl decl");

            for (Decl* method : impl->impl.fields) {
                ARIA_ASSERT(method->kind == DeclKind::Method, "Invalid method decl");
                resolve_method_body(method);
            }
        }

        for (Decl* func : unit->funcs) {
            ARIA_ASSERT(func->kind == DeclKind::Function, "Invalid function decl");

            if (func->function.body) {
                resolve_function_body(func);
            }
        }

        for (Decl* gen : unit->generics) {
            ARIA_ASSERT(gen->kind == DeclKind::Generic, "Invalid generic decl");

            resolve_generic_body(gen);
        }
    }

} // namespace ariac