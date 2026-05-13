#include "ariac/compiler_flags.hpp"
#include "ariac/compilation_context.hpp"

#include "fmt/format.h"

#include <cstring>
#include <vector>

namespace Aria::Internal {
    
    static void print_help(const char* prog_name) {
        fmt::println("{}: help: ", prog_name);
        fmt::println("  {} <flags> <files>", prog_name);
        fmt::println("  flags:");
        fmt::println("    -no-stdlib              Does not compile the standard library");
        fmt::println("    -dump-ast               Prints the human readable AST of all the input files");
        fmt::println("    -dump-ast-to-file       Dumps the human readable AST of all the input files to a file");
        fmt::println("    -dump-bytecode          Prints the human readable bytecode that has been generated");
        fmt::println("    -dump-bytecode-to-file  Dumps the human readable bytecode that has been generated to a file");
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
        bool needs_bytecode_dump_output = false;

        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "-dump-ast") == 0) { flags.dump_ast = true; }
            else if (strcmp(argv[i], "-dump-bytecode") == 0) { flags.dump_bytecode = true; }
            else if (strcmp(argv[i], "-dump-ast-to-file") == 0) { flags.dump_ast = true; needs_ast_dump_output = true; }
            else if (strcmp(argv[i], "-dump-bytecode-to-file") == 0) { flags.dump_bytecode = true; needs_bytecode_dump_output = true; }
            else if (strcmp(argv[i], "-no-stdlib") == 0) { flags.no_stdlib = true; }
            else {
                if (needs_ast_dump_output) {
                    flags.ast_dump_output = argv[i];
                    needs_ast_dump_output = false;
                } else if (needs_bytecode_dump_output) {
                    flags.bytecode_dump_output = argv[i];
                    needs_bytecode_dump_output = false;
                } else {
                    files.push_back(argv[i]);
                }
            }
        }

        if (needs_ast_dump_output) {
            fmt::println("No output file specified for '-dump-ast-to-file'");
            return 1;
        }

        if (needs_bytecode_dump_output) {
            fmt::println("No output file specified for '-dump-bytecode-to-file'");
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