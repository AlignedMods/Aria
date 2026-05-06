#include "aria/context.hpp"

#include "fmt/format.h"

static void print_help(const char* prog_name) {
    fmt::println("{}: help: ", prog_name);
    fmt::println("  {} <file>", prog_name);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_help(argv[0]);
        return 1;
    }

    Aria::Context ctx;
    ctx.load_file(argv[1]);
    ctx.run();

    if (ctx.has_function("main()")) {
        ctx.call("main()", 0);
    }

    return 0;
}