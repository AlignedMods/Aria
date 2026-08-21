#include "ariac/semantic_analyzer/semantic_analyzer.hpp"

namespace ariac {

    void SemanticAnalyzer::pass_module_heirarchy() {
        for (Module* mod : context.modules) {
            resolve_module_heirarchy(mod);
        }
    }

    void SemanticAnalyzer::pass_imports() {
        for (CompilationUnit* unit : context.compilation_units) {
            if (!unit->parent) { continue; }
            resolve_unit_imports(unit->parent, unit);
        }
    }

    void SemanticAnalyzer::pass_decls() {
        for (Module* mod : context.modules) {
            for (size_t i = 0; i < mod->units.size(); i++) {
                if (mod->units[i]->if_attr) {
                    resolve_expr(mod->units[i]->if_attr);

                    if (!is_const_expr(mod->units[i]->if_attr)) {
                        report_diag(mod->units[i]->if_attr->loc, "Expression must be a compile time constant");
                        break;
                    }

                    if (!mod->units[i]->if_attr->type->is_boolean()) {
                        report_diag(mod->units[i]->if_attr->loc, "Expression must be of type 'bool'");
                        break;
                    }

                    bool result = eval_const_expr(mod->units[i]->if_attr)->const_.boolean;
                    if (!result) {
                        mod->units.erase(mod->units.begin() + i);
                        i--;
                        continue;
                    }
                }
                
                resolve_unit_type_decls(mod, mod->units[i]);
            }
        }

        for (Module* mod : context.modules) {
            for (CompilationUnit* unit : mod->units) {
                resolve_unit_decls(mod, unit);
            }
        }

        if (!context.main_func) {
            fmt::println(stderr, "No main function was found for this executable, please add it.\n");
        }
    }

    void SemanticAnalyzer::pass_code() {
        for (Module* mod : context.modules) {
            for (CompilationUnit* unit : mod->units) {
                resolve_unit_code(mod, unit);
            }
        }
    }

    void SemanticAnalyzer::resolve_module_heirarchy(Module* module) {


        // std::string_view parent = get_parent_path(module->name);
        // 
        // // No parent
        // if (parent.length() == 0) { module->top_module = module; return; }
        // 
        // for (Module* mod : context.modules) {
        //     if (mod->name == parent) {
        //         // We have found the parent
        //         module->parent = mod;
        //         mod->children.push_back(module);
        //         mod->child_lookup[get_bottom_path(module->name)] = module;
        // 
        //         // Set top module
        //         Module* top = mod;
        //         while (top->parent) {
        //             top = top->parent;
        //         }
        //         module->top_module = top;
        //         return;
        //     }
        // }
        // 
        // // No parent module exists so we create one
        // Module* mod = context.find_or_create_module(parent);
        // module->parent = mod;
        // mod->children.push_back(module);
        // mod->child_lookup[get_bottom_path(module->name)] = module;
        // 
        // // Set top module
        // Module* top = mod;
        // while (top->parent) {
        //     top = top->parent;
        // }
        // module->top_module = top;
        // 
        // resolve_module_heirarchy(mod);
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
        context.active_comp_unit = unit;

        // Add top level modules
        for (Module* m : context.modules) {
            Module* top = m->top_module;
            unit->local_modules[top->name] = top;
        }

        // Add the current module's children
        for (Module* c : module->children) {
            unit->local_modules[c->name] = c;
        }

        // Implicitly import the parent modules
        {
            Module* m = module->parent;
            while (m) {
                unit->imported_modules[m->name] = m;

                // Add their children too
                for (Module* c : m->children) {
                    unit->local_modules[c->name] = c;
                }

                m = m->parent;
            }
        }

        for (size_t i = 0; i < unit->imports.size(); i++) {
            Decl* decl = unit->imports[i];
            if (decl->kind == DeclKind::Error) { continue; }
            ARIA_ASSERT(decl->kind == DeclKind::Import, "Invalid stmt in Imports");

            resolve_import_decl(decl);
        }

        add_unit_to_module(module, unit);
    }

    void SemanticAnalyzer::resolve_unit_type_decls(Module* module, CompilationUnit* unit) {
        context.active_comp_unit = unit;

        for (Decl* struc : unit->structs) {
            struc->parent_module = module;
            struc->parent_unit = unit;

            StructDecl* s = nullptr;
            switch (struc->kind) {
                case DeclKind::Struct: {
                    s = &struc->struct_;
                    break;
                }

                case DeclKind::Generic: {
                    s = &struc->generic.decl->struct_;
                    struc->generic.decl->parent_module = module;
                    struc->generic.decl->parent_unit = unit;
                    break;
                }

                default: ARIA_UNREACHABLE("Invalid struct decl");
            }

            if (module->symbols.contains(s->identifier)) {
                Decl* d = module->symbols.at(s->identifier);
                report_diag(struc->loc, fmt::format("Redefining symbol '{}'", s->identifier));
                report_diag(d->loc, "Previous declaration here", CompilerDiagKind::Note);
                struc->kind = DeclKind::Error;
                continue;
            }

            module->symbols[s->identifier] = struc;
        }

        for (size_t i = 0; i < unit->typedefs.size(); i++) {
            Decl* td = unit->typedefs[i];

            td->parent_module = module;
            td->parent_unit = unit;

            TypedefDecl& t = td->typedef_;

            bool erase = false;
            resolve_decl_attributes(td, td->attributes, &erase);
            
            if (erase) {
                context.active_comp_unit->typedefs.erase(context.active_comp_unit->typedefs.begin() + i);
                i--;
                replace_decl(td, &error_decl);
                continue;
            }

            if (module->symbols.contains(t.identifier)) {
                Decl* d = module->symbols.at(t.identifier);
                report_diag(td->loc, fmt::format("Redefining symbol '{}'", t.identifier));
                report_diag(d->loc, "Previous declaration here", CompilerDiagKind::Note);
                td->kind = DeclKind::Error;
                continue;
            }

            module->symbols[t.identifier] = td;
        }

        for (size_t i = 0; i < unit->enums.size(); i++) {
            Decl* en = unit->enums[i];

            en->parent_module = module;
            en->parent_unit = unit;

            EnumDecl& e = en->enum_;

            bool erase = false;
            resolve_decl_attributes(en, en->attributes, &erase);

            if (erase) {
                context.active_comp_unit->enums.erase(context.active_comp_unit->enums.begin() + i);
                i--;
                replace_decl(en, &error_decl);
                continue;
            }

            if (module->symbols.contains(e.identifier)) {
                Decl* d = module->symbols.at(e.identifier);
                report_diag(en->loc, fmt::format("Redefining symbol '{}'", e.identifier));
                report_diag(d->loc, "Previous declaration here", CompilerDiagKind::Note);
                en->kind = DeclKind::Error;
                continue;
            }

            module->symbols[e.identifier] = en;
        }
    }

    void SemanticAnalyzer::resolve_unit_decls(Module* module, CompilationUnit* unit) {
        context.active_comp_unit = unit;

        for (Decl* global : unit->globals) {
            global->parent_module = module;
            global->parent_unit = unit;

            ARIA_ASSERT(global->kind == DeclKind::Var, "Invalid global in globals");

            VarDecl& var = global->var;

            if (module->symbols.contains(var.identifier)) {
                Decl* d = module->symbols.at(var.identifier);
                report_diag(global->loc, fmt::format("Redefining symbol '{}'", var.identifier));
                report_diag(d->loc, "Previous declaration here", CompilerDiagKind::Note);
                global->kind = DeclKind::Error;
                continue;
            }

            module->symbols[var.identifier] = global;    
        }

        for (size_t i = 0; i < unit->funcs.size(); i++) {
            Decl* func = unit->funcs[i];
            func->parent_module = module;
            func->parent_unit = unit;
            bool generic = false;

            switch (func->kind) {
                case DeclKind::Function: break;
                case DeclKind::Generic: {
                    func = func->generic.decl;
                    func->parent_module = module;
                    func->parent_unit = unit;
                    generic = true;
                    break;
                }

                default: ARIA_UNREACHABLE("Invalid func in funcs");
            }

            FunctionDecl& f = func->function;

            if (generic) {
                GenericContext ctx;
                for (Decl* p : unit->funcs[i]->generic.parameters) {
                    ctx[p->generic_parameter.identifier] = p;
                }
                m_generics.push_back(ctx);
                resolve_type(f.type);
                m_generics.pop_back();
            } else {
                resolve_type(f.type);
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
                if (generic) {
                    report_diag(func->loc, "Main function must not be generic");
                    continue;
                }

                if (context.main_func) {
                    report_diag(func->loc, "Redefining main function");
                    report_diag(context.main_func->loc, "Previous declaration here", CompilerDiagKind::Note);
                    func->kind = DeclKind::Error;
                    continue;
                }

                if (f.type->function.params.size > 1) {
                    ARIA_ASSERT(func, "Func was null");
                    report_diag(func->loc, "Main function must have one or zero parameters");
                }

                if (f.type->function.params.size >= 1) {
                    TypeInfo* type = TypeInfo::create_slice(TypeInfo::get_string());
                    if (!type_is_equal(f.type->function.params.items[0]->param.type, type)) {
                        report_diag(f.type->function.params.items[0]->loc, fmt::format("First parameter of 'main' function must be of type '{}'", type_info_to_string(type)));
                    }
                }

                if (!f.type->function.return_type->is_void() && f.type->function.return_type->kind != TypeKind::Int) {
                    report_diag(func->loc, "Return type of 'main' function must be 'void' or 'int'");
                    continue;
                }

                module->symbols[f.identifier] = func;
                context.main_func = func;
                continue;
            }

            if (module->symbols.contains(f.identifier)) {
                Decl* d = module->symbols.at(f.identifier);
                report_diag(func->loc, fmt::format("Redefining symbol '{}'", f.identifier));
                report_diag(d->loc, "Previous declaration here", CompilerDiagKind::Note);
                func->kind = DeclKind::Error;
                continue;
            }

            module->symbols[f.identifier] = unit->funcs[i];
        }
    }

    void SemanticAnalyzer::resolve_unit_code(Module* module, CompilationUnit* unit) {
        context.active_comp_unit = unit;

        for (Decl* struc : unit->structs) {
            switch (struc->kind) {
                case DeclKind::Error: break;

                case DeclKind::Struct: {
                    resolve_struct_body(struc);
                    break;
                }

                case DeclKind::Generic: {
                    if (struc->generic.decl->resolve_status == ResolveStatus::NotStarted) {
                        GenericContext ctx;
                        for (Decl* p : struc->generic.parameters) {
                            ctx[p->generic_parameter.identifier] = p;
                        }
                        m_generics.push_back(ctx);
                        resolve_struct_body(struc->generic.decl);
                        m_generics.pop_back();
                    }
                    break;
                }

                default: ARIA_UNREACHABLE("Invalid struct");
            }
        }

        for (Decl* struc : unit->enums) {
            ARIA_ASSERT(struc->kind == DeclKind::Enum, "Invalid enum decl");
            resolve_enum_decl(struc);
        }

        for (Decl* var : unit->globals) {
            if (var->kind == DeclKind::Error) { continue; }
            resolve_var_decl(var);
        }

        for (Decl* func : unit->funcs) {
            switch (func->kind) {
                case DeclKind::Error: break;

                case DeclKind::Function: {
                    resolve_function_body(func);
                    break;
                }

                case DeclKind::Generic: {
                    if (func->generic.decl->resolve_status == ResolveStatus::NotStarted) {
                        GenericContext ctx;
                        for (Decl* p : func->generic.parameters) {
                            ctx[p->generic_parameter.identifier] = p;
                        }
                        m_generics.push_back(ctx);
                        resolve_function_body(func->generic.decl);
                        m_generics.pop_back();
                    }
                    break;
                }

                default: ARIA_UNREACHABLE("Invalid function");
            }
        }
    }

} // namespace ariac