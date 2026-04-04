#include "aria/aria.hpp"

void PrintHelp(const char* appName) {
    fmt::println("Help:");
    fmt::println("  {} <file>", appName);
}

void PrintInt(Aria::Context* ctx) {
    ctx->GetArg(0);
    fmt::print("{}\n", ctx->GetInt(-1));
    ctx->Pop(1);
}

void Print(Aria::Context* ctx) {
    ctx->GetArg(0);
    fmt::print("{}\n", ctx->GetString(-1));
    ctx->Pop(1);
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
    ctx.AddExternalFunction("PrintInt()", PrintInt);
    ctx.AddExternalFunction("Print()", Print);
    fmt::print("{}", ctx.DumpAST());
    fmt::print("{}", ctx.Disassemble());
    ctx.Run();

    ctx.GetGlobal("powered");
    fmt::print("{}\n", ctx.GetLong(-1));
    
    ctx.FreeModule(fileName);
}
