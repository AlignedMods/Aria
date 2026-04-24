#include "aria/internal/stdlib/io.hpp"

#include <fmt/printf.h>

namespace Aria::Internal {

    void __aria_print(Context* ctx) {
        fmt::println("{}", ctx->GetString(-1));
        ctx->Pop(1);
    }

} // namespace Aria::Internal