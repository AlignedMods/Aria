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

    // std::string fileName = argv[1];
    // Aria::Context ctx;
    // ctx.CompileFile(fileName, fileName);
    // ctx.AddExternalFunction("PrintInt()", PrintInt, fileName);
    // fmt::print("{}", ctx.DumpAST(fileName));
    // fmt::print("{}", ctx.Disassemble(fileName, false));
    // ctx.Run(fileName);
    // 
    // ctx.PushGlobal("a");
    // fmt::print("a = {}", ctx.GetInt(-1));
    // ctx.Pop(1);
    // 
    // ctx.FreeModule(fileName);

    Aria::Context ctx = Aria::Context::Create();
    ctx.CompileFile("tests/runtime/basic_expressions.aria", "Runtime Basic Expressions");
    ctx.AddExternalFunction("ShouldNeverBeCalled", [](Aria::Context* ctx) {
        throw std::runtime_error("function that shouldn't be called was called");
    }, "Runtime Basic Expressions");
    fmt::print("{}", ctx.DumpAST("Runtime Basic Expressions"));
    fmt::print("{}", ctx.Disassemble("Runtime Basic Expressions", false));
    ctx.Run("Runtime Basic Expressions");

}
