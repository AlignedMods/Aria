#include "aria/internal/stdlib/io.hpp"

#include <fmt/printf.h>

namespace Aria::Internal {

    void __aria_print(Context* ctx) {
        ctx->GetArg(0);
        fmt::println("{}", ctx->GetString(-1));
        ctx->Pop(1);
    }

} // namespace Aria::Internal