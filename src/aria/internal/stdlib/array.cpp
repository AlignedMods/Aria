#include "aria/internal/stdlib/array.hpp"
#include "aria/core.hpp"

namespace Aria::Internal {

    struct Array {
        uint8_t* Data = nullptr;
        int32_t MemberSize = 0;

        int32_t Size = 0;
        int32_t Capacity = 0;
    };

    void bl__array__init__(Context* ctx) {
        Array* arr = new Array();
        int32_t size = ctx->GetInt(-2);
        
        arr->Size = 0;
        arr->Capacity = 1;
        arr->MemberSize = size;
        arr->Data = new uint8_t[size];
        
        ctx->StorePointer(-1, arr);
    }

    void bl__array__copy__(Context* ctx) {
        // Array* dst = reinterpret_cast<Array*>(ctx->GetVM().GetPointer(-2));
        // Array* src = reinterpret_cast<Array*>(ctx->GetVM().GetPointer(-1));
        // 
        // dst->Size = src->Size;
        // dst->MemberSize = src->MemberSize;
        // dst->Data = new uint8_t[dst->Size * dst->MemberSize];
        // memcpy(dst->Data, src->Data, dst->Size * dst->MemberSize);

        ARIA_ASSERT(false, "TODO");
    }

    void bl__array__destruct__(Context* ctx) {
        Array* arr = reinterpret_cast<Array*>(ctx->GetPointer(-1));
        
        delete[] arr->Data;
        arr->Size = 0;
        arr->Capacity = 0;
        
        delete arr;
        ctx->StorePointer(-1, nullptr);
    }

    void bl__array__append__(Context* ctx) {
        Array* arr = reinterpret_cast<Array*>(ctx->GetPointer(-2));
        // StackSlot slot = ctx->GetStackSlot(-1);

        // ARIA_ASSERT(slot.Size == arr->MemberSize, "Invalid Array::Append argument");
        // 
        // if (arr->Size + 1 >= arr->Capacity) {
        //     arr->Capacity *= 2;
        //     uint8_t* newData = new uint8_t[arr->Capacity * arr->MemberSize];
        //     memcpy(newData, arr->Data, arr->Size * arr->MemberSize);
        //     delete[] arr->Data;
        //     arr->Data = newData;
        // }
        // 
        // memcpy(arr->Data + arr->Size * arr->MemberSize, slot.Memory, arr->MemberSize);
        // arr->Size++;

        ARIA_ASSERT(false, "todo!");
    }

    void bl__array__index__(Context* ctx) {
        Array* arr = reinterpret_cast<Array*>(ctx->GetPointer(-3));
        int32_t index = ctx->GetInt(-2);
        
        if (index >= arr->Size) {
            ARIA_ASSERT(false, "TODO: runtime error");
        }

        // StackSlot retSlot = ctx->GetStackSlot(-1);
        // memcpy(retSlot.Memory, &arr->Data[index * arr->MemberSize], arr->MemberSize);
        ARIA_ASSERT(false, "todo!");
    }

} // namespace Aria::Internal
