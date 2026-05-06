#include "ariac/compilation_context.hpp"
#include "ariac/lexer/lexer.hpp"
#include "ariac/parser/parser.hpp"
#include "ariac/semantic_analyzer/semantic_analyzer.hpp"
#include "ariac/emitter/emitter.hpp"
#include "ariac/ast/ast_dumper.hpp"
#include "ariac/byte_code/disassembler.hpp"
#include "ariac/byte_code/serializer.hpp"

#include <filesystem>
#include <fstream>

namespace Aria::Internal {

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
        for (auto& file : files) {
            compile_file(file, flags);
        }

        finish_compilation(flags);

        if (flags.dump_ast) {
            for (CompilationUnit* unit : compilation_units) {
                ASTDumper d(unit->root_ast_node);
                fmt::println("'{}'\n\n{}", unit->filename, d.get_output());
            }
        }

        if (flags.dump_bytecode) {
            Disassembler d(ops);
            fmt::println("{}", d.get_dissasembly());
        }
    }

    void CompilationContext::compile_file(const std::string& file, const CompilerFlags& flags) {
        std::ifstream f(file);
        if (!f) {
            fmt::println("Could not open file '{}'", file);
            return;
        }
        std::stringstream ss;
        ss << f.rdbuf();
        std::string source = ss.str();
        ss.clear();

        CompilationUnit* unit = new CompilationUnit(file, source);

        if (compilation_units.size() > 0) { unit->index = compilation_units[compilation_units.size() - 1]->index + 1; }

        compilation_units.push_back(unit);
        active_comp_unit = unit;

        lex();
        parse();
    }

    void CompilationContext::finish_compilation(const CompilerFlags& flags) {
        analyze();
        if (!has_errors) {
            emit();
        } else {
            for (CompilationUnit* unit : compilation_units) {
                for (auto& diag : unit->diagnostics) {
                    print_diag(unit->filename, unit->source, &diag);
                }
            }
        }
    }

    void CompilationContext::print_diag(const std::string& path, const std::string& source, Internal::CompilerDiagnostic* diag) {
        if (diag->line && diag->column) {
            fmt::print(fg(fmt::color::gray), "{}:{}:{}: ", path, diag->line, diag->column);
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
            fmt::print(" {:6} | {}\n", diag->line, get_line(source, diag->line));
            fmt::print("        | {:>{w}}\n", "^", fmt::arg("w", diag->column));
        }

        for (auto& note : diag->notes) {
            if (diag->line && diag->column) {
                fmt::print(fg(fmt::color::gray), "{}:{}:{}: ", path, diag->line, diag->column);
            }
            fmt::print(fg(fmt::color::light_blue), "note: ");
            fmt::print("{}\n", note);
        }
    }

    void CompilationContext::lex() { Lexer l(this); }
    void CompilationContext::parse() { Parser p(this); }
    void CompilationContext::analyze() { SemanticAnalyzer s(this); }
    void CompilationContext::emit() { Emitter e(this); Serialzier s(this); }

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

} // namespace Aria::Internal
