#include "ariac/compiler_flags.hpp"
#include "ariac/compilation_context.hpp"

#include "fmt/format.h"

#include <cstring>
#include <vector>

namespace ariac {
    
    static void print_help(const char* prog_name) {
        fmt::println("{}: help: ", prog_name);
        fmt::println("  {} [<options>] [<args>]", prog_name);
        fmt::println("");
        fmt::println("  Options:");
        fmt::println("    -no-codegen              Do not run any codegen");
        fmt::println("    -no-stdlib               Does not compile the standard library");
        fmt::println("    -stdlib-path <path>      Tells the compiler where the standard library is located");
        fmt::println("    -dump-ast                Prints the human readable AST of all the input files");
        fmt::println("    -dump-ast-to-file <file> Dumps the human readable AST of all the input files to a file");
        fmt::println("    -dump-ir                 Prints the human readable LLVM IR output of all the input files");
        fmt::println("    -l <lib>                 Adds <lib> to the list of libraries to link with");
        fmt::println("    -L <path>                Adds <path> to the list of paths where to search for libraries");
    }

    static int main_real(int argc, char** argv) {
        if (argc <= 1) {
            print_help(argv[0]);
            return 1;
        } else if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            print_help(argv[0]);
            return 0;
        }

        CompilerFlags flags;
        std::vector<std::string> files;

        bool needs_stdlib_path = false;
        bool needs_ast_dump_output = false;
        bool needs_lib = false;
        bool needs_lib_path = false;

        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "-no-codegen") == 0) { flags.no_codegen = true; }
            else if (strcmp(argv[i], "-dump-ast") == 0) { flags.dump_ast = true; }
            else if (strcmp(argv[i], "-dump-ir") == 0) { flags.dump_ir = true; }
            else if (strcmp(argv[i], "-dump-ast-to-file") == 0) { flags.dump_ast = true; needs_ast_dump_output = true; }
            else if (strcmp(argv[i], "-no-stdlib") == 0) { flags.no_stdlib = true; }
            else if (strcmp(argv[i], "-stdlib-path") == 0) { needs_stdlib_path = true; }
            else if (strcmp(argv[i], "-l") == 0) { needs_lib = true; }
            else if (strcmp(argv[i], "-L") == 0) { needs_lib_path = true; }
            else {
                if (needs_stdlib_path) {
                    flags.stdlib_path = argv[i];
                    needs_stdlib_path = false;
                } else if (needs_ast_dump_output) {
                    flags.ast_dump_output = argv[i];
                    needs_ast_dump_output = false;
                } else if (needs_lib) {
                    flags.libs.push_back(argv[i]);
                    needs_lib = false;
                } else if (needs_lib_path) {
                    flags.libdirs.push_back(argv[i]);
                    needs_lib_path = false;
                } else {    
                    files.push_back(argv[i]);
                }
            }
        }

        if (needs_stdlib_path) {
            fmt::println("No path specified for '-stdlib-path'");
            return 1;
        }

        if (needs_ast_dump_output) {
            fmt::println("No output file specified for '-dump-ast-to-file'");
            return 1;
        }

        if (needs_lib) {
            fmt::println("No library specified for '-l'");
            return 1;
        }

        if (needs_lib_path) {
            fmt::println("No directory specified for '-L'");
            return 1;
        }

        if (files.size() == 0) {
            fmt::println("No files to compile.");
            return 1;
        }

        if (flags.stdlib_path.empty()) {
            flags.stdlib_path = "stdlib";
        }

        CompilationContext ctx;
        ctx.compile_files(files, flags);
        return 0;
    }

} // namespace ariac

int main(int argc, char** argv) { return ariac::main_real(argc, argv); }