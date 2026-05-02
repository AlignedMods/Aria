#include "aria/internal/compiler/semantic_analyzer/semantic_analyzer.hpp"

namespace Aria::Internal {

    void SemanticAnalyzer::pass_imports() {
        for (CompilationUnit* unit : m_Context->CompilationUnits) {
            if (!unit->Parent) { continue; }

            m_Context->ActiveCompUnit = unit;
            resolve_unit_imports(unit->Parent, unit);
        }
    }

    void SemanticAnalyzer::pass_decls() {
        for (Module* mod : m_Context->Modules) {
            resolve_module_decls(mod);
        }
    }

    void SemanticAnalyzer::pass_code() {
        for (Module* mod : m_Context->Modules) {
            resolve_module_code(mod);
        }
    }

    void SemanticAnalyzer::add_unit_to_module(Module* module, CompilationUnit* unit) {
        for (CompilationUnit* comp : module->Units) {
            if (comp == unit) {
                return;
            }
        }

        module->Units.push_back(unit);
    }

    void SemanticAnalyzer::resolve_module_imports(Module* module) {
        std::vector<CompilationUnit*> units = module->Units;

        for (CompilationUnit* unit : units) {
            resolve_unit_imports(module, unit);
        }

        m_ImportedModules.clear();
    }

    void SemanticAnalyzer::resolve_unit_imports(Module* module, CompilationUnit* unit) {
        m_ImportedModules[module->Name] = true;

        for (size_t i = 0; i < unit->Imports.size(); i++) {
            Stmt* stmt = unit->Imports[i];

            if (stmt->Kind == StmtKind::Error) { return; }
            ARIA_ASSERT(stmt->Kind == StmtKind::Import, "Invalid stmt in Imports");

            if (stmt->Import.Name == module->Name) {
                m_Context->report_compiler_diagnostic(stmt->Loc, stmt->Range, "Including self is not allowed");
                stmt->Kind = StmtKind::Error;
                return;
            }

            if (m_ImportedModules.contains(fmt::format("{}", stmt->Import.Name))) {
                m_Context->report_compiler_diagnostic(stmt->Loc, stmt->Range, "Recursive imports are not allowed");
                stmt->Kind = StmtKind::Error;
                return;
            }

            Module* resolvedModule = nullptr;

            for (size_t i = 0; i < m_Context->Modules.size(); i++) {
                Module* mod = m_Context->Modules[i];

                if (mod->Name == stmt->Import.Name) {
                    resolve_module_imports(mod);
                    resolvedModule = mod;
                    break;
                }
            }

            if (!resolvedModule) {
                m_Context->report_compiler_diagnostic(stmt->Loc, stmt->Range, fmt::format("Could not find module '{}'", stmt->Import.Name));
            }

            stmt->Import.ResolvedModule = resolvedModule;
        }

        add_unit_to_module(module, unit);
        m_ImportedModules.clear();
    }

    void SemanticAnalyzer::resolve_module_decls(Module* module) {
        for (CompilationUnit* unit : module->Units) {
            resolve_unit_decls(module, unit);
        }
    }

    void SemanticAnalyzer::resolve_unit_decls(Module* module, CompilationUnit* unit) {
        m_Context->ActiveCompUnit = unit;

        for (Decl* global : unit->Globals) {
            ARIA_ASSERT(global->Kind == DeclKind::Var, "Invalid global in globals");

            VarDecl& var = global->Var;
            std::string ident = fmt::format("{}", var.Identifier);

            module->Symbols[ident] = global;
            unit->LocalSymbols[ident] = global;
        }

        for (Decl* func : unit->Funcs) {
            ARIA_ASSERT(func->Kind == DeclKind::Function, "Invalid func in funcs");

            FunctionDecl& f = func->Function;

            // do not mangle the main function
            if (f.Identifier == "main") { f.Attributes.append(m_Context, { FunctionDecl::AttributeKind::NoMangle }); }
            std::string ident = fmt::format("{}", f.Identifier);

            if (module->Symbols.contains(ident)) {
                Decl* d = module->Symbols.at(ident);
                
                // Handle overloading the first non-overloaded function
                if (d->Kind == DeclKind::Function) {
                    Decl* overloaded = Decl::Create(m_Context, d->Loc, d->Range, DeclKind::OverloadedFunction, ErrorDecl());
                    module->Symbols[ident] = overloaded;
                    unit->LocalSymbols[ident] = overloaded;

                    std::string oldMangle = mangle_function(&d->Function);
                    std::string newMangle = mangle_function(&f);

                    if (oldMangle == newMangle) {
                        m_Context->report_compiler_diagnostic(func->Loc, func->Range, fmt::format("Redefining function '{}'", ident));
                        func->Kind = DeclKind::Error;
                        continue;
                    }

                    module->OverloadedFuncs[ident].push_back(d);
                    module->OverloadedFuncs[ident].push_back(func);
                    continue;
                } else if (d->Kind == DeclKind::OverloadedFunction) {
                    std::string oldMangle = mangle_function(&d->Function);
                    std::string newMangle = mangle_function(&f);

                    if (oldMangle == newMangle) {
                        m_Context->report_compiler_diagnostic(func->Loc, func->Range, fmt::format("Redefining overloaded function '{}'", ident));
                        func->Kind = DeclKind::Error;
                        continue;
                    }

                    module->OverloadedFuncs[ident].push_back(func);
                    continue;
                } else if (d->Kind == DeclKind::Var) {
                    m_Context->report_compiler_diagnostic(func->Loc, func->Range, fmt::format("Redefining global variable '{}' as function", ident));
                }

                func->Kind = DeclKind::Error;
                return;
            }

            module->Symbols[ident] = func;
            unit->LocalSymbols[ident] = func;
        }

        for (Decl* struc : unit->Structs) {
            ARIA_ASSERT(struc->Kind == DeclKind::Struct, "Invalid struct in structs");

            StructDecl& s = struc->Struct;
            std::string ident = fmt::format("{}", s.Identifier);

            module->Symbols[ident] = struc;
            unit->LocalSymbols[ident] = struc;
        }
    }

    void SemanticAnalyzer::resolve_module_code(Module* module) {
        for (CompilationUnit* unit : module->Units) {
            resolve_unit_code(module, unit);
        }
    }

    void SemanticAnalyzer::resolve_unit_code(Module* module, CompilationUnit* unit) {
        m_Context->ActiveCompUnit = unit;
        resolve_stmt(unit->RootASTNode);
    }

} // namespace Aria::Internal