#include "aria/internal/stdlib/string.hpp"
#include "aria/internal/runtime/types.hpp"

#include <cstring>

namespace Aria::Internal {

    void __aria_destruct_str(Context* ctx) {
        void* mem = ctx->GetPointer(-1);
        ctx->Pop(1);

        RuntimeString& str = *reinterpret_cast<RuntimeString*>(mem);
        delete[] str.RawData;
        str.RawData = nullptr;
        str.Size = 0;
    }

    void __aria_copy_str(Context* ctx) {
        void* mem = ctx->GetPointer(-1);
        ctx->Pop(1);

        RuntimeString& str = *reinterpret_cast<RuntimeString*>(mem);

        char* copiedData = new char[str.Size];
        memcpy(copiedData, str.RawData, str.Size);

        ctx->PushString(std::string_view(copiedData, str.Size));
    }

    void __aria_append_str(Context* ctx) {
        void* dst = ctx->GetPointer(-1);
        std::string_view src = ctx->GetString(-2);
        ctx->Pop(2);

        RuntimeString& dstStr = *reinterpret_cast<RuntimeString*>(dst);

        char* copiedData = new char[dstStr.Size + src.size()];
        memcpy(copiedData, dstStr.RawData, dstStr.Size);
        memcpy(copiedData + dstStr.Size, src.data(), src.size());
        dstStr.RawData = copiedData;
        dstStr.Size += src.size();
    }

} // namespace Aria::Internal