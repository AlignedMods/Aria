#include "ariac/compilation_context.hpp"
#include "ariac/core/platform.hpp"
#include "ariac/lexer/lexer.hpp"
#include "ariac/parser/parser.hpp"
#include "ariac/semantic_analyzer/semantic_analyzer.hpp"
#include "ariac/codegen/codegen.hpp"
#include "ariac/ast/ast_dumper.hpp"

#include <filesystem>
#include <fstream>

#ifdef PLATFORM_WINDOWS
    #include <io.h>

    #define isatty _isatty
    #define fileno _fileno
#endif

namespace ariac {

    CompilationContext context;

    static const char* stdlib_files[] = {
        "core/types.aria",
        "libc.aria",
        "io/io.aria",
        "io/file.aria",
        "core/string_stream.aria",
        "core/mem.aria",
        "core/list.aria",
        "core/any.aria",
        "core/assert.aria",
        "process/process.aria",
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

    void CompilationContext::compile_files(BuildOptions* opts) {
        this->opts = opts;

        if (!std::filesystem::exists(".build")) {
            std::filesystem::create_directory(".build");
        }

        if (!opts->no_stdlib) { compile_stdlib(); }

        for (auto& file : opts->files) { compile_file(file, false); }
        finish_compilation();
    }

    void CompilationContext::compile_file(const std::string& file, bool std) {
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

        compilation_units.push_back(unit);
        active_comp_unit = unit;

        lex();
        parse();
    }

    void CompilationContext::compile_stdlib() {
        for (const char* file : stdlib_files) {
            compile_file((opts->stdlib_path / file).string(), true);
        }
    }

    void CompilationContext::finish_compilation() {
        analyze();

        if (opts->emit_ast) {
            for (Module* mod : modules) {
                ASTDumper d(mod);
                std::string& output = d.get_output();
                if (!output.empty()) { fmt::println("{}", d.get_output()); }
            }
        }

        for (auto& diag : diagnostics) {
            print_diag(&diag);
        }

        if (!has_errors && !opts->no_codegen) {
            codegen();

            if (opts->run_after_compile) {
                fmt::println("Running executable '{}'\n", opts->output_path.string());

                std::vector<llvm::StringRef> args;
                std::string out = opts->output_path.string();
                args.push_back(out);
                for (auto& arg : opts->args) { args.push_back(arg); }
                std::string err;
                int code = llvm::sys::ExecuteAndWait(opts->output_path.string(), args, {}, {}, 0, 0, &err);

                if (code == -1) {
                    fmt::println("Could not run executable after compilation: {}", err);
                } else if (code == -2) {
                    fmt::println("Failed to run executable after compilation: {}", err);
                } else {
                    fmt::println("Program finished with exit code: {}", code);
                }
            }
        }

    }

    void CompilationContext::print_diag(CompilerDiagnostic* diag) {
        bool is_tty = isatty(fileno(stdout));
        if (diag->loc.line && diag->loc.col) {
            if (is_tty) {
                fmt::print(fg(fmt::color::gray), "{}:{}:{}: ", diag->unit->filename, diag->loc.line, diag->loc.col);
            } else {
                fmt::print(stderr, "{}:{}:{}: ", diag->unit->filename, diag->loc.line, diag->loc.col);
            }
        }

        if (diag->kind == CompilerDiagKind::Error) {
            if (is_tty) {
                fmt::print(fg(fmt::color::pale_violet_red), "error: ");
            } else {
                fmt::print(stderr, "error: ");
            }
        } else if (diag->kind == CompilerDiagKind::Warning) {
            if (is_tty) {
                fmt::print(fg(fmt::color::yellow), "warning: ");
            } else {
                fmt::print(stderr, "warning: ");
            }
        } else if (diag->kind == CompilerDiagKind::Note) {
            if (is_tty) {
                fmt::print(fg(fmt::color::light_blue), "note: ");
            } else {
                fmt::print(stderr, "note: ");
            }
        }

        if (is_tty) {
            fmt::print("{}\n", diag->message);
        } else {
            fmt::print(stderr, "{}\n", diag->message);
        }

        if (diag->loc.line && diag->loc.col) {
            // fmt format strings from: https://hackingcpp.com/cpp/libs/fmt
            if (is_tty) {
                fmt::print(" {:6} | {}\n", diag->loc.line, get_line(diag->unit->source, diag->loc.line));
                fmt::print("        | {:>{w}}\n", std::string(diag->loc.len, '^'), fmt::arg("w", diag->loc.col + diag->loc.len - 1));
            } else {
                fmt::print(stderr, " {:6} | {}\n", diag->loc.line, get_line(diag->unit->source, diag->loc.line));
                fmt::print(stderr, "        | {:>{w}}\n", std::string(diag->loc.len, '^'), fmt::arg("w", diag->loc.col + diag->loc.len - 1));
            }
        }

        for (auto& note : diag->notes) {
            if (diag->loc.line && diag->loc.col) {
                if (is_tty) {
                    fmt::print(fg(fmt::color::gray), "{}:{}:{}: ", diag->unit->filename, diag->loc.line, diag->loc.col);
                } else {
                    fmt::print(stderr, "{}:{}:{}: ", diag->unit->filename, diag->loc.line, diag->loc.col);
                }
            }

            if (is_tty) {
                fmt::print(fg(fmt::color::light_blue), "note: ");
                fmt::print("{}\n\n", note);
            } else {
                fmt::print(stderr, "note: {}\n\n", note);
            }
        }
    }

    void CompilationContext::lex() { Lexer l; }
    void CompilationContext::parse() { Parser p; }
    void CompilationContext::analyze() { SemanticAnalyzer s; }
    void CompilationContext::codegen() { Codegen c; }

    Module* CompilationContext::find_or_create_module(std::string_view name) {
        for (Module* mod : modules) {
            if (mod->name == name) {
                return mod;
            }
        }

        Module* mod = new Module();
        mod->name = name;
        modules.push_back(mod);

        if (name == "std::core") { std_core_module = mod; }

        return mod;
    }

} // namespace ariac
