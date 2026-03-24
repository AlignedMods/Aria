#include "aria/context.hpp"
#include "aria/core.hpp"

#include "catch2.hpp"

TEST_CASE("Runtime Variable Declaration") {
    Aria::Context ctx = Aria::Context::Create();
    ctx.CompileFile("tests/runtime/variable_declaration.aria", "Runtime Variable Declaration");
    ctx.Run();
    
    ctx.GetGlobal("f");
    REQUIRE(ctx.GetBool(-1) == false);
    ctx.GetGlobal("t");
    REQUIRE(ctx.GetBool(-1) == true);
    ctx.GetGlobal("i");
    REQUIRE(ctx.GetInt(-1) == 99);

    ctx.Pop(3);
}

TEST_CASE("Runtime Basic Expressions") {
    Aria::Context ctx = Aria::Context::Create();
    ctx.CompileFile("tests/runtime/basic_expressions.aria", "Runtime Basic Expressions");
    ctx.AddExternalFunction("ShouldNeverBeCalled()", [](Aria::Context* ctx) {
        throw std::runtime_error("function that shouldn't be called was called");
    });
    ctx.Run();
    
    ctx.GetGlobal("a");
    REQUIRE(ctx.GetInt(-1) == 10);
    ctx.GetGlobal("b");
    REQUIRE(ctx.GetInt(-1) == -3);
    ctx.GetGlobal("c");
    REQUIRE(ctx.GetInt(-1) == 10);

    ctx.GetGlobal("d");
    REQUIRE(ctx.GetBool(-1) == false);
    ctx.GetGlobal("e");
    REQUIRE(ctx.GetBool(-1) == false);
    ctx.GetGlobal("f");
    REQUIRE(ctx.GetBool(-1) == true);
    ctx.GetGlobal("g");
    REQUIRE(ctx.GetBool(-1) == false);

    ctx.GetGlobal("h");
    REQUIRE(ctx.GetBool(-1) == true);
    ctx.GetGlobal("i");
    REQUIRE(ctx.GetBool(-1) == true);
    ctx.GetGlobal("j");
    REQUIRE(ctx.GetBool(-1) == false);
    ctx.GetGlobal("k");
    REQUIRE(ctx.GetBool(-1) == true);
}

TEST_CASE("Runtime Functions") {
    Aria::Context ctx = Aria::Context::Create();
    ctx.CompileFile("tests/runtime/functions.aria", "Runtime Functions");
    ctx.AddExternalFunction("ExternalFunction()", [](Aria::Context* ctx) {
        ctx->GetArg(0);
        ctx->GetArg(1);
        ctx->GetArg(2);
        REQUIRE(ctx->GetInt(-3) == 5);
        REQUIRE(ctx->GetInt(-2) == 66);
        REQUIRE(ctx->GetInt(-1) == 50);
        ctx->Pop(3);
    });
    ctx.Run();

    ctx.Call("main()", 0);
    
    ctx.GetGlobal("result");
    REQUIRE(ctx.GetInt(-1) == 24);
    ctx.GetGlobal("otherResult");
    REQUIRE(ctx.GetInt(-1) == 26);
    ctx.Pop(2);
}

TEST_CASE("Runtime Control Flow") {
    Aria::Context ctx = Aria::Context::Create();
    ctx.CompileFile("tests/runtime/control_flow.aria", "Runtime Control Flow");
    ctx.Run();
    
    ctx.PushInt(0);
    ctx.Call("While()", 0);
    REQUIRE(ctx.GetInt(-1) == 10);
    ctx.Pop(1);
    
    ctx.PushInt(0);
    ctx.Call("DoWhile1()", 0);
    REQUIRE(ctx.GetInt(-1) == 10);
    ctx.Pop(1);
    
    ctx.PushBool(false);
    ctx.Call("DoWhile2()", 0);
    REQUIRE(ctx.GetBool(-1) == true);
    ctx.Pop(1);
    
    ctx.PushBool(false);
    ctx.Call("If()", 0);
    REQUIRE(ctx.GetBool(-1) == false);
    ctx.Pop(1);
}

TEST_CASE("Runtime Recursion") {
    Aria::Context ctx = Aria::Context::Create();
    ctx.CompileFile("tests/runtime/recursion.aria", "Runtime Recursion");
    ctx.Run();
    
    ctx.PushInt(10);
    ctx.Call("Fib()", 1);
    REQUIRE(ctx.GetInt(-1) == 55);
    ctx.Pop(1);
    
    ctx.PushInt(20);
    ctx.Call("Fib()", 1);
    REQUIRE(ctx.GetInt(-1) == 6765);
    ctx.Pop(1);
}

TEST_CASE("Runtime Casts") {
    Aria::Context ctx = Aria::Context::Create();
    ctx.CompileFile("tests/runtime/casts.aria", "Runtime Casts");
    ctx.Run();

    ctx.GetGlobal("a");
    REQUIRE(ctx.GetInt(-1) == 2);
    ctx.GetGlobal("b");
    REQUIRE((ctx.GetFloat(-1) > 1.9999f && ctx.GetFloat(-1) < 2.0001f)); // Due to floating point inaccuracies we don't require exact matching numbers
    ctx.GetGlobal("c");
    REQUIRE(ctx.GetLong(-1) == static_cast<int64_t>(5));
    ctx.GetGlobal("d");
    REQUIRE((ctx.GetDouble(-1) > 9.9999999 && ctx.GetDouble(-1) < 10.000000001));

    ctx.Pop(4);
}

TEST_CASE("Runtime Structs") {
    Aria::Context ctx = Aria::Context::Create();
    ctx.CompileFile("tests/runtime/structs.aria", "Runtime Structs");
    ctx.Run();
    
    ctx.GetGlobalPtr("p");
    ctx.Call("Player::GetX()", 1);
    REQUIRE(ctx.GetFloat(-1) == 5.0f);
    ctx.Pop(1); // Pop the return value

    ctx.GetGlobalPtr("p");
    ctx.Call("Player::GetY()", 1);
    REQUIRE(ctx.GetFloat(-1) == 4.0f);
    ctx.Pop(1);
}

TEST_CASE("Runtime Arrays") {
    // Aria::Context ctx = Aria::Context::Create();
    // ctx.CompileFile("tests/runtime/arrays.bl", "Runtime Arrays");
    // fmt::print("{}\n", ctx.Disassemble("Runtime Arrays"));
    // ctx.Run("Runtime Arrays");
    // ctx.Call("main", "Runtime Arrays");
    // 
    // ctx.GetGlobal("result");
    // REQUIRE(ctx.GetInt(-1) == 140);
}
