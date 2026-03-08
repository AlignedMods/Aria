#include "aria/aria.hpp"

void PrintHelp(const char* appName) {
    fmt::println("Help:");
    fmt::println("  {} <file>", appName);
}

int main(int argc, char** argv) {
    if (argc == 1) {
        PrintHelp(argv[0]);
        return 1;
    } else if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        PrintHelp(argv[0]);
        return 0;
    }

    std::string fileName = argv[1];
    Aria::Context ctx;
    ctx.CompileFile(fileName, fileName);
    fmt::print("{}", ctx.DumpAST(fileName));
    fmt::print("{}", ctx.Disassemble(fileName));
    ctx.Run(fileName);
    ctx.FreeModule(fileName);
}
