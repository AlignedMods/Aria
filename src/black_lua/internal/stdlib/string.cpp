#include "black_lua/internal/stdlib/string.hpp"
#include "black_lua/core.hpp"

namespace BlackLua::Internal {

    struct String {
        int8_t* DynamicBuffer = nullptr;
        
        int32_t Size = 0;
        int32_t Capacity = 0;
    };

    void bl__string__construct__(Context* ctx) {
        // String* str = new String();
        // 
        // str->Size = 0;
        // str->Capacity = 1;
        // str->DynamicBuffer = new int8_t[1];
        // 
        // ctx->StorePointer(-1, str);
        BLUA_ASSERT(false, "TODO");
    }

    void bl__string__construct_from_literal__(Context* ctx) {
        // StackSlot s = ctx->GetVM().GetStackSlot(-2);
        // 
        // String* str = new String();
        // str->Capacity = s.Size;
        // str->DynamicBuffer = new int8_t[str->Capacity];
        // 
        // memcpy(str->DynamicBuffer, s.Memory, s.Size);
        // 
        // ctx->GetVM().StorePointer(-1, str);
        BLUA_ASSERT(false, "TODO");
    }

    void bl__string__copy__(Context* ctx) {
        // String* str = reinterpret_cast<String*>(ctx->GetVM().GetPointer(-2));
        // 
        // String* newStr = new String();
        // newStr->Capacity = str->Capacity;
        // newStr->Size = str->Size;
        // newStr->DynamicBuffer = new int8_t[newStr->Capacity];
        // 
        // memcpy(newStr->DynamicBuffer, str->DynamicBuffer, str->Size);
        // 
        // ctx->GetVM().StorePointer(-1, newStr);
        BLUA_ASSERT(false, "TODO");
    }

    void bl__string__assign__(Context* ctx) {
        // String* str = reinterpret_cast<String*>(ctx->GetVM().GetPointer(-3));
        // String* other = reinterpret_cast<String*>(ctx->GetVM().GetPointer(-2));
        // delete[] str->DynamicBuffer;
        // 
        // str->Capacity = other->Capacity;
        // str->Size = other->Size;
        // str->DynamicBuffer = new int8_t[str->Capacity];
        // 
        // memcpy(str->DynamicBuffer, other->DynamicBuffer, other->Size);
        // 
        // ctx->GetVM().StorePointer(-1, str);
        BLUA_ASSERT(false, "TODO");
    }

    void bl__string__destruct__(Context* ctx) {
        // String* str = reinterpret_cast<String*>(ctx->GetVM().GetPointer(-1));
        // delete[] str->DynamicBuffer;
        // 
        // str->Size = 0;
        // str->Capacity = 0;
        // 
        // delete str;
        // ctx->GetVM().StorePointer(-1, nullptr);
        BLUA_ASSERT(false, "TODO");
    }

} // namespace BlackLua::Internal
