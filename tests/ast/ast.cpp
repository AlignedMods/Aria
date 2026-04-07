#include "aria/aria.hpp"

#include "catch2.hpp"

TEST_CASE("AST Implicit Casts") {
    Aria::Context ctx = Aria::Context::Create();
    ctx.CompileFile("tests/ast/implicit_casts.aria");
}
