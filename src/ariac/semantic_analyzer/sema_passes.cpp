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
            std::string ident = fmt::format("{}", var.identifier);

            module->symbols[ident] = global;
            unit->local_symbols[ident] = global;
        }

        for (Decl* func : unit->funcs) {
            ARIA_ASSERT(func->kind == DeclKind::Function, "Invalid func in funcs");

            FunctionDecl& f = func->function;

            // do not mangle the main function
            if (f.identifier == "main") { f.attributes.append(m_context, { FunctionDecl::AttributeKind::NoMangle }); }
            std::string ident = fmt::format("{}", f.identifier);

            if (module->symbols.contains(ident)) {
                Decl* d = module->symbols.at(ident);
                
                // Handle overloading the first non-overloaded function
                if (d->kind == DeclKind::Function) {
                    Decl* overloaded = Decl::Create(m_context, d->loc, d->range, DeclKind::OverloadedFunction, ErrorDecl());
                    module->symbols[ident] = overloaded;
                    unit->local_symbols[ident] = overloaded;

                    std::string oldMangle = mangle_function(&d->function);
                    std::string newMangle = mangle_function(&f);

                    if (oldMangle == newMangle) {
                        m_context->report_compiler_diagnostic(func->loc, func->range, fmt::format("Redefining function '{}'", ident));
                        func->kind = DeclKind::Error;
                        continue;
                    }

                    module->overloaded_funcs[ident].push_back(d);
                    module->overloaded_funcs[ident].push_back(func);
                    continue;
                } else if (d->kind == DeclKind::OverloadedFunction) {
                    std::string oldMangle = mangle_function(&d->function);
                    std::string newMangle = mangle_function(&f);

                    if (oldMangle == newMangle) {
                        m_context->report_compiler_diagnostic(func->loc, func->range, fmt::format("Redefining overloaded function '{}'", ident));
                        func->kind = DeclKind::Error;
                        continue;
                    }

                    module->overloaded_funcs[ident].push_back(func);
                    continue;
                } else if (d->kind == DeclKind::Var) {
                    m_context->report_compiler_diagnostic(func->loc, func->range, fmt::format("Redefining global variable '{}' as function", ident));
                }

                func->kind = DeclKind::Error;
                return;
            }

            module->symbols[ident] = func;
            unit->local_symbols[ident] = func;
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