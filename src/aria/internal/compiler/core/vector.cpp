#include "aria/internal/compiler/core/vector.hpp"
#include "aria/internal/compiler/compilation_context.hpp"

namespace Aria::Internal {
    
    void* alloc_arena(CompilationContext* ctx, size_t size) { return ctx->allocate_sized(size); }

} // namespace Aria::Internal

