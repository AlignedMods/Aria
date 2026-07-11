#include "ariac/build.hpp"
#include "ariac/compilation_context.hpp"

#include "llvm/TargetParser/Host.h"
#include "fmt/format.h"

#ifdef PLATFORM_WINDOWS
    #include <io.h>

    #define isatty _isatty
    #define fileno _fileno
#endif

#include <cstring>
#include <vector>

namespace ariac {

    static int arg_index = 0;
    static int arg_size = 0;
    static const char** args;
    static const char* current_arg;
    static const char* program_name;
    
    static void usage() {
        fmt::println("{}: usage: ", program_name);
        fmt::println("  {} [<options>] <files>", program_name);
        fmt::println("");
        fmt::println("  Options:");
        fmt::println("    -h --help                Print the help");
        fmt::println("    -r --run <args>          Run the compiled program after compilation with the provided <args>");
        fmt::println("    --no-codegen             Do not run any codegen");
        fmt::println("    --no-stdlib              Does not compile the standard library");
        fmt::println("    --stdlib-path <path>     Tells the compiler where the standard library is located");
        fmt::println("    --emit-ast               Prints the human readable AST of all the input files");
        fmt::println("    --emit-llvm              Prints the human readable LLVM IR output of all the input files");
        fmt::println("    -l <lib>                 Adds <lib> to the list of libraries to link with");
        fmt::println("    -L <path>                Adds <path> to the list of paths where to search for libraries");

        exit(0);
    }

    template <typename... T>
    [[noreturn]] static void error_exit(fmt::format_string<T...> fmt, T&&... args) {
        fmt::println(fmt, static_cast<T&&>(args)...);
        exit(1);
    }

    static bool at_end() { return arg_index == arg_size - 1; }

    static const char* next_arg() {
        ARIA_ASSERT(!at_end(), "Invalid next_arg() call");
        current_arg = args[++arg_index];
        return current_arg;
    }

    static bool is_next_opt() {
        ARIA_ASSERT(!at_end(), "Invalid is_next_opt() call");
        return args[arg_index + 1][0] == '-';
    }

    static bool match_arg(const char* str) {
        return strcmp(current_arg, str) == 0;
    }

    static void handle_option(BuildOptions* opts) {
        if (match_arg("-h") || match_arg("--help")) {
            usage();
        }

        if (match_arg("-r") || match_arg("--run")) {
            opts->run_after_compile = true;
            return;
        }

        if (match_arg("--no-codegen")) {
            opts->no_codegen = true;
            return;
        }

        if (match_arg("--no-stdlib")) {
            opts->no_stdlib = true;
            return;
        }

        if (match_arg("--stdlib-path")) {
            if (at_end() || is_next_opt()) {
                error_exit("Expected a path for '--stdlib-path'");
            }

            opts->stdlib_path = next_arg();
            return;
        }

        if (match_arg("--emit-ast")) {
            opts->emit_ast = true;
            return;
        }

        if (match_arg("--emit-llvm")) {
            opts->emit_llvm = true;
            return;
        }

        if (match_arg("-l")) {
            if (at_end() || is_next_opt()) {
                error_exit("Expected a path for '-l'");
            }

            opts->libs.push_back(next_arg());
            return;
        }

        if (match_arg("-L")) {
            if (at_end() || is_next_opt()) {
                error_exit("Expected a path for '-L'");
            }

            opts->libdirs.push_back(next_arg());
            return;
        }

        error_exit("Unknown option '{}'", current_arg);
    }

    static void add_file(BuildOptions* opts) {
        opts->files.push_back(current_arg);
    }

    static void add_arg(BuildOptions* opts) {
        opts->args.push_back(current_arg);
    }

    static BuildOptions handle_args(int argc, const char** argv) {
        BuildOptions opts;
        opts.triple = llvm::Triple(llvm::sys::getDefaultTargetTriple());
        opts.output_path = std::filesystem::path(".build") / "main.exe";
        opts.stdlib_path = "stdlib";

        arg_size = argc;
        args = argv;

        program_name = argv[0];

        while (!at_end()) {
            next_arg();

            if (current_arg[0] == '-') {
                handle_option(&opts);
                continue;
            }

            if (opts.run_after_compile) {
                add_arg(&opts);
                continue;
            }

            add_file(&opts);
        }

        if (opts.files.empty()) {
            error_exit("No files to compile");
        }

        return opts;
    }

    static int main_real(int argc, const char** argv) {
        BuildOptions opts = handle_args(argc, argv);

        if (!std::filesystem::exists(opts.stdlib_path)) {
            if (isatty(fileno(stdout))) {
                fmt::print(fmt::fg(fmt::color::pale_violet_red), "error: ");
                fmt::print("Could not find standard library: {}\n", opts.stdlib_path.string());
            } else {
                fmt::print(stderr, "error: Could not find standard library: {}\n", opts.stdlib_path.string());
            }
            return 1;
        }

        context.compile_files(&opts);
        return 0;
    }

} // namespace ariac

int main(int argc, const char** argv) { return ariac::main_real(argc, argv); }