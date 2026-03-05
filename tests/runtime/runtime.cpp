#include "black_lua/context.hpp"

#include "catch2.hpp"

TEST_CASE("Runtime Variable Declaration") {
    BlackLua::Context ctx = BlackLua::Context::Create();
    ctx.CompileFile("tests/runtime/variable_declaration.bl", "Runtime Variable Declaration");
    ctx.Run("Runtime Variable Declaration");
    
    ctx.PushGlobal("f");
    REQUIRE(ctx.GetBool(-1) == false);
    ctx.PushGlobal("t");
    REQUIRE(ctx.GetBool(-1) == true);
    ctx.PushGlobal("i");
    REQUIRE(ctx.GetInt(-1) == 99);
}

TEST_CASE("Runtime Basic Expressions") {
    BlackLua::Context ctx = BlackLua::Context::Create();
    ctx.CompileFile("tests/runtime/basic_expressions.bl", "Runtime Basic Expressions");
    ctx.AddExternalFunction("ShouldNeverBeCalled", [](BlackLua::Context* ctx) {
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
    // BlackLua::Context ctx = BlackLua::Context::Create();
    // ctx.CompileFile("tests/runtime/functions.bl", "Runtime Functions");
    // ctx.AddExternalFunction("ExternalFunction", [](BlackLua::Context* ctx) {
    //     REQUIRE(ctx->GetInt(-3) == 5);
    //     REQUIRE(ctx->GetInt(-2) == 66);
    //     REQUIRE(ctx->GetInt(-1) == 50);
    // }, "Runtime Functions");
    // ctx.Run("Runtime Functions");
    // 
    // ctx.Call("main", "Runtime Functions");
    // 
    // ctx.PushGlobal("result");
    // REQUIRE(ctx.GetInt(-1) == 24);
    // ctx.PushGlobal("otherResult");
    // REQUIRE(ctx.GetInt(-1) == 26);
}

TEST_CASE("Runtime Control Flow") {
    // BlackLua::Context ctx = BlackLua::Context::Create();
    // ctx.CompileFile("tests/runtime/control_flow.bl", "Runtime Control Flow");
    // ctx.Run("Runtime Control Flow");
    // 
    // ctx.Call("While", "Runtime Control Flow");
    // REQUIRE(ctx.GetInt(-1) == 10);
    // ctx.Pop(1);
    // 
    // ctx.Call("DoWhile1", "Runtime Control Flow");
    // REQUIRE(ctx.GetInt(-1) == 10);
    // ctx.Pop(1);
    // 
    // ctx.Call("DoWhile2", "Runtime Control Flow");
    // REQUIRE(ctx.GetBool(-1) == true);
    // ctx.Pop(1);
    // 
    // ctx.Call("If", "Runtime Control Flow");
    // REQUIRE(ctx.GetBool(-1) == false);
    // ctx.Pop(1);
}

TEST_CASE("Runtime Recursion") {
    // BlackLua::Context ctx = BlackLua::Context::Create();
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
    // BlackLua::Context ctx = BlackLua::Context::Create();
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
    // BlackLua::Context ctx = BlackLua::Context::Create();
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
    // BlackLua::Context ctx = BlackLua::Context::Create();
    // ctx.CompileFile("tests/runtime/arrays.bl", "Runtime Arrays");
    // fmt::print("{}\n", ctx.Disassemble("Runtime Arrays"));
    // ctx.Run("Runtime Arrays");
    // ctx.Call("main", "Runtime Arrays");
    // 
    // ctx.PushGlobal("result");
    // REQUIRE(ctx.GetInt(-1) == 140);
}
