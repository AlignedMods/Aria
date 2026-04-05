#include "aria/aria.hpp"

void PrintHelp(const char* appName) {
    fmt::println("Help:");
    fmt::println("  {} <flags> <file>\n", appName);
    fmt::println("  Flags:");
    fmt::println("    -c      (Compile the file normally but do not run it)");
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

    bool compileOnly = false;
    const char* fileName = nullptr;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0) { compileOnly = true; }
        else { fileName = argv[i]; }
    }
    
    if (!fileName) {
        fmt::print("No files to compile.");
        return 1;
    }

    Aria::Context ctx;
    ctx.CompileFile(fileName, fileName);

    if (!compileOnly) {
        ctx.AddExternalFunction("PrintInt()", PrintInt);
        ctx.AddExternalFunction("Print()", Print);
        ctx.Run();

        if (ctx.HasFunction("main()")) {
            ctx.Call("main()", 0);
        }
    }
    
    ctx.FreeModule(fileName);
}
