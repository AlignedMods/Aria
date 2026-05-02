#include "aria/internal/compiler/types/type_info.hpp"

namespace Aria::Internal {

    TypeInfo* TypeInfo::Create(CompilationContext* ctx, TypeKind kind, bool isReference) {
        TypeInfo* t = ctx->allocate<TypeInfo>();
        t->Kind = kind;
        t->Reference = isReference;
    
        return t;
    }

    TypeInfo* TypeInfo::Dup(CompilationContext* ctx, TypeInfo* type) {
        TypeInfo* t = ctx->allocate<TypeInfo>();
        memcpy(reinterpret_cast<void*>(t), type, sizeof(TypeInfo));
        return t;
    }

} // namespace Aria::Internal 
