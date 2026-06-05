#include "ariac/core/vector.hpp"
#include "ariac/compilation_context.hpp"

namespace ariac {
    
    void* alloc_arena(CompilationContext* ctx, size_t size) { return ctx->allocate_sized(size); }

} // namespace ariac

