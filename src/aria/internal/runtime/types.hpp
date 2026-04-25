#pragma once

namespace Aria::Internal {
    
    struct RuntimeString {
        char* RawData = nullptr;
        size_t Size = 0;
    };

} // namespace Aria::Internal