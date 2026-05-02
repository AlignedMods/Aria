#include "aria/internal/stdlib/io.hpp"

#include <fmt/printf.h>

namespace Aria::Internal {

    void __aria_print(Context* ctx) {
        fmt::println("{}", ctx->get_string(-1));
        ctx->pop(1);
    }

} // namespace Aria::Internal