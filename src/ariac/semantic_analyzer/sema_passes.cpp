#include "ariac/semantic_analyzer/semantic_analyzer.hpp"

namespace Aria::Internal {

    void SemanticAnalyzer::pass_imports() {
        for (CompilationUnit* unit : m_context->compilation_units) {
            if (!unit->parent) { continue; }

            m_context->active_comp_unit = unit;
            resolve_unit_imports(unit->parent, unit);
        }
    }

    void SemanticAnalyzer::pass_decls() {
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

    void SemanticAnalyzer::resolve_module_imports(Module* module) {
        std::vector<CompilationUnit*> units = module->units;

        for (CompilationUnit* unit : units) {
            resolve_unit_imports(module, unit);
        }
    }

    void SemanticAnalyzer::resolve_unit_imports(Module* module, CompilationUnit* unit) {
        for (size_t i = 0; i < unit->imports.size(); i++) {
            Stmt* stmt = unit->imports[i];

            if (stmt->kind == StmtKind::Error) { return; }
            ARIA_ASSERT(stmt->kind == StmtKind::Import, "Invalid stmt in Imports");

            if (stmt->import.name == module->name) {
                m_context->report_compiler_diagnostic(stmt->loc, stmt->range, "Including self is not allowed");
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
                m_context->report_compiler_diagnostic(stmt->loc, stmt->range, fmt::format("Could not find module '{}'", stmt->import.name));
            }

            stmt->import.resolved_module = resolvedModule;
        }

        add_unit_to_module(module, unit);
    }

    void SemanticAnalyzer::resolve_module_decls(Module* module) {
        for (CompilationUnit* unit : module->units) {
            resolve_unit_decls(module, unit);
        }
    }

    void SemanticAnalyzer::resolve_unit_decls(Module* module, CompilationUnit* unit) {
        m_context->active_comp_unit = unit;

        for (Decl* global : unit->globals) {
            ARIA_ASSERT(global->kind == DeclKind::Var, "Invalid global in globals");

            VarDecl& var = global->var;
            module->symbols[var.identifier] = global;
            unit->local_symbols[var.identifier] = global;
        }

        for (Decl* func : unit->funcs) {
            ARIA_ASSERT(func->kind == DeclKind::Function, "Invalid func in funcs");
            FunctionDecl& f = func->function;

            if (f.identifier == "main") {
                if (m_context->main_func) {
                    m_context->report_compiler_diagnostic(func->loc, func->range, "Redefining main function");
                    m_context->report_compiler_diagnostic(m_context->main_func->loc, m_context->main_func->range, "Previous declaration here", CompilerDiagKind::Note);
                    func->kind = DeclKind::Error;
                    continue;
                }

                if (f.parameters.size != 0) {
                    m_context->report_compiler_diagnostic(func->loc, func->range, "Main function must't have any parameters");
                }

                module->symbols[f.identifier] = func;
                unit->local_symbols[f.identifier] = func;
                m_context->main_func = func;
                continue;
            }

            if (module->symbols.contains(f.identifier)) {
                Decl* d = module->symbols.at(f.identifier);
                
                // Handle overloading the first non-overloaded function
                if (d->kind == DeclKind::Function) {
                    Decl* overloaded = Decl::Create(m_context, d->loc, d->range, DeclKind::OverloadedFunction, ErrorDecl());
                    module->symbols[f.identifier] = overloaded;
                    unit->local_symbols[f.identifier] = overloaded;

                    std::string oldMangle = mangle_function(&d->function);
                    std::string newMangle = mangle_function(&f);

                    if (oldMangle == newMangle) {
                        m_context->report_compiler_diagnostic(func->loc, func->range, fmt::format("Redefining function '{}'", f.identifier));
                        func->kind = DeclKind::Error;
                        continue;
                    }

                    module->overloaded_funcs[f.identifier].push_back(d);
                    module->overloaded_funcs[f.identifier].push_back(func);
                    continue;
                } else if (d->kind == DeclKind::OverloadedFunction) {
                    std::string oldMangle = mangle_function(&d->function);
                    std::string newMangle = mangle_function(&f);

                    if (oldMangle == newMangle) {
                        m_context->report_compiler_diagnostic(func->loc, func->range, fmt::format("Redefining overloaded function '{}'", f.identifier));
                        func->kind = DeclKind::Error;
                        continue;
                    }

                    module->overloaded_funcs[f.identifier].push_back(func);
                    continue;
                } else if (d->kind == DeclKind::Var) {
                    m_context->report_compiler_diagnostic(func->loc, func->range, fmt::format("Redefining global variable '{}' as function", f.identifier));
                }

                func->kind = DeclKind::Error;
                return;
            }

            module->symbols[f.identifier] = func;
            unit->local_symbols[f.identifier] = func;
        }

        for (Decl* struc : unit->structs) {
            ARIA_ASSERT(struc->kind == DeclKind::Struct, "Invalid struct in structs");

            StructDecl& s = struc->struct_;
            std::string ident = fmt::format("{}", s.identifier);

            module->symbols[ident] = struc;
            unit->local_symbols[ident] = struc;
        }
    }

    void SemanticAnalyzer::resolve_module_code(Module* module) {
        for (CompilationUnit* unit : module->units) {
            resolve_unit_code(module, unit);
        }
    }

    void SemanticAnalyzer::resolve_unit_code(Module* module, CompilationUnit* unit) {
        m_context->active_comp_unit = unit;
        resolve_stmt(unit->root_ast_node);
    }

} // namespace Aria::Internal