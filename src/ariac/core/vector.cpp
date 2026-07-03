#include "ariac/core/vector.hpp"
#include "ariac/compilation_context.hpp"

namespace ariac {
    
    void* alloc_arena(size_t size) { return context.allocate_sized(size); }

} // namespace ariac

