#pragma once

namespace Aria::Internal {
    
    struct RuntimeString {
        const char* RawData = nullptr;
        size_t Size = 0;
    };

} // namespace Aria::Internal