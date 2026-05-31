#include "ariac/compiler_flags.hpp"
#include "ariac/compilation_context.hpp"

#include "fmt/format.h"

#include <cstring>
#include <vector>

namespace Aria::Internal {
    
    static void print_help(const char* prog_name) {
        fmt::println("{}: help: ", prog_name);
        fmt::println("  {} [<options>] [<args>]", prog_name);
        fmt::println("");
        fmt::println("  Options:");
        fmt::println("    -no-stdlib              Does not compile the standard library");
        fmt::println("    -dump-ast               Prints the human readable AST of all the input files");
        fmt::println("    -dump-ast-to-file       Dumps the human readable AST of all the input files to a file");
        fmt::println("    -dump-ir                Prints the human readable LLVM IR output of all the input files");
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

        bool needs_ast_dump_output = false;

        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "-dump-ast") == 0) { flags.dump_ast = true; }
            else if (strcmp(argv[i], "-dump-ir") == 0) { flags.dump_ir = true; }
            else if (strcmp(argv[i], "-dump-ast-to-file") == 0) { flags.dump_ast = true; needs_ast_dump_output = true; }
            else if (strcmp(argv[i], "-no-stdlib") == 0) { flags.no_stdlib = true; }
            else {
                if (needs_ast_dump_output) {
                    flags.ast_dump_output = argv[i];
                    needs_ast_dump_output = false;
                } else {
                    files.push_back(argv[i]);
                }
            }
        }

        if (needs_ast_dump_output) {
            fmt::println("No output file specified for '-dump-ast-to-file'");
            return 1;
        }

        if (files.size() == 0) {
            fmt::println("No files to compile.");
            return 1;
        }

        CompilationContext ctx;
        ctx.compile_files(files, flags);
        return 0;
    }

} // namespace Aria::Internal

int main(int argc, char** argv) { return Aria::Internal::main_real(argc, argv); }