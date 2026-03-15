#include "aria/context.hpp"
#include "aria/core.hpp"

#include "catch2.hpp"

TEST_CASE("Runtime Variable Declaration") {
    Aria::Context ctx = Aria::Context::Create();
    ctx.CompileFile("tests/runtime/variable_declaration.aria", "Runtime Variable Declaration");
    ctx.Run("Runtime Variable Declaration");
    
    ctx.PushGlobal("f");
    REQUIRE(ctx.GetBool(-1) == false);
    ctx.PushGlobal("t");
    REQUIRE(ctx.GetBool(-1) == true);
    ctx.PushGlobal("i");
    REQUIRE(ctx.GetInt(-1) == 99);

    ctx.Pop(3);
}

TEST_CASE("Runtime Basic Expressions") {
    Aria::Context ctx = Aria::Context::Create();
    ctx.CompileFile("tests/runtime/basic_expressions.aria", "Runtime Basic Expressions");
    ctx.AddExternalFunction("ShouldNeverBeCalled()", [](Aria::Context* ctx) {
        throw std::runtime_error("function that shouldn't be called was called");
    }, "Runtime Basic Expressions");
    ctx.Run("Runtime Basic Expressions");
    
    ctx.PushGlobal("a");
    REQUIRE(ctx.GetInt(-1) == 10);
    ctx.PushGlobal("b");
    REQUIRE(ctx.GetInt(-1) == -3);
    ctx.PushGlobal("c");
    REQUIRE(ctx.GetInt(-1) == 10);

    ctx.PushGlobal("d");
    REQUIRE(ctx.GetBool(-1) == false);
    ctx.PushGlobal("e");
    REQUIRE(ctx.GetBool(-1) == false);
    ctx.PushGlobal("f");
    REQUIRE(ctx.GetBool(-1) == true);
    ctx.PushGlobal("g");
    REQUIRE(ctx.GetBool(-1) == false);

    ctx.PushGlobal("h");
    REQUIRE(ctx.GetBool(-1) == true);
    ctx.PushGlobal("i");
    REQUIRE(ctx.GetBool(-1) == true);
    ctx.PushGlobal("j");
    REQUIRE(ctx.GetBool(-1) == false);
    ctx.PushGlobal("k");
    REQUIRE(ctx.GetBool(-1) == true);
}

TEST_CASE("Runtime Functions") {
    Aria::Context ctx = Aria::Context::Create();
    ctx.CompileFile("tests/runtime/functions.aria", "Runtime Functions");
    ctx.AddExternalFunction("ExternalFunction()", [](Aria::Context* ctx) {
        ctx->PushArg(0);
        ctx->PushArg(1);
        ctx->PushArg(2);
        REQUIRE(ctx->GetInt(-3) == 5);
        REQUIRE(ctx->GetInt(-2) == 66);
        REQUIRE(ctx->GetInt(-1) == 50);
        ctx->Pop(3);
    }, "Runtime Functions");
    ctx.Run("Runtime Functions");

    ctx.Call("main()", 0, "Runtime Functions");
    
    ctx.PushGlobal("result");
    REQUIRE(ctx.GetInt(-1) == 24);
    ctx.PushGlobal("otherResult");
    REQUIRE(ctx.GetInt(-1) == 26);
    ctx.Pop(2);
}

TEST_CASE("Runtime Control Flow") {
    Aria::Context ctx = Aria::Context::Create();
    ctx.CompileFile("tests/runtime/control_flow.aria", "Runtime Control Flow");

    ctx.Run("Runtime Control Flow");
    
    ctx.PushInt(0);
    ctx.Call("While()", 0, "Runtime Control Flow");
    REQUIRE(ctx.GetInt(-1) == 10);
    ctx.Pop(1);
    
    ctx.PushInt(0);
    ctx.Call("DoWhile1()", 0, "Runtime Control Flow");
    REQUIRE(ctx.GetInt(-1) == 10);
    ctx.Pop(1);
    
    ctx.PushBool(false);
    ctx.Call("DoWhile2()", 0, "Runtime Control Flow");
    REQUIRE(ctx.GetBool(-1) == true);
    ctx.Pop(1);
    
    ctx.PushBool(false);
    ctx.Call("If()", 0, "Runtime Control Flow");
    REQUIRE(ctx.GetBool(-1) == false);
    ctx.Pop(1);
}

TEST_CASE("Runtime Recursion") {
    // Aria::Context ctx = Aria::Context::Create();
    // ctx.CompileFile("tests/runtime/recursion.bl", "Runtime Recursion");
    // ctx.Run("Runtime Recursion");
    // 
    // ctx.PushInt(10);
    // ctx.Call("Fib", "Runtime Recursion");
    // REQUIRE(ctx.GetInt(-1) == 55);
    // ctx.Pop(2);
    // 
    // ctx.PushInt(20);
    // ctx.Call("Fib", "Runtime Recursion");
    // REQUIRE(ctx.GetInt(-1) == 6765);
    // ctx.Pop(2);
}

TEST_CASE("Runtime Casts") {
    // Aria::Context ctx = Aria::Context::Create();
    // ctx.CompileFile("tests/runtime/casts.bl", "Runtime Casts");
    // ctx.Run("Runtime Casts");
    // 
    // ctx.PushGlobal("a");
    // REQUIRE(ctx.GetInt(-1) == 2);
    // ctx.PushGlobal("b");
    // REQUIRE((ctx.GetFloat(-1) > 1.9999f && ctx.GetFloat(-1) < 2.0001f)); // Due to floating point inaccuracies we don't require exact matching numbers
    // ctx.PushGlobal("c");
    // REQUIRE(ctx.GetLong(-1) == static_cast<int64_t>(5));
}

TEST_CASE("Runtime Structs") {
    // Aria::Context ctx = Aria::Context::Create();
    // ctx.CompileFile("tests/runtime/structs.bl", "Runtime Structs");
    // ctx.Run("Runtime Structs");
    // 
    // ctx.PushGlobal("p");
    // ctx.Call("Player::GetX", "Runtime Structs");
    // REQUIRE((ctx.GetFloat(-1) > 4.9999f && ctx.GetFloat(-1) < 5.0001f));
    // ctx.Pop(1); // Pop the return value
    // ctx.Call("Player::GetY", "Runtime Structs");
    // REQUIRE((ctx.GetFloat(-1) > 3.9999f && ctx.GetFloat(-1) < 4.0001f));
}

TEST_CASE("Runtime Arrays") {
    // Aria::Context ctx = Aria::Context::Create();
    // ctx.CompileFile("tests/runtime/arrays.bl", "Runtime Arrays");
    // fmt::print("{}\n", ctx.Disassemble("Runtime Arrays"));
    // ctx.Run("Runtime Arrays");
    // ctx.Call("main", "Runtime Arrays");
    // 
    // ctx.PushGlobal("result");
    // REQUIRE(ctx.GetInt(-1) == 140);
}
