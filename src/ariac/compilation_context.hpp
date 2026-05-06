#pragma once

#include "ariac/allocator.hpp"
#include "ariac/compiler_flags.hpp"
#include "ariac/core/source_location.hpp"
#include "ariac/lexer/tokens.hpp"
#include "common/op_codes.hpp"
#include "ariac/reflection/compiler_reflection.hpp"

namespace Aria::Internal {

    struct Stmt;
    struct Decl;

    enum class CompilerDiagKind {
        Note,
        Warning,
        Error
    };

    struct CompilerDiagnostic {
        CompilerDiagKind kind = CompilerDiagKind::Warning;

        size_t line = 0; size_t column = 0;
        size_t start_line = 0; size_t start_column = 0;

        size_t end_line = 0; size_t end_column = 0;
        std::string message;
        std::vector<std::string> notes;
    };

    struct CompilationUnit;

    struct Module {
        std::unordered_map<std::string_view, Decl*> symbols;
        std::unordered_map<std::string_view, std::vector<Decl*>> overloaded_funcs;
        std::vector<CompilationUnit*> units;
        std::string_view name;
        OpCodes ops;
        CompilerReflectionData reflection_data;
    };

    struct CompilationUnit {
        inline CompilationUnit(const std::string& filename, const std::string& source)
            : filename(filename), source(source) {}

        size_t index = 0;
        std::string filename;

        std::string source;
        std::vector<Token> tokens;
        Stmt* root_ast_node = nullptr;

        std::vector<CompilerDiagnostic> diagnostics;

        std::vector<Decl*> globals;
        std::vector<Decl*> funcs;
        std::vector<Decl*> structs;

        std::vector<Stmt*> imports;

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

        inline void report_compiler_diagnostic(SourceLocation loc, SourceRange range, const std::string& error, CompilerDiagKind kind = CompilerDiagKind::Error) {
            CompilerDiagnostic d;
            d.kind = kind;
            d.line = loc.line;
            d.column = loc.column;
            d.start_line = range.start.line;
            d.start_column = range.start.column;
            d.end_line = range.end.line;
            d.end_column = range.end.column;
            d.message = error;
            active_comp_unit->diagnostics.push_back(d);

            if (kind == CompilerDiagKind::Error) {
                has_errors = true;
            }
        }

        inline void report_compiler_diagnostic_with_notes(SourceLocation loc, SourceRange range, const std::string& error, std::initializer_list<std::string> notes, CompilerDiagKind kind = CompilerDiagKind::Error) {
            CompilerDiagnostic d;
            d.kind = kind;
            d.line = loc.line;
            d.column = loc.column;
            d.start_line = range.start.line;
            d.start_column = range.start.column;
            d.end_line = range.end.line;
            d.end_column = range.end.column;
            d.message = error;
            d.notes = notes;
            active_comp_unit->diagnostics.push_back(d);

            if (kind == CompilerDiagKind::Error) {
                has_errors = true;
            }
        }
    
        void compile_files(const std::vector<std::string>& files, const CompilerFlags& flags);
        void compile_file(const std::string& file, const CompilerFlags& flags);
        void finish_compilation(const CompilerFlags& flags);

        void print_diag(const std::string& path, const std::string& source, Internal::CompilerDiagnostic* diag);

        void lex();
        void parse();
        void analyze();
        void emit();

        Module* find_or_create_module(std::string_view name);

        ArenaAllocator* arena = nullptr;

        std::vector<CompilationUnit*> compilation_units;
        CompilationUnit* active_comp_unit = nullptr;

        std::vector<Module*> modules;
        Decl* main_func = nullptr;

        OpCodes ops;
        CompilerReflectionData reflection_data;
        bool has_errors = false;
    };

} // namespace Aria::Internal
