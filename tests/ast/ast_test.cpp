#include "common.hpp"

#include <string_view>

static const char* ast_basic_expect = 
"'tests/test_suite/ast/basic.aria'\n"
"\n"
"TranslationUnitDecl\n"
"    ModuleDecl 'main'\n"
"    FunctionDecl 'main' public 'void()'\n"
"        BlockStmt \n";

static const char* ast_var_expect = 
"'tests/test_suite/ast/var.aria'\n"
"\n"
"TranslationUnitDecl\n"
"    ModuleDecl 'var'\n"
"    FunctionDecl 'main' public 'void()'\n"
"        BlockStmt \n"
"            VarDecl 'x' 'int'\n"
"                IntegerLiteralExpr 0 'int' rvalue\n"
"            VarDecl 'y' 'bool'\n"
"                BooleanLiteralExpr false 'bool' rvalue\n"
"            VarDecl 'z' 'double'\n"
"                FloatingLiteralExpr 6 'double' rvalue\n";

static const char* ast_binary_expr_expect =
"'tests/test_suite/ast/binary_expr.aria'\n"
"\n"
"TranslationUnitDecl\n"
"    ModuleDecl 'binary_expr'\n"
"    FunctionDecl 'main' public 'void()'\n"
"        BlockStmt \n"
"            VarDecl 'a' 'int'\n"
"                BinaryOperatorExpr '+' 'int' rvalue\n"
"                    IntegerLiteralExpr 6 'int' rvalue\n"
"                    IntegerLiteralExpr 7 'int' rvalue\n"
"            VarDecl 'b' 'int'\n"
"                BinaryOperatorExpr '-' 'int' rvalue\n"
"                    BinaryOperatorExpr '*' 'int' rvalue\n"
"                        IntegerLiteralExpr 7 'int' rvalue\n"
"                        IntegerLiteralExpr 7 'int' rvalue\n"
"                    IntegerLiteralExpr 2 'int' rvalue\n"
"            VarDecl 'c' 'int'\n"
"                BinaryOperatorExpr '&' 'int' rvalue\n"
"                    BinaryOperatorExpr '/' 'int' rvalue\n"
"                        IntegerLiteralExpr 7 'int' rvalue\n"
"                        ParenExpr 'int' rvalue\n"
"                            BinaryOperatorExpr '*' 'int' rvalue\n"
"                                IntegerLiteralExpr 7 'int' rvalue\n"
"                                IntegerLiteralExpr 2 'int' rvalue\n"
"                    IntegerLiteralExpr 3 'int' rvalue\n"
"            VarDecl 'd' 'int'\n"
"                BinaryOperatorExpr '%' 'int' rvalue\n"
"                    IntegerLiteralExpr 1200 'int' rvalue\n"
"                    IntegerLiteralExpr 4 'int' rvalue\n"
"            VarDecl 'e' 'int'\n"
"                BinaryOperatorExpr '|' 'int' rvalue\n"
"                    BinaryOperatorExpr '&' 'int' rvalue\n"
"                        BinaryOperatorExpr '^' 'int' rvalue\n"
"                            IntegerLiteralExpr 7 'int' rvalue\n"
"                            IntegerLiteralExpr 7 'int' rvalue\n"
"                        IntegerLiteralExpr 4 'int' rvalue\n"
"                    BinaryOperatorExpr '*' 'int' rvalue\n"
"                        IntegerLiteralExpr 3 'int' rvalue\n"
"                        IntegerLiteralExpr 7 'int' rvalue\n"
"            VarDecl 'f' 'bool'\n"
"                BinaryOperatorExpr '&&' 'bool' rvalue\n"
"                    BooleanLiteralExpr true 'bool' rvalue\n"
"                    BooleanLiteralExpr false 'bool' rvalue\n"
"            VarDecl 'g' 'bool'\n"
"                BinaryOperatorExpr '||' 'bool' rvalue\n"
"                    BooleanLiteralExpr false 'bool' rvalue\n"
"                    BinaryOperatorExpr '&&' 'bool' rvalue\n"
"                        BooleanLiteralExpr false 'bool' rvalue\n"
"                        BooleanLiteralExpr true 'bool' rvalue\n"
"            VarDecl 'h' 'bool'\n"
"                BinaryOperatorExpr '<' 'bool' rvalue\n"
"                    IntegerLiteralExpr 6 'int' rvalue\n"
"                    IntegerLiteralExpr 9 'int' rvalue\n"
"            VarDecl 'i' 'bool'\n"
"                BinaryOperatorExpr '>' 'bool' rvalue\n"
"                    IntegerLiteralExpr 6 'int' rvalue\n"
"                    IntegerLiteralExpr 9 'int' rvalue\n"
"            VarDecl 'j' 'bool'\n"
"                BinaryOperatorExpr '==' 'bool' rvalue\n"
"                    IntegerLiteralExpr 6 'int' rvalue\n"
"                    IntegerLiteralExpr 9 'int' rvalue\n"
"            VarDecl 'k' 'bool'\n"
"                BinaryOperatorExpr '!=' 'bool' rvalue\n"
"                    IntegerLiteralExpr 6 'int' rvalue\n"
"                    IntegerLiteralExpr 177 'int' rvalue\n"
"            VarDecl 'l' 'bool'\n"
"                BinaryOperatorExpr '&&' 'bool' rvalue\n"
"                    BinaryOperatorExpr '==' 'bool' rvalue\n"
"                        IntegerLiteralExpr 6 'int' rvalue\n"
"                        IntegerLiteralExpr 6 'int' rvalue\n"
"                    BinaryOperatorExpr '!=' 'bool' rvalue\n"
"                        IntegerLiteralExpr 7 'int' rvalue\n"
"                        IntegerLiteralExpr 6 'int' rvalue\n";

void test_ast_basic(std::string_view ariac_path) {
    std::string cmd = fmt::format("{} tests/test_suite/ast/basic.aria -no-stdlib -dump-ast-to-file tests/output/ast_basic.ast", ariac_path);
    // NOTE: Using system() is not good and should be changed in the future
    system(cmd.c_str());

    std::string ast = read_file("tests/output/ast_basic.ast");
    REQUIRE(ast == ast_basic_expect);
}

void test_ast_var(std::string_view ariac_path) {
    std::string cmd = fmt::format("{} tests/test_suite/ast/var.aria -no-stdlib -dump-ast-to-file tests/output/ast_var.ast", ariac_path);
    // NOTE: Using system() is not good and should be changed in the future
    system(cmd.c_str());

    std::string ast = read_file("tests/output/ast_var.ast");
    REQUIRE(ast == ast_var_expect);
}

void test_ast_binary_expr(std::string_view ariac_path) {
    std::string cmd = fmt::format("{} tests/test_suite/ast/binary_expr.aria -no-stdlib -dump-ast-to-file tests/output/ast_binary_expr.ast", ariac_path);
    // NOTE: Using system() is not good and should be changed in the future
    system(cmd.c_str());

    std::string ast = read_file("tests/output/ast_binary_expr.ast");
    REQUIRE(ast == ast_binary_expr_expect);
}