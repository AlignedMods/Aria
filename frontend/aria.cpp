#include "aria/aria.hpp"

void PrintHelp(const char* appName) {
    fmt::println("Help:");
    fmt::println("  {} <file>", appName);
}

void PrintInt(Aria::Context* ctx) {
    fmt::print("{}\n", ctx->GetInt(0));
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
    ctx.AddExternalFunction("PrintInt()", PrintInt, fileName);
    fmt::print("{}", ctx.DumpAST(fileName));
    fmt::print("{}", ctx.Disassemble(fileName, false));
    ctx.Run(fileName);
    
    ctx.PushGlobal("y");
    fmt::print("result: {}", ctx.GetFloat(-1));

    ctx.FreeModule(fileName);
}
