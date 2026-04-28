#include "aria/internal/compiler/types/type_info.hpp"

namespace Aria::Internal {

    TypeInfo* TypeInfo::Create(CompilationContext* ctx, PrimitiveType type, bool isReference, decltype(TypeInfo::Data) data) {
        TypeInfo* t = ctx->Allocate<TypeInfo>();
        t->Type = type;
        t->Data = data;
        t->Reference = isReference;
    
        return t;
    }

    TypeInfo* TypeInfo::Dup(CompilationContext* ctx, TypeInfo* type) {
        TypeInfo* t = ctx->Allocate<TypeInfo>();
        memcpy(reinterpret_cast<void*>(t), type, sizeof(TypeInfo));
        return t;
    }

} // namespace Aria::Internal 
