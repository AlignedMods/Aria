#include "arialib/stdlib.hpp"

#include "fmt/format.h"

namespace Aria::Internal {

    void print_stdout(Context* ctx) {
        std::string_view str = ctx->get_string(-1);
        fmt::print("{}", str);
        ctx->pop(1);
    }

    void close_file(Context* ctx) {
        void* handle = ctx->get_pointer(-1);
        fclose(reinterpret_cast<FILE*>(handle));
        ctx->pop(1);
    }

} // namespace Aria::Internal