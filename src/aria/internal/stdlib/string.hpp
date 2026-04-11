#pragma once

#include "aria/context.hpp"

namespace Aria::Internal {
    
    void __aria_destruct_str(Context* ctx);
    void __aria_copy_str(Context* ctx);

} // namespace Aria::Internal