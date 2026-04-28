#pragma once

#include "aria/context.hpp"

namespace Aria::Internal {
    
    void __aria_destruct_str(Context* ctx);
    void __aria_copy_str(Context* ctx);
    void __aria_append_str_char(Context* ctx);
    void __aria_append_str_uchar(Context* ctx);
    void __aria_append_str_short(Context* ctx);
    void __aria_append_str_ushort(Context* ctx);
    void __aria_append_str_int(Context* ctx);
    void __aria_append_str_uint(Context* ctx);
    void __aria_append_str_long(Context* ctx);
    void __aria_append_str_ulong(Context* ctx);
    void __aria_append_str_float(Context* ctx);
    void __aria_append_str_double(Context* ctx);
    void __aria_append_str_string(Context* ctx);

} // namespace Aria::Internal