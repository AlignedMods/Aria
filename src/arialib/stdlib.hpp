#pragma once

#include "aria/context.hpp"

namespace Aria::Internal {

    void print_stdout(Context* ctx);
    void close_file(Context* ctx);

} // namespace Aria::Internal