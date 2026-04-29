#pragma once

namespace Aria::Internal {
    
    struct RuntimeSlice {
        void* Mem = nullptr;
        size_t Size = 0;
    };

} // namespace Aria::Internal