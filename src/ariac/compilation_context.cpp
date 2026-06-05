#include "ariac/compilation_context.hpp"
#include "ariac/lexer/lexer.hpp"
#include "ariac/parser/parser.hpp"
#include "ariac/semantic_analyzer/semantic_analyzer.hpp"
#include "ariac/codegen/codegen.hpp"
#include "ariac/ast/ast_dumper.hpp"

#include <filesystem>
#include <fstream>

namespace ariac {

    static const char* stdlib_files[] = {
        "io/io.aria",
        "core/string_stream.aria",
        "core/string.aria",
        "core/mem.aria"
    };

    inline static std::string get_line(const std::string& str, size_t line) {
        std::vector<std::string> lines;
        std::stringstream ss(str);
        std::string item;

        while (std::getline(ss, item, '\n')) {
            lines.push_back(item);
        }

        return lines.at(line - 1);
    }

    void CompilationContext::compile_files(const std::vector<std::string>& files, const CompilerFlags& flags) {
        this->flags = flags;

        if (!std::filesystem::exists(".build")) {
            std::filesystem::create_directory(".build");
        }

        if (!flags.no_stdlib) { compile_stdlib(flags); }

        for (auto& file : files) { compile_file(file, flags, false); }
        finish_compilation(flags);

        if (flags.dump_ast) {
            if (flags.ast_dump_output.empty()) {
                for (CompilationUnit* unit : compilation_units) {
                    if (!unit->is_stdlib) {
                        ASTDumper d(unit->root_ast_node);
                        fmt::println("'{}'\n\n{}", unit->filename, d.get_output());
                    }
                }
            } else {
                std::ofstream out(flags.ast_dump_output);

                if (!out) {
                    fmt::print(stderr, "Failed to open AST output file '{}'\n", flags.ast_dump_output);
                    return;
                }

                for (CompilationUnit* unit : compilation_units) {
                    if (!unit->is_stdlib) {
                        ASTDumper d(unit->root_ast_node);
                        out << fmt::format("'{}'\n\n{}", unit->filename, d.get_output());
                    }
                }
            }
        }
    }

    void CompilationContext::compile_file(const std::string& file, const CompilerFlags& flags, bool std) {
        std::ifstream f(file);
        if (!f) {
            fmt::println("Could not open file '{}'", file);
            return;
        }
        std::stringstream ss;
        ss << f.rdbuf();
        std::string source = ss.str();
        ss.clear();

        CompilationUnit* unit = new CompilationUnit(file, source, std);

        if (compilation_units.size() > 0) { unit->index = compilation_units[compilation_units.size() - 1]->index + 1; }

        compilation_units.push_back(unit);
        active_comp_unit = unit;

        lex();
        parse();
    }

    void CompilationContext::compile_stdlib(const CompilerFlags& flags) {
        if (!std::filesystem::exists("stdlib/")) {
            fmt::print(stderr, "Could not find standard library");
            return;
        }

        for (const char* file : stdlib_files) {
            compile_file(fmt::format("stdlib/{}", file), flags, true);
        }
    }

    void CompilationContext::finish_compilation(const CompilerFlags& flags) {
        analyze();
        if (!has_errors && !flags.no_codegen) {
            codegen();
        }

        for (auto& diag : diagnostics) {
            print_diag(&diag);
        }
    }

    void CompilationContext::print_diag(CompilerDiagnostic* diag) {
        if (diag->line && diag->column) {
            fmt::print(fg(fmt::color::gray), "{}:{}:{}: ", diag->unit->filename, diag->line, diag->column);
        }

        if (diag->kind == CompilerDiagKind::Error) {
            fmt::print(fg(fmt::color::pale_violet_red), "error: ");
        } else if (diag->kind == CompilerDiagKind::Warning) {
            fmt::print(fg(fmt::color::yellow), "warning: ");
        } else if (diag->kind == CompilerDiagKind::Note) {
            fmt::print(fg(fmt::color::light_blue), "note: ");
        }

        fmt::print("{}\n", diag->message);

        if (diag->line && diag->column) {
            // fmt format strings from: https://hackingcpp.com/cpp/libs/fmt
            fmt::print(" {:6} | {}\n", diag->line, get_line(diag->unit->source, diag->line));
            fmt::print("        | {:>{w}}\n", "^", fmt::arg("w", diag->column));
        }

        for (auto& note : diag->notes) {
            if (diag->line && diag->column) {
                fmt::print(fg(fmt::color::gray), "{}:{}:{}: ", diag->unit->filename, diag->line, diag->column);
            }
            fmt::print(fg(fmt::color::light_blue), "note: ");
            fmt::print("{}\n", note);
        }
    }

    void CompilationContext::lex() { Lexer l(this); }
    void CompilationContext::parse() { Parser p(this); }
    void CompilationContext::analyze() { SemanticAnalyzer s(this); }
    void CompilationContext::codegen() { Codegen c(this); }

    Module* CompilationContext::find_or_create_module(std::string_view name) {
        for (Module* mod : modules) {
            if (mod->name == name) {
                return mod;
            }
        }

        Module* mod = new Module();
        mod->name = name;
        modules.push_back(mod);
        return mod;
    }

} // namespace ariac
