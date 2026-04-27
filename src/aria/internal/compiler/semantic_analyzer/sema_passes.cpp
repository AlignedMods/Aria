#include "aria/internal/compiler/semantic_analyzer/semantic_analyzer.hpp"

namespace Aria::Internal {

    void SemanticAnalyzer::PassImports() {
        for (CompilationUnit* unit : m_Context->CompilationUnits) {
            m_Context->ActiveCompUnit = unit;
            ResolveUnitImports(unit->Parent, unit);
        }
    }

    void SemanticAnalyzer::PassDecls() {
        for (Module* mod : m_Context->Modules) {
            ResolveModuleDecls(mod);
        }
    }

    void SemanticAnalyzer::PassCode() {
        for (Module* mod : m_Context->Modules) {
            ResolveModuleCode(mod);
        }
    }

    void SemanticAnalyzer::AddUnitToModule(Module* module, CompilationUnit* unit) {
        for (CompilationUnit* comp : module->Units) {
            if (comp == unit) {
                return;
            }
        }

        module->Units.push_back(unit);
    }

    void SemanticAnalyzer::ResolveModuleImports(Module* module) {
        std::vector<CompilationUnit*> units = module->Units;

        for (CompilationUnit* unit : units) {
            ResolveUnitImports(module, unit);
        }

        m_ImportedModules.clear();
    }

    void SemanticAnalyzer::ResolveUnitImports(Module* module, CompilationUnit* unit) {
        m_ImportedModules[module->Name] = true;

        for (size_t i = 0; i < unit->Imports.size(); i++) {
            Stmt* stmt = unit->Imports[i];

            if (stmt->Kind == StmtKind::Error) { return; }
            ARIA_ASSERT(stmt->Kind == StmtKind::Import, "Invalid stmt in Imports");

            if (stmt->Import.Name == module->Name) {
                m_Context->ReportCompilerDiagnostic(stmt->Loc, stmt->Range, "Including self is not allowed");
                stmt->Kind = StmtKind::Error;
                return;
            }

            if (m_ImportedModules.contains(fmt::format("{}", stmt->Import.Name))) {
                m_Context->ReportCompilerDiagnostic(stmt->Loc, stmt->Range, "Recursive imports are not allowed");
                stmt->Kind = StmtKind::Error;
                return;
            }

            Module* resolvedModule = nullptr;

            for (size_t i = 0; i < m_Context->Modules.size(); i++) {
                Module* mod = m_Context->Modules[i];

                if (mod->Name == stmt->Import.Name) {
                    ResolveModuleImports(mod);
                    resolvedModule = mod;
                    break;
                }
            }

            if (!resolvedModule) {
                m_Context->ReportCompilerDiagnostic(stmt->Loc, stmt->Range, fmt::format("Could not find module '{}'", stmt->Import.Name));
            }

            stmt->Import.ResolvedModule = resolvedModule;
        }

        AddUnitToModule(module, unit);
        m_ImportedModules.clear();
    }

    void SemanticAnalyzer::ResolveModuleDecls(Module* module) {
        for (CompilationUnit* unit : module->Units) {
            ResolveUnitDecls(module, unit);
        }
    }

    void SemanticAnalyzer::ResolveUnitDecls(Module* module, CompilationUnit* unit) {
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
            if (f.Identifier == "main") { func->Attributes.Append(m_Context, { DeclAttributeKind::NoMangle }); }
            std::string ident = fmt::format("{}", f.Identifier);

            if (module->Symbols.contains(ident)) {
                Decl* d = module->Symbols.at(ident);
                
                // Handle overloading the first non-overloaded function
                if (d->Kind == DeclKind::Function) {
                    module->Symbols[ident] = Decl::Create(m_Context, d->Loc, d->Range, DeclKind::OverloadedFunction, {}, d->Visibility, ErrorDecl());

                    std::string oldMangle = MangleFunction(&d->Function);
                    std::string newMangle = MangleFunction(&f);

                    if (oldMangle == newMangle) {
                        m_Context->ReportCompilerDiagnostic(func->Loc, func->Range, fmt::format("Redefining function '{}'", ident));
                        func->Kind = DeclKind::Error;
                        continue;
                    }

                    module->OverloadedFuncs[ident].push_back(d);
                    module->OverloadedFuncs[ident].push_back(func);
                    continue;
                } else if (d->Kind == DeclKind::OverloadedFunction) {
                    std::string oldMangle = MangleFunction(&d->Function);
                    std::string newMangle = MangleFunction(&f);

                    if (oldMangle == newMangle) {
                        m_Context->ReportCompilerDiagnostic(func->Loc, func->Range, fmt::format("Redefining overloaded function '{}'", ident));
                        func->Kind = DeclKind::Error;
                        continue;
                    }

                    module->OverloadedFuncs[ident].push_back(func);
                    continue;
                } else if (d->Kind == DeclKind::Var) {
                    m_Context->ReportCompilerDiagnostic(func->Loc, func->Range, fmt::format("Redefining global variable '{}' as function", ident));
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

    void SemanticAnalyzer::ResolveModuleCode(Module* module) {
        for (CompilationUnit* unit : module->Units) {
            ResolveUnitCode(module, unit);
        }
    }

    void SemanticAnalyzer::ResolveUnitCode(Module* module, CompilationUnit* unit) {
        m_Context->ActiveCompUnit = unit;
        ResolveStmt(unit->RootASTNode);
    }

} // namespace Aria::Internal