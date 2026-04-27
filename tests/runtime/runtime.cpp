#include "aria/context.hpp"
#include "aria/core.hpp"

#include "catch2.hpp"

TEST_CASE("Runtime Variable Declaration") {
    Aria::Context ctx = Aria::Context::Create();
    ctx.CompileFile("tests/runtime/variable_declaration.aria");
    ctx.Run();
    
    ctx.GetGlobal("variable_declaration::f");
    REQUIRE(ctx.GetBool(-1) == false);
    ctx.GetGlobal("variable_declaration::t");
    REQUIRE(ctx.GetBool(-1) == true);
    ctx.GetGlobal("variable_declaration::i");
    REQUIRE(ctx.GetInt(-1) == 99);

    ctx.Pop(3);
}

TEST_CASE("Runtime Basic Expressions") {
    Aria::Context ctx = Aria::Context::Create();
    ctx.CompileFile("tests/runtime/basic_expressions.aria");
    ctx.AddExternalFunction("should_never_be_called()", [](Aria::Context* ctx) {
        throw std::runtime_error("function that shouldn't be called was called");
    });
    
    ctx.Run();
    
    ctx.GetGlobal("basic_expressions::a");
    REQUIRE(ctx.GetInt(-1) == 10);
    ctx.GetGlobal("basic_expressions::b");
    REQUIRE(ctx.GetInt(-1) == -3);
    ctx.GetGlobal("basic_expressions::c");
    REQUIRE(ctx.GetInt(-1) == 10);

    ctx.GetGlobal("basic_expressions::d");
    REQUIRE(ctx.GetBool(-1) == false);
    ctx.GetGlobal("basic_expressions::e");
    REQUIRE(ctx.GetBool(-1) == false);
    ctx.GetGlobal("basic_expressions::f");
    REQUIRE(ctx.GetBool(-1) == true);
    ctx.GetGlobal("basic_expressions::g");
    REQUIRE(ctx.GetBool(-1) == false);

    ctx.GetGlobal("basic_expressions::h");
    REQUIRE(ctx.GetBool(-1) == true);
    ctx.GetGlobal("basic_expressions::i");
    REQUIRE(ctx.GetBool(-1) == true);
    ctx.GetGlobal("basic_expressions::j");
    REQUIRE(ctx.GetBool(-1) == false);
    ctx.GetGlobal("basic_expressions::k");
    REQUIRE(ctx.GetBool(-1) == true);

    ctx.GetGlobal("basic_expressions::l");
    REQUIRE(ctx.GetInt(-1) == 32);
    ctx.GetGlobal("basic_expressions::m");
    REQUIRE(ctx.GetInt(-1) == 1);
    ctx.GetGlobal("basic_expressions::n");
    REQUIRE(ctx.GetInt(-1) == 0);
    ctx.GetGlobal("basic_expressions::o");
    REQUIRE(ctx.GetInt(-1) == 4);
    ctx.GetGlobal("basic_expressions::p");
    REQUIRE(ctx.GetInt(-1) == 20);

    ctx.GetGlobal("basic_expressions::cc");
    REQUIRE(ctx.GetInt(-1) == 4);
    ctx.GetGlobal("basic_expressions::dd");
    REQUIRE(ctx.GetInt(-1) == 50);
}

TEST_CASE("Runtime Functions") {
    Aria::Context ctx = Aria::Context::Create();
    ctx.CompileFile("tests/runtime/functions.aria");
    ctx.AddExternalFunction("external_function()", [](Aria::Context* ctx) {
        REQUIRE(ctx->GetInt(-1) == 5);
        REQUIRE(ctx->GetInt(-2) == 66);
        REQUIRE(ctx->GetInt(-3) == 50);
        ctx->Pop(3);
    });

    ctx.Run();
    
    ctx.Call("main()", 0);
    
    ctx.GetGlobal("functions::result");
    REQUIRE(ctx.GetInt(-1) == 24);
    ctx.GetGlobal("functions::otherResult");
    REQUIRE(ctx.GetInt(-1) == 26);
    ctx.Pop(2);
}

TEST_CASE("Runtime Control Flow") {
    Aria::Context ctx = Aria::Context::Create();
    ctx.CompileFile("tests/runtime/control_flow.aria");
    ctx.Run();
    
    ctx.Call("control_flow::test_while()", 0);
    REQUIRE(ctx.GetInt(-1) == 10);
    ctx.Pop(1);
    
    ctx.Call("control_flow::test_do_while1()", 0);
    REQUIRE(ctx.GetInt(-1) == 10);
    ctx.Pop(1);
    
    ctx.Call("control_flow::test_do_while2()", 0);
    REQUIRE(ctx.GetBool(-1) == true);
    ctx.Pop(1);
    
    ctx.Call("control_flow::test_for()", 0);
    REQUIRE(ctx.GetInt(-1) == 190);
    ctx.Pop(1);

    ctx.Call("control_flow::test_break()", 0);
    REQUIRE(ctx.GetInt(-1) == 69);
    ctx.Pop(1);
    
    ctx.Call("control_flow::test_if()", 0);
    REQUIRE(ctx.GetBool(-1) == false);
    ctx.Pop(1);
}

TEST_CASE("Runtime Recursion") {
    Aria::Context ctx = Aria::Context::Create();
    ctx.CompileFile("tests/runtime/recursion.aria");
    ctx.Run();
    
    ctx.PushInt(10);
    ctx.Call("recursion::fib(int)", 1);
    REQUIRE(ctx.GetInt(-1) == 55);
    ctx.Pop(1);
    
    ctx.PushInt(20);
    ctx.Call("recursion::fib(int)", 1);
    REQUIRE(ctx.GetInt(-1) == 6765);
    ctx.Pop(1);
}

TEST_CASE("Runtime Casts") {
    Aria::Context ctx = Aria::Context::Create();
    ctx.CompileFile("tests/runtime/casts.aria");
    ctx.Run();

    ctx.GetGlobal("casts::a");
    REQUIRE(ctx.GetInt(-1) == 2);
    ctx.GetGlobal("casts::b");
    REQUIRE((ctx.GetFloat(-1) > 1.9999f && ctx.GetFloat(-1) < 2.0001f)); // Due to floating point inaccuracies we don't require exact matching numbers
    ctx.GetGlobal("casts::c");
    REQUIRE(ctx.GetLong(-1) == static_cast<int64_t>(5));
    ctx.GetGlobal("casts::d");
    REQUIRE((ctx.GetDouble(-1) > 9.9999999 && ctx.GetDouble(-1) < 10.000000001));

    ctx.Pop(4);
}

TEST_CASE("Runtime Strings") {
    Aria::Context ctx = Aria::Context::Create();
    ctx.CompileFile("tests/runtime/strings.aria");
    ctx.Run();

    ctx.GetGlobal("strings::str");
    REQUIRE(ctx.GetString(-1) == "Hello world!");
    ctx.GetGlobal("strings::copyStr");
    REQUIRE(ctx.GetString(-1) == "Hello world!");
    ctx.GetGlobal("strings::escaped");
    REQUIRE(ctx.GetString(-1) == std::string_view("\n\r\t\0\0\0Hello world", 17));
    ctx.GetGlobal("strings::formatted");
    REQUIRE(ctx.GetString(-1) == "Hello world!, Bye world!, See you later!");

    ctx.Pop(3);
}

TEST_CASE("Runtime Structs") {
    // Aria::Context ctx = Aria::Context::Create();
    // ctx.CompileFile("tests/runtime/structs.aria", "Runtime Structs");
    // ctx.Run();
    // 
    // ctx.GetGlobalPtr("p");
    // ctx.Call("Player::GetX()", 1);
    // REQUIRE(ctx.GetFloat(-1) == 5.0f);
    // ctx.Pop(1); // Pop the return value
    // 
    // ctx.GetGlobalPtr("p");
    // ctx.Call("Player::GetY()", 1);
    // REQUIRE(ctx.GetFloat(-1) == 4.0f);
    // ctx.Pop(1);
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
