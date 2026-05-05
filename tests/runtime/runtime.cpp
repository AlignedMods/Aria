#include "aria/context.hpp"
#include "aria/core.hpp"

#include "catch2.hpp"

TEST_CASE("runtime: variable declaration") {
    Aria::Context ctx = Aria::Context::Create();
    ctx.compile_file("tests/runtime/variable_declaration.aria");
    ctx.run();
    
    ctx.get_global("variable_declaration::f");
    REQUIRE(ctx.get_bool(-1) == false);
    ctx.get_global("variable_declaration::t");
    REQUIRE(ctx.get_bool(-1) == true);
    ctx.get_global("variable_declaration::i");
    REQUIRE(ctx.get_int(-1) == 99);

    ctx.pop(3);
}

TEST_CASE("runtime: basic expressions") {
    Aria::Context ctx = Aria::Context::Create();
    ctx.compile_file("tests/runtime/basic_expressions.aria");
    ctx.add_external_function("should_never_be_called()", [](Aria::Context* ctx) {
        throw std::runtime_error("function that shouldn't be called was called");
    });
    
    ctx.run();
    
    ctx.get_global("basic_expressions::a");
    REQUIRE(ctx.get_int(-1) == 10);
    ctx.get_global("basic_expressions::b");
    REQUIRE(ctx.get_int(-1) == -3);
    ctx.get_global("basic_expressions::c");
    REQUIRE(ctx.get_int(-1) == 10);

    ctx.get_global("basic_expressions::d");
    REQUIRE(ctx.get_bool(-1) == false);
    ctx.get_global("basic_expressions::e");
    REQUIRE(ctx.get_bool(-1) == false);
    ctx.get_global("basic_expressions::f");
    REQUIRE(ctx.get_bool(-1) == true);
    ctx.get_global("basic_expressions::g");
    REQUIRE(ctx.get_bool(-1) == false);

    ctx.get_global("basic_expressions::h");
    REQUIRE(ctx.get_bool(-1) == true);
    ctx.get_global("basic_expressions::i");
    REQUIRE(ctx.get_bool(-1) == true);
    ctx.get_global("basic_expressions::j");
    REQUIRE(ctx.get_bool(-1) == false);
    ctx.get_global("basic_expressions::k");
    REQUIRE(ctx.get_bool(-1) == true);

    ctx.get_global("basic_expressions::l");
    REQUIRE(ctx.get_int(-1) == 32);
    ctx.get_global("basic_expressions::m");
    REQUIRE(ctx.get_int(-1) == 1);
    ctx.get_global("basic_expressions::n");
    REQUIRE(ctx.get_int(-1) == 0);
    ctx.get_global("basic_expressions::o");
    REQUIRE(ctx.get_int(-1) == 4);
    ctx.get_global("basic_expressions::p");
    REQUIRE(ctx.get_int(-1) == 20);

    ctx.get_global("basic_expressions::cc");
    REQUIRE(ctx.get_int(-1) == 4);
    ctx.get_global("basic_expressions::dd");
    REQUIRE(ctx.get_int(-1) == 50);
}

TEST_CASE("runtime: functions") {
    Aria::Context ctx = Aria::Context::Create();
    ctx.compile_file("tests/runtime/functions.aria");
    ctx.add_external_function("external_function()", [](Aria::Context* ctx) {
        REQUIRE(ctx->get_int(-1) == 5);
        REQUIRE(ctx->get_int(-2) == 66);
        REQUIRE(ctx->get_int(-3) == 50);
        ctx->pop(3);
    });

    ctx.run();
    
    ctx.call("main()", 0);
    
    ctx.get_global("functions::result");
    REQUIRE(ctx.get_int(-1) == 24);
    ctx.get_global("functions::otherResult");
    REQUIRE(ctx.get_int(-1) == 26);
    ctx.pop(2);
}

TEST_CASE("runtime: control flow") {
    Aria::Context ctx = Aria::Context::Create();
    ctx.compile_file("tests/runtime/control_flow.aria");
    ctx.run();
    
    ctx.call("control_flow::test_while()", 0);
    REQUIRE(ctx.get_int(-1) == 10);
    ctx.pop(1);
    
    ctx.call("control_flow::test_do_while1()", 0);
    REQUIRE(ctx.get_int(-1) == 10);
    ctx.pop(1);
    
    ctx.call("control_flow::test_do_while2()", 0);
    REQUIRE(ctx.get_bool(-1) == true);
    ctx.pop(1);
    
    ctx.call("control_flow::test_for()", 0);
    REQUIRE(ctx.get_int(-1) == 190);
    ctx.pop(1);

    ctx.call("control_flow::test_break()", 0);
    REQUIRE(ctx.get_int(-1) == 69);
    ctx.pop(1);
    
    ctx.call("control_flow::test_if()", 0);
    REQUIRE(ctx.get_bool(-1) == false);
    ctx.pop(1);
}

TEST_CASE("runtime: recursion") {
    Aria::Context ctx = Aria::Context::Create();
    ctx.compile_file("tests/runtime/recursion.aria");
    ctx.run();
    
    ctx.push_int(10);
    ctx.call("recursion::fib(int)", 1);
    REQUIRE(ctx.get_int(-1) == 55);
    ctx.pop(1);
    
    ctx.push_int(20);
    ctx.call("recursion::fib(int)", 1);
    REQUIRE(ctx.get_int(-1) == 6765);
    ctx.pop(1);

    ctx.push_long(5);
    ctx.call("recursion::factorial(long)", 1);
    REQUIRE(ctx.get_long(-1) == 120);
    ctx.pop(1);
    
    ctx.push_long(7);
    ctx.call("recursion::factorial(long)", 1);
    REQUIRE(ctx.get_long(-1) == 5040);
    ctx.pop(1);

    ctx.push_long(14);
    ctx.call("recursion::factorial(long)", 1);
    REQUIRE(ctx.get_long(-1) == 87178291200);
    ctx.pop(1);

    ctx.push_long(16);
    ctx.call("recursion::factorial(long)", 1);
    REQUIRE(ctx.get_long(-1) == 20922789888000);
    ctx.pop(1);

    ctx.push_int(30);
    ctx.call("recursion::sum(int)", 1);
    REQUIRE(ctx.get_int(-1) == 465);
    ctx.pop(1);
    
    ctx.push_int(50);
    ctx.call("recursion::sum(int)", 1);
    REQUIRE(ctx.get_int(-1) == 1275);
    ctx.pop(1);
}

TEST_CASE("runtime: casts") {
    Aria::Context ctx = Aria::Context::Create();
    ctx.compile_file("tests/runtime/casts.aria");
    ctx.run();

    ctx.get_global("casts::a");
    REQUIRE(ctx.get_int(-1) == 2);
    ctx.get_global("casts::b");
    REQUIRE((ctx.get_float(-1) > 1.9999f && ctx.get_float(-1) < 2.0001f)); // Due to floating point inaccuracies we don't require exact matching numbers
    ctx.get_global("casts::c");
    REQUIRE(ctx.get_long(-1) == static_cast<int64_t>(5));
    ctx.get_global("casts::d");
    REQUIRE((ctx.get_double(-1) > 9.9999999 && ctx.get_double(-1) < 10.000000001));

    ctx.pop(4);
}

TEST_CASE("runtime: strings") {
    Aria::Context ctx = Aria::Context::Create();
    ctx.compile_file("tests/runtime/strings.aria");
    ctx.run();

    ctx.get_global("strings::str");
    REQUIRE(ctx.get_string(-1) == "Hello world!");
    ctx.get_global("strings::copyStr");
    REQUIRE(ctx.get_string(-1) == "Hello world!");
    ctx.get_global("strings::escaped");
    REQUIRE(ctx.get_string(-1) == std::string_view("\n\r\t\0\0\0Hello world", 17));
    // ctx.get_global("strings::formatted");
    // REQUIRE(ctx.get_string(-1) == "Hello world!, Bye world!, See you later!");

    ctx.pop(3);
}

TEST_CASE("runtime: structs") {
    // Aria::Context ctx = Aria::Context::Create();
    // ctx.compile_file("tests/runtime/structs.aria", "runtime Structs");
    // ctx.run();
    // 
    // ctx.get_globalPtr("p");
    // ctx.call("Player::GetX()", 1);
    // REQUIRE(ctx.get_float(-1) == 5.0f);
    // ctx.pop(1); // pop the return value
    // 
    // ctx.get_globalPtr("p");
    // ctx.call("Player::GetY()", 1);
    // REQUIRE(ctx.get_float(-1) == 4.0f);
    // ctx.pop(1);
}

TEST_CASE("runtime: arrays") {
    // Aria::Context ctx = Aria::Context::Create();
    // ctx.compile_file("tests/runtime/arrays.bl", "runtime Arrays");
    // fmt::print("{}\n", ctx.Disassemble("runtime Arrays"));
    // ctx.run("runtime Arrays");
    // ctx.call("main", "runtime Arrays");
    // 
    // ctx.get_global("result");
    // REQUIRE(ctx.get_int(-1) == 140);
}
