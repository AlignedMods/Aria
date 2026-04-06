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
    bool dumpAST = false;
    bool dumpByteCode = false;
    std::vector<std::string> files;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0) { compileOnly = true; }
        else if (strcmp(argv[i], "-dump-ast") == 0) { dumpAST = true; }
        else if (strcmp(argv[i], "-dump-bytecode") == 0) { dumpByteCode = true; }
        else { files.push_back(argv[i]); }
    }
    
    if (files.size() == 0) {
        fmt::print("No files to compile.");
        return 1;
    }

    Aria::Context ctx;
    ctx.CompileFiles(files, files.front());

    if (dumpAST) { fmt::print("{}", ctx.DumpAST()); }
    if (dumpByteCode) { fmt::print("{}", ctx.Disassemble()); }

    if (!compileOnly) {
        ctx.AddExternalFunction("print_int()", PrintInt);
        ctx.AddExternalFunction("print()", Print);
        ctx.Run();

        if (ctx.HasFunction("main()")) {
            ctx.Call("main()", 0);
        }
    }
    
    ctx.FreeModule(files.front());
}
