#include "black_lua/internal/compiler/types/type_info.hpp"

namespace BlackLua::Internal {

    TypeInfo* TypeInfo::Create(CompilationContext* ctx, PrimitiveType type, decltype(TypeInfo::Data) data) {
        TypeInfo* t = ctx->Allocate<TypeInfo>();
        t->Type = type;
        t->Data = data;
    
        return t;
    }

} // namespace BlackLua::Internal 
