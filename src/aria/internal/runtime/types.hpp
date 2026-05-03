#pragma once

#include "aria/internal/types.hpp"

namespace Aria::Internal {
    
    struct RuntimeSlice {
        void* mem = nullptr;
        u64 size = 0;
    };

    static_assert(sizeof(RuntimeSlice) == 16, "Invalid size of RuntimeSlice");
    static_assert(offsetof(RuntimeSlice, mem) == 0, "Invalid offset of RuntimeSlice::mem");
    static_assert(offsetof(RuntimeSlice, size) == 8, "Invalid offset of RuntimeSlice::size");

} // namespace Aria::Internal