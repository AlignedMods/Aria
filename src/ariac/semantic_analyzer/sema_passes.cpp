#include "ariac/semantic_analyzer/semantic_analyzer.hpp"

namespace ariac {

    void SemanticAnalyzer::pass_imports() {
        for (CompilationUnit* unit : m_context->compilation_units) {
            if (!unit->parent) { continue; }
            resolve_unit_imports(unit->parent, unit);
        }
    }

    void SemanticAnalyzer::pass_decls() {
        for (Module* mod : m_context->modules) {
            resolve_module_type_decls(mod);
        }

        for (Module* mod : m_context->modules) {
            resolve_module_decls(mod);
        }

        if (!m_context->main_func) {
            fmt::println(stderr, "No main function was found for this executable, please add it.\n");
        }
    }

    void SemanticAnalyzer::pass_code() {
        for (Module* mod : m_context->modules) {
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
        m_context->active_module = module;
        m_context->active_comp_unit = unit;

        bool imports_std_core = false;

        for (size_t i = 0; i < unit->imports.size(); i++) {
            Stmt* stmt = unit->imports[i];

            if (stmt->kind == StmtKind::Error) { return; }
            ARIA_ASSERT(stmt->kind == StmtKind::Import, "Invalid stmt in Imports");

            if (stmt->import.name == module->name) {
                m_context->report_compiler_diagnostic(stmt->loc, "Including self is not allowed");
                stmt->kind = StmtKind::Error;
                return;
            }

            Module* resolvedModule = nullptr;

            for (size_t i = 0; i < m_context->modules.size(); i++) {
                Module* mod = m_context->modules[i];

                if (mod->name == stmt->import.name) {
                    resolvedModule = mod;
                    break;
                }
            }

            if (!resolvedModule) {
                m_context->report_compiler_diagnostic(stmt->loc, fmt::format("Could not find module '{}'", stmt->import.name));
                continue;
            }

            if (resolvedModule == m_context->std_core_module) { imports_std_core = true; }
            stmt->import.resolved_module = resolvedModule;
        }

        if (!imports_std_core && m_context->std_core_module) { // Implicitly import std::core if it is avaliable
            Stmt* imp = Stmt::Create(m_context, {}, StmtKind::Import, ImportStmt("std::core", m_context->std_core_module));
            unit->imports.push_back(imp);
        }

        add_unit_to_module(module, unit);
    }

    void SemanticAnalyzer::resolve_module_type_decls(Module* module) {
        m_context->active_module = module;

        for (CompilationUnit* unit : module->units) {
            resolve_unit_type_decls(module, unit);
        }
    }

    void SemanticAnalyzer::resolve_module_decls(Module* module) {
        m_context->active_module = module;

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
                case DeclKind::Struct: {
                    std::string_view ident = gen.decl->struct_.identifier;

                    module->symbols[ident] = g;
                    unit->local_symbols[ident] = g;
                    break;
                }

                default: ARIA_UNREACHABLE();
            }
        }
    }

    void SemanticAnalyzer::resolve_unit_decls(Module* module, CompilationUnit* unit) {
        m_context->active_comp_unit = unit;

        for (Decl* impl : unit->impls) {
            impl->parent_module = module;
            impl->parent_unit = unit;

            ImplDecl& i = impl->impl;
            if (!module->symbols.contains(i.identifier)) {
                m_context->report_compiler_diagnostic(impl->loc, fmt::format("No such struct '{}' to create an implementation for", i.identifier));
            } else {
                Decl* sym = module->symbols.at(i.identifier);

                if (sym->kind != DeclKind::Struct) {
                    m_context->report_compiler_diagnostic(impl->loc, fmt::format("'{}' is not a struct", i.identifier));
                    m_context->report_compiler_diagnostic(sym->loc, "Declared here", CompilerDiagKind::Note, sym->parent_unit);
                    continue;
                }

                impl->impl.parent = sym;
                sym->struct_.impls.append(m_context, impl);
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

        for (size_t i = 0; i < unit->funcs.size(); i++) {
            Decl* func = unit->funcs[i];
            func->parent_module = module;
            func->parent_unit = unit;

            ARIA_ASSERT(func->kind == DeclKind::Function, "Invalid func in funcs");
            FunctionDecl& f = func->function;
            resolve_type(f.type);

            bool erase = false;
            resolve_decl_attributes(func, func->attributes, &erase);
            
            if (erase) {
                m_context->active_comp_unit->funcs.erase(m_context->active_comp_unit->funcs.begin() + i);
                i--;
                replace_decl(func, &error_decl);
                continue;
            }

            if (f.identifier == "main") {
                if (m_context->main_func) {
                    m_context->report_compiler_diagnostic(func->loc, "Redefining main function");
                    m_context->report_compiler_diagnostic(m_context->main_func->loc, "Previous declaration here", CompilerDiagKind::Note, m_context->main_func->parent_unit);
                    func->kind = DeclKind::Error;
                    continue;
                }

                if (f.parameters.size > 1) {
                    m_context->report_compiler_diagnostic(func->loc, "Main function must have one or zero parameters");
                }

                if (f.parameters.size >= 1) {
                    TypeInfo* type = TypeInfo::get_string(m_context);
                    if (!type_is_equal(f.parameters.items[0]->param.type, type)) {
                        m_context->report_compiler_diagnostic(f.parameters.items[0]->loc, fmt::format("First parameter of 'main' function must be of type '{}'", type_info_to_string(type)));
                    }
                }

                if (!f.type->function.return_type->is_void() && f.type->function.return_type->kind != TypeKind::Int) {
                    m_context->report_compiler_diagnostic(f.type->loc, "Return type of 'main' function must be 'void' or 'int'");
                }

                module->symbols[f.identifier] = func;
                unit->local_symbols[f.identifier] = func;
                m_context->main_func = func;
                continue;
            }

            if (module->symbols.contains(f.identifier)) {
                Decl* d = module->symbols.at(f.identifier);

                if (d->kind == DeclKind::Function) {
                    m_context->report_compiler_diagnostic(func->loc, fmt::format("Redefining function '{}'", f.identifier));
                    m_context->report_compiler_diagnostic(func->loc, "Previous declaration here", CompilerDiagKind::Note, func->parent_unit);
                } else if (d->kind == DeclKind::Var) {
                    m_context->report_compiler_diagnostic(func->loc, fmt::format("Redefining global variable '{}' as function", f.identifier));
                    m_context->report_compiler_diagnostic(func->loc, "Previous declaration here", CompilerDiagKind::Note, func->parent_unit);
                } else if (d->kind == DeclKind::Struct) {
                    m_context->report_compiler_diagnostic(func->loc, fmt::format("Redefining struct '{}' as function", f.identifier));
                    m_context->report_compiler_diagnostic(func->loc, "Previous declaration here", CompilerDiagKind::Note, func->parent_unit);
                } else {
                    ARIA_UNREACHABLE();
                }

                func->kind = DeclKind::Error;
                return;
            }

            module->symbols[f.identifier] = func;
            unit->local_symbols[f.identifier] = func;
        }
    }

    void SemanticAnalyzer::resolve_module_code(Module* module) {
        m_context->active_module = module;

        for (CompilationUnit* unit : module->units) {
            resolve_unit_code(module, unit);
        }
    }

    void SemanticAnalyzer::resolve_unit_code(Module* module, CompilationUnit* unit) {
        m_context->active_comp_unit = unit;
        resolve_stmt(unit->root_ast_node);
    }

} // namespace ariac