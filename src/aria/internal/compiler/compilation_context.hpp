#pragma once

#include "aria/internal/allocator.hpp"
#include "aria/internal/compiler/core/source_location.hpp"
#include "aria/internal/compiler/lexer/tokens.hpp"
#include "aria/internal/vm/op_codes.hpp"
#include "aria/internal/compiler/reflection/compiler_reflection.hpp"

namespace Aria::Internal {

    struct Stmt;
    struct Decl;

    struct CompilerError {
        size_t Line = 0; size_t Column = 0;
        size_t StartLine = 0; size_t StartColumn = 0;

        size_t EndLine = 0; size_t EndColumn = 0;
        std::string Error;
    };

    struct CompilationUnit;

    struct Module {
        std::unordered_map<std::string, Decl*> Symbols;
        std::vector<CompilationUnit*> Units;
        std::string Name;
        std::vector<OpCode> OpCodes;
        CompilerReflectionData ReflectionData;
    };

    struct CompilationUnit {
        inline CompilationUnit(const std::string& source)
            : Source(source) {}

        size_t Index = 0;

        std::string Source;
        std::vector<Token> Tokens;
        Stmt* RootASTNode = nullptr;

        std::vector<CompilerError> Errors;

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
        inline T* Allocate() {
            return Arena->AllocateNamed<T>();
        }

        template <typename T, typename... Args>
        inline T* Allocate(Args&&... args) {
            return Arena->AllocateNamed<T>(std::forward<Args>(args)...);
        }

        inline void* AllocateSized(size_t size) {
            return Arena->Allocate(size);
        }

        inline void ReportCompilerError(SourceLocation loc, SourceRange range, const std::string& error) {
            CompilerError e;
            e.Line = loc.Line;
            e.Column = loc.Column;
            e.StartLine = range.Start.Line;
            e.StartColumn = range.Start.Column;
            e.EndLine = range.End.Line;
            e.EndColumn = range.End.Column;
            e.Error = error;
            ActiveCompUnit->Errors.push_back(e);

            HasErrors = true;
        }
    
        void CompileFile(const std::string& source);
        void FinishCompilation();

        void Lex();
        void Parse();
        void Analyze();
        void Emit();
        void Link();

        Module* FindOrCreateModule(const std::string& name);

        ArenaAllocator* Arena = nullptr;

        std::vector<CompilationUnit*> CompilationUnits;
        CompilationUnit* ActiveCompUnit = nullptr;

        std::vector<Module*> Modules;

        std::vector<OpCode> OpCodes;
        CompilerReflectionData ReflectionData;
        bool HasErrors = false;
    };

} // namespace Aria::Internal
