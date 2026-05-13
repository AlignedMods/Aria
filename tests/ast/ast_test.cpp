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

static const char* ast_casts_expect =
"'tests/test_suite/ast/casts.aria'\n"
"\n"
"TranslationUnitDecl\n"
"    ModuleDecl 'casts'\n"
"    FunctionDecl 'main' public 'void()'\n"
"        BlockStmt \n"
"            VarDecl 'a' 'int'\n"
"                IntegerLiteralExpr 6 'int' rvalue\n"
"            VarDecl 'b' 'int'\n"
"                ImplicitCastExpr <LValueToRValue> 'int' rvalue\n"
"                    DeclRefExpr 'a' Var 'int' lvalue\n"
"            VarDecl 'c' 'float'\n"
"                ImplicitCastExpr <IntegralToFloating> 'float' rvalue\n"
"                    ImplicitCastExpr <LValueToRValue> 'int' rvalue\n"
"                        DeclRefExpr 'a' Var 'int' lvalue\n"
"            VarDecl 'd' 'char'\n"
"                CastExpr 'char' rvalue\n"
"                    ImplicitCastExpr <Integral> 'char' rvalue\n"
"                        ImplicitCastExpr <LValueToRValue> 'int' rvalue\n"
"                            DeclRefExpr 'a' Var 'int' lvalue\n"
"            VarDecl 'e' 'char'\n"
"                ImplicitCastExpr <LValueToRValue> 'char' rvalue\n"
"                    DeclRefExpr 'd' Var 'char' lvalue\n"
"            VarDecl 'f' 'ulong'\n"
"                ImplicitCastExpr <Integral> 'ulong' rvalue\n"
"                    IntegerLiteralExpr 7 'int' rvalue\n"
"            VarDecl 'g' 'long'\n"
"                ImplicitCastExpr <Integral> 'long' rvalue\n"
"                    ImplicitCastExpr <LValueToRValue> 'ulong' rvalue\n"
"                        DeclRefExpr 'f' Var 'ulong' lvalue\n";

static const char* ast_binary_promotion_expect =
"'tests/test_suite/ast/binary_promotion.aria'\n"
"\n"
"TranslationUnitDecl\n"
"    ModuleDecl 'binary_promotion'\n"
"    FunctionDecl 'main' public 'void()'\n"
"        BlockStmt \n"
"            VarDecl 'a' 'long'\n"
"                BinaryOperatorExpr '+' 'long' rvalue\n"
"                    ImplicitCastExpr <Integral> 'long' rvalue\n"
"                        IntegerLiteralExpr 7 'int' rvalue\n"
"                    CastExpr 'long' rvalue\n"
"                        ImplicitCastExpr <Integral> 'long' rvalue\n"
"                            IntegerLiteralExpr 8 'int' rvalue\n"
"            VarDecl 'b' 'short'\n"
"                ImplicitCastExpr <Integral> 'short' rvalue\n"
"                    BinaryOperatorExpr '+' 'int' rvalue\n"
"                        ImplicitCastExpr <Integral> 'int' rvalue\n"
"                            CastExpr 'short' rvalue\n"
"                                ImplicitCastExpr <Integral> 'short' rvalue\n"
"                                    IntegerLiteralExpr 6 'int' rvalue\n"
"                        ImplicitCastExpr <Integral> 'int' rvalue\n"
"                            CastExpr 'short' rvalue\n"
"                                ImplicitCastExpr <Integral> 'short' rvalue\n"
"                                    IntegerLiteralExpr 9 'int' rvalue\n"
"            VarDecl 'c' 'float'\n"
"                ImplicitCastExpr <Floating> 'float' rvalue\n"
"                    BinaryOperatorExpr '+' 'double' rvalue\n"
"                        ImplicitCastExpr <IntegralToFloating> 'double' rvalue\n"
"                            IntegerLiteralExpr 7 'int' rvalue\n"
"                        FloatingLiteralExpr 7 'double' rvalue\n"
"            VarDecl 'd' 'double'\n"
"                ImplicitCastExpr <Floating> 'double' rvalue\n"
"                    BinaryOperatorExpr '-' 'float' rvalue\n"
"                        ImplicitCastExpr <LValueToRValue> 'float' rvalue\n"
"                            DeclRefExpr 'c' Var 'float' lvalue\n"
"                        ImplicitCastExpr <IntegralToFloating> 'float' rvalue\n"
"                            IntegerLiteralExpr 6 'int' rvalue\n"
"            VarDecl 'e' 'char'\n"
"                CharacterLiteralExpr 0x66 'char' rvalue\n"
"            VarDecl 'f' 'uchar'\n"
"                ImplicitCastExpr <Integral> 'uchar' rvalue\n"
"                    BinaryOperatorExpr '-' 'int' rvalue\n"
"                        ImplicitCastExpr <Integral> 'int' rvalue\n"
"                            ImplicitCastExpr <LValueToRValue> 'char' rvalue\n"
"                                DeclRefExpr 'e' Var 'char' lvalue\n"
"                        ImplicitCastExpr <Integral> 'int' rvalue\n"
"                            ImplicitCastExpr <LValueToRValue> 'char' rvalue\n"
"                                DeclRefExpr 'e' Var 'char' lvalue\n";

static const char* ast_functions_expect =
"'tests/test_suite/ast/functions.aria'\n"
"\n"
"TranslationUnitDecl\n"
"    ModuleDecl 'functions'\n"
"    FunctionDecl 'foo' public 'void()'\n"
"        BlockStmt \n"
"    FunctionDecl 'add' public 'int(int, int)'\n"
"        ParamDecl 'lhs' 'int'\n"
"        ParamDecl 'rhs' 'int'\n"
"        BlockStmt \n"
"            ReturnStmt\n"
"                BinaryOperatorExpr '+' 'int' rvalue\n"
"                    ImplicitCastExpr <LValueToRValue> 'int' rvalue\n"
"                        DeclRefExpr 'lhs' Param 'int' lvalue\n"
"                    ImplicitCastExpr <LValueToRValue> 'int' rvalue\n"
"                        DeclRefExpr 'rhs' Param 'int' lvalue\n"
"    FunctionDecl 'sub' public 'int(int, int)'\n"
"        ParamDecl 'lhs' 'int'\n"
"        ParamDecl 'rhs' 'int'\n"
"        BlockStmt \n"
"            ReturnStmt\n"
"                BinaryOperatorExpr '-' 'int' rvalue\n"
"                    ImplicitCastExpr <LValueToRValue> 'int' rvalue\n"
"                        DeclRefExpr 'lhs' Param 'int' lvalue\n"
"                    ImplicitCastExpr <LValueToRValue> 'int' rvalue\n"
"                        DeclRefExpr 'rhs' Param 'int' lvalue\n"
"    FunctionDecl 'recursive' public 'int(int)'\n"
"        ParamDecl 'x' 'int'\n"
"        BlockStmt \n"
"            ReturnStmt\n"
"                CallExpr 'int' rvalue\n"
"                    DeclRefExpr 'recursive' Function 'int(int)' lvalue\n"
"                    ImplicitCastExpr <LValueToRValue> 'int' rvalue\n"
"                        DeclRefExpr 'x' Param 'int' lvalue\n"
"    FunctionDecl 'main' public 'void()'\n"
"        BlockStmt \n"
"            CallExpr 'void' rvalue\n"
"                DeclRefExpr 'foo' Function 'void()' lvalue\n"
"            VarDecl 'x' 'int'\n"
"                CallExpr 'int' rvalue\n"
"                    DeclRefExpr 'sub' Function 'int(int, int)' lvalue\n"
"                    CallExpr 'int' rvalue\n"
"                        DeclRefExpr 'add' Function 'int(int, int)' lvalue\n"
"                        IntegerLiteralExpr 5 'int' rvalue\n"
"                        IntegerLiteralExpr 6 'int' rvalue\n"
"                    IntegerLiteralExpr 6 'int' rvalue\n"
"            CallExpr 'int' rvalue\n"
"                DeclRefExpr 'recursive' Function 'int(int)' lvalue\n"
"                IntegerLiteralExpr 66 'int' rvalue\n";

static const char* ast_overloaded_functions =
"'tests/test_suite/ast/overloaded_functions.aria'\n"
"\n"
"TranslationUnitDecl\n"
"    ModuleDecl 'overloaded_functions'\n"
"    FunctionDecl 'foo' public 'void(int)'\n"
"        ParamDecl 'x' 'int'\n"
"        BlockStmt \n"
"    FunctionDecl 'foo' public 'void(long)'\n"
"        ParamDecl 'x' 'long'\n"
"        BlockStmt \n"
"    FunctionDecl 'foo' public 'void(ulong)'\n"
"        ParamDecl 'x' 'ulong'\n"
"        BlockStmt \n"
"    FunctionDecl 'main' public 'void()'\n"
"        BlockStmt \n"
"            VarDecl 'x' 'ulong'\n"
"                ImplicitCastExpr <Integral> 'ulong' rvalue\n"
"                    IntegerLiteralExpr 65 'int' rvalue\n"
"            CallExpr 'void' rvalue\n"
"                DeclRefExpr 'foo' Function 'void(int)' lvalue\n"
"                IntegerLiteralExpr 8 'int' rvalue\n"
"            CallExpr 'void' rvalue\n"
"                DeclRefExpr 'foo' Function 'void(long)' lvalue\n"
"                CastExpr 'long' rvalue\n"
"                    ImplicitCastExpr <Integral> 'long' rvalue\n"
"                        IntegerLiteralExpr 9 'int' rvalue\n"
"            CallExpr 'void' rvalue\n"
"                DeclRefExpr 'foo' Function 'void(ulong)' lvalue\n"
"                CastExpr 'ulong' rvalue\n"
"                    ImplicitCastExpr <Integral> 'ulong' rvalue\n"
"                        CastExpr 'long' rvalue\n"
"                            ImplicitCastExpr <Integral> 'long' rvalue\n"
"                                IntegerLiteralExpr 55 'int' rvalue\n"
"            CallExpr 'void' rvalue\n"
"                DeclRefExpr 'foo' Function 'void(ulong)' lvalue\n"
"                ImplicitCastExpr <LValueToRValue> 'ulong' rvalue\n"
"                    DeclRefExpr 'x' Var 'ulong' lvalue\n"
"            CallExpr 'void' rvalue\n"
"                DeclRefExpr 'foo' Function 'void(ulong)' lvalue\n"
"                BinaryOperatorExpr '*' 'ulong' rvalue\n"
"                    ImplicitCastExpr <LValueToRValue> 'ulong' rvalue\n"
"                        DeclRefExpr 'x' Var 'ulong' lvalue\n"
"                    ImplicitCastExpr <LValueToRValue> 'ulong' rvalue\n"
"                        DeclRefExpr 'x' Var 'ulong' lvalue\n"
"            CallExpr 'void' rvalue\n"
"                DeclRefExpr 'foo' Function 'void(int)' lvalue\n"
"                CastExpr 'int' rvalue\n"
"                    ImplicitCastExpr <Integral> 'int' rvalue\n"
"                        ParenExpr 'ulong' rvalue\n"
"                            BinaryOperatorExpr '-' 'ulong' rvalue\n"
"                                ImplicitCastExpr <LValueToRValue> 'ulong' rvalue\n"
"                                    DeclRefExpr 'x' Var 'ulong' lvalue\n"
"                                ImplicitCastExpr <LValueToRValue> 'ulong' rvalue\n"
"                                    DeclRefExpr 'x' Var 'ulong' lvalue\n";

static const char* ast_control_flow_expect =
"'tests/test_suite/ast/control_flow.aria'\n"
"\n"
"TranslationUnitDecl\n"
"    ModuleDecl 'control_flow'\n"
"    FunctionDecl 'main' public 'void()'\n"
"        BlockStmt \n"
"            VarDecl 'a' 'ulong'\n"
"                ImplicitCastExpr <Integral> 'ulong' rvalue\n"
"                    IntegerLiteralExpr 0 'int' rvalue\n"
"            WhileStmt\n"
"                BinaryOperatorExpr '&&' 'bool' rvalue\n"
"                    BooleanLiteralExpr true 'bool' rvalue\n"
"                    BooleanLiteralExpr true 'bool' rvalue\n"
"                BlockStmt \n"
"                    IfStmt\n"
"                        BinaryOperatorExpr '==' 'bool' rvalue\n"
"                            ImplicitCastExpr <LValueToRValue> 'ulong' rvalue\n"
"                                DeclRefExpr 'a' Var 'ulong' lvalue\n"
"                            ImplicitCastExpr <Integral> 'ulong' rvalue\n"
"                                IntegerLiteralExpr 10 'int' rvalue\n"
"                        BlockStmt \n"
"                            BreakStmt\n"
"                        BlockStmt \n"
"                            UnaryOperatorExpr '++' infix 'ulong' rvalue\n"
"                                DeclRefExpr 'a' Var 'ulong' lvalue\n"
"            ForStmt\n"
"                VarDecl 'i' 'ulong'\n"
"                    ImplicitCastExpr <Integral> 'ulong' rvalue\n"
"                        IntegerLiteralExpr 0 'int' rvalue\n"
"                BinaryOperatorExpr '<' 'bool' rvalue\n"
"                    ImplicitCastExpr <LValueToRValue> 'ulong' rvalue\n"
"                        DeclRefExpr 'i' Var 'ulong' lvalue\n"
"                    ImplicitCastExpr <Integral> 'ulong' rvalue\n"
"                        IntegerLiteralExpr 100 'int' rvalue\n"
"                UnaryOperatorExpr '++' infix 'ulong' rvalue\n"
"                    DeclRefExpr 'i' Var 'ulong' lvalue\n"
"                BlockStmt \n"
"                    CompoundAssignExpr '-=' 'ulong' lvalue\n"
"                        DeclRefExpr 'a' Var 'ulong' lvalue\n"
"                        ImplicitCastExpr <Integral> 'ulong' rvalue\n"
"                            IntegerLiteralExpr 2 'int' rvalue\n"
"                    ContinueStmt\n"
"            DoWhileStmt\n"
"                BooleanLiteralExpr false 'bool' rvalue\n"
"                BlockStmt \n"
"                    CompoundAssignExpr '*=' 'ulong' lvalue\n"
"                        DeclRefExpr 'a' Var 'ulong' lvalue\n"
"                        BinaryOperatorExpr '*' 'ulong' rvalue\n"
"                            ImplicitCastExpr <LValueToRValue> 'ulong' rvalue\n"
"                                DeclRefExpr 'a' Var 'ulong' lvalue\n"
"                            ImplicitCastExpr <LValueToRValue> 'ulong' rvalue\n"
"                                DeclRefExpr 'a' Var 'ulong' lvalue\n";

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

void test_ast_casts(std::string_view ariac_path) {
    std::string cmd = fmt::format("{} tests/test_suite/ast/casts.aria -no-stdlib -dump-ast-to-file tests/output/ast_casts.ast", ariac_path);
    // NOTE: Using system() is not good and should be changed in the future
    system(cmd.c_str());

    std::string ast = read_file("tests/output/ast_casts.ast");
    REQUIRE(ast == ast_casts_expect);
}

void test_ast_binary_promotion(std::string_view ariac_path) {
    std::string cmd = fmt::format("{} tests/test_suite/ast/binary_promotion.aria -no-stdlib -dump-ast-to-file tests/output/ast_binary_promotion.ast", ariac_path);
    // NOTE: Using system() is not good and should be changed in the future
    system(cmd.c_str());

    std::string ast = read_file("tests/output/ast_binary_promotion.ast");
    REQUIRE(ast == ast_binary_promotion_expect);
}

void test_ast_functions(std::string_view ariac_path) {
    std::string cmd = fmt::format("{} tests/test_suite/ast/functions.aria -no-stdlib -dump-ast-to-file tests/output/ast_functions.ast", ariac_path);
    // NOTE: Using system() is not good and should be changed in the future
    system(cmd.c_str());

    std::string ast = read_file("tests/output/ast_functions.ast");
    REQUIRE(ast == ast_functions_expect);
}

void test_ast_overloaded_functions(std::string_view ariac_path) {
    std::string cmd = fmt::format("{} tests/test_suite/ast/overloaded_functions.aria -no-stdlib -dump-ast-to-file tests/output/ast_overloaded_functions.ast", ariac_path);
    // NOTE: Using system() is not good and should be changed in the future
    system(cmd.c_str());

    std::string ast = read_file("tests/output/ast_overloaded_functions.ast");
    REQUIRE(ast == ast_overloaded_functions);
}

void test_ast_control_flow(std::string_view ariac_path) {
    std::string cmd = fmt::format("{} tests/test_suite/ast/control_flow.aria -no-stdlib -dump-ast-to-file tests/output/ast_control_flow.ast", ariac_path);
    // NOTE: Using system() is not good and should be changed in the future
    system(cmd.c_str());

    std::string ast = read_file("tests/output/ast_control_flow.ast");
    REQUIRE(ast == ast_control_flow_expect);
}