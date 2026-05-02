#pragma once

#include "aria/internal/allocator.hpp"
#include "aria/internal/compiler/core/source_location.hpp"
#include "aria/internal/compiler/lexer/tokens.hpp"
#include "aria/internal/vm/op_codes.hpp"
#include "aria/internal/compiler/reflection/compiler_reflection.hpp"

namespace Aria::Internal {

    struct Stmt;
    struct Decl;

    enum class CompilerDiagKind {
        Warning,
        Error
    };

    struct CompilerDiagnostic {
        CompilerDiagKind Kind = CompilerDiagKind::Warning;

        size_t Line = 0; size_t Column = 0;
        size_t StartLine = 0; size_t StartColumn = 0;

        size_t EndLine = 0; size_t EndColumn = 0;
        std::string Message;
    };

    struct CompilationUnit;

    struct Module {
        std::unordered_map<std::string, Decl*> Symbols;
        std::unordered_map<std::string, std::vector<Decl*>> OverloadedFuncs;
        std::vector<CompilationUnit*> Units;
        std::string Name;
        OpCodes Ops;
        CompilerReflectionData ReflectionData;
    };

    struct CompilationUnit {
        inline CompilationUnit(const std::string& source)
            : Source(source) {}

        size_t Index = 0;

        std::string Source;
        std::vector<Token> Tokens;
        Stmt* RootASTNode = nullptr;

        std::vector<CompilerDiagnostic> Diagnostics;

        std::vector<Decl*> Globals;
        std::vector<Decl*> Funcs;
        std::vector<Decl*> Structs;

        std::vector<Stmt*> Imports;

        std::unordered_map<std::string, Decl*> LocalSymbols;

        Module* Parent = nullptr;
    };

    struct CompilationContext {
        inline CompilationContext()
            : Arena(new ArenaAllocator(10 * 1024 * 1024)) {}

        inline CompilationContext(const CompilationContext& other) = delete; // Disallow copying
        inline CompilationContext(const CompilationContext&& other) = delete; // Disallow moving

        inline ~CompilationContext() {
            delete Arena;

            // Free all the modules and compilation units
            for (CompilationUnit* unit : CompilationUnits) { delete unit; }
            for (Module* mod : Modules) { delete mod; }
        }

        template <typename T>
        inline T* allocate() {
            return Arena->allocate_named<T>();
        }

        template <typename T, typename... Args>
        inline T* allocate(Args&&... args) {
            return Arena->allocate_named<T>(std::forward<Args>(args)...);
        }

        inline void* allocate_sized(size_t size) {
            return Arena->allocate(size);
        }

        inline void report_compiler_diagnostic(SourceLocation loc, SourceRange range, const std::string& error, CompilerDiagKind kind = CompilerDiagKind::Error) {
            CompilerDiagnostic d;
            d.Kind = kind;
            d.Line = loc.Line;
            d.Column = loc.Column;
            d.StartLine = range.Start.Line;
            d.StartColumn = range.Start.Column;
            d.EndLine = range.End.Line;
            d.EndColumn = range.End.Column;
            d.Message = error;
            ActiveCompUnit->Diagnostics.push_back(d);

            if (kind == CompilerDiagKind::Error) {
                HasErrors = true;
            }
        }
    
        void compile_file(const std::string& source);
        void finish_compilation();

        void lex();
        void parse();
        void analyze();
        void emit();

        Module* find_or_create_module(const std::string& name);

        ArenaAllocator* Arena = nullptr;

        std::vector<CompilationUnit*> CompilationUnits;
        CompilationUnit* ActiveCompUnit = nullptr;

        std::vector<Module*> Modules;

        OpCodes Ops;
        CompilerReflectionData ReflectionData;
        bool HasErrors = false;
    };

} // namespace Aria::Internal
