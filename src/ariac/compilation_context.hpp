#pragma once

#include "ariac/allocator.hpp"
#include "ariac/build.hpp"
#include "ariac/core/source_location.hpp"
#include "ariac/lexer/tokens.hpp"
#include "ariac/reflection/compiler_reflection.hpp"

namespace ariac {

    struct Expr;
    struct Stmt;
    struct Decl;
    struct CompilationUnit;

    enum class CompilerDiagKind {
        Note,
        Warning,
        Error
    };

    struct CompilerDiagnostic {
        CompilerDiagKind kind = CompilerDiagKind::Warning;

        SourceLoc loc;

        std::string message;
        std::vector<std::string> notes;

        CompilationUnit* unit = nullptr;
    };

    struct Module {
        std::unordered_map<std::string_view, Decl*> symbols;
        std::unordered_map<std::string_view, Decl*> private_symbols;
        std::vector<CompilationUnit*> units;
        std::string_view name;
        CompilerReflectionData reflection_data;
    };

    // A compilation unit is a module section, so one compilation unit does not necessarily correspond to one file
    struct CompilationUnit {
        inline CompilationUnit(const std::string& filename, const std::string& source, bool is_stdlib)
            : filename(filename), source(source), is_stdlib(is_stdlib) {}

        std::string filename;
        bool is_stdlib = false;

        std::string source;
        std::vector<Token> tokens;

        std::vector<Decl*> globals;
        std::vector<Decl*> funcs;
        std::vector<Decl*> structs;
        std::vector<Decl*> impls;
        std::vector<Decl*> typedefs;
        std::vector<Decl*> enums;
        std::vector<Decl*> generics;
        std::vector<Decl*> imports;

        Expr* if_attr = nullptr;

        std::unordered_map<std::string_view, Decl*> local_symbols;

        Module* parent = nullptr;
    };

    struct CompilationContext {
        inline CompilationContext()
            : arena(new ArenaAllocator(10 * 1024 * 1024)) {}

        inline CompilationContext(const CompilationContext& other) = delete; // Disallow copying
        inline CompilationContext(const CompilationContext&& other) = delete; // Disallow moving

        inline ~CompilationContext() {
            delete arena;

            // Free all the modules and compilation units
            for (CompilationUnit* unit : compilation_units) { delete unit; }
            for (Module* mod : modules) { delete mod; }
        }

        template <typename T>
        inline T* allocate() {
            return arena->allocate_named<T>();
        }

        template <typename T, typename... Args>
        inline T* allocate(Args&&... args) {
            return arena->allocate_named<T>(std::forward<Args>(args)...);
        }

        inline void* allocate_sized(size_t size) {
            return arena->allocate(size);
        }

        inline void report_compiler_diagnostic(SourceLoc loc, const std::string& error, CompilerDiagKind kind = CompilerDiagKind::Error, CompilationUnit* unit = nullptr) {
            CompilerDiagnostic d;
            d.kind = kind;
            d.loc = loc;
            d.message = error;
            d.unit = unit ? unit : active_comp_unit;

            diagnostics.push_back(d);

            if (kind == CompilerDiagKind::Error) {
                has_errors = true;
            }
        }

        inline void report_compiler_diagnostic_with_notes(SourceLoc loc, const std::string& error, std::initializer_list<std::string> notes, CompilerDiagKind kind = CompilerDiagKind::Error, CompilationUnit* unit = nullptr) {
            CompilerDiagnostic d;
            d.kind = kind;
            d.loc = loc;
            d.message = error;
            d.notes = notes;
            d.unit = unit ? unit : active_comp_unit;

            diagnostics.push_back(d);

            if (kind == CompilerDiagKind::Error) {
                has_errors = true;
            }
        }
    
        void compile_files(BuildOptions* opts);
        void compile_file(const std::string& file, bool std);
        void compile_stdlib();
        void finish_compilation();

        void print_diag(CompilerDiagnostic* diag);

        void lex();
        void parse();
        void analyze();
        void codegen();

        Module* find_or_create_module(std::string_view name);

    public:
        ArenaAllocator* arena = nullptr;

        std::vector<CompilationUnit*> compilation_units;
        CompilationUnit* active_comp_unit = nullptr;
        Module* active_module = nullptr;
        BuildOptions* opts = nullptr;

        std::vector<Module*> modules;
        Module* std_core_module = nullptr;
        Decl* main_func = nullptr;

        CompilerReflectionData reflection_data;
        bool has_errors = false;

        std::vector<CompilerDiagnostic> diagnostics;
    };

    extern CompilationContext context;

} // namespace ariac
