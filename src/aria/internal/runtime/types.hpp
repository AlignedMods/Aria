#pragma once

#include "aria/internal/types.hpp"

namespace Aria::Internal {
    
    struct RuntimeSlice {
        void* Mem = nullptr;
        u64 Size = 0;
    };

    static_assert(sizeof(RuntimeSlice) == 16, "Invalid size of RuntimeSlice");
    static_assert(offsetof(RuntimeSlice, Mem) == 0, "Invalid offset of RuntimeSlice::Mem");
    static_assert(offsetof(RuntimeSlice, Size) == 8, "Invalid offset of RuntimeSlice::Size");

} // namespace Aria::Internal