#include "aria/internal/compiler/types/type_info.hpp"

namespace Aria::Internal {

    TypeInfo* TypeInfo::Create(CompilationContext* ctx, TypeKind kind, bool is_reference) {
        TypeInfo* t = ctx->allocate<TypeInfo>();
        t->kind = kind;
        t->reference = is_reference;
    
        return t;
    }

    TypeInfo* TypeInfo::Dup(CompilationContext* ctx, TypeInfo* type) {
        TypeInfo* t = ctx->allocate<TypeInfo>();
        memcpy(reinterpret_cast<void*>(t), type, sizeof(TypeInfo));
        return t;
    }

} // namespace Aria::Internal 
