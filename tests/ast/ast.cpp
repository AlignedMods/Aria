#include "black_lua/black_lua.hpp"

#include "catch2.hpp"

TEST_CASE("AST Implicit Casts") {
    BlackLua::Context ctx = BlackLua::Context::Create();
    ctx.CompileFile("tests/ast/implicit_casts.bl", "AST Implicit Casts");
    
    fmt::println("{}", ctx.DumpAST("AST Implicit Casts"));
    // REQUIRE(ctx.DumpAST("AST Implicit Casts") == )
}
