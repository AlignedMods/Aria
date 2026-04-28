#include "aria/internal/stdlib/string.hpp"
#include "aria/internal/runtime/types.hpp"

#include "fmt/format.h"

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

    void __aria_append_str_char(Context* ctx) {
        void* dst = ctx->GetPointer(-1);
        int8_t c = ctx->GetChar(-2);
        ctx->Pop(2);

        RuntimeString& dstStr = *reinterpret_cast<RuntimeString*>(dst);

        char* copiedData = new char[dstStr.Size + 1];
        memcpy(copiedData, dstStr.RawData, dstStr.Size);
        copiedData[dstStr.Size] = c;
        delete[] dstStr.RawData;

        dstStr.RawData = copiedData;
        dstStr.Size += 1;
    }

    void __aria_append_str_uchar(Context* ctx) {
        void* dst = ctx->GetPointer(-1);
        uint8_t c = ctx->GetUChar(-2);
        ctx->Pop(2);

        RuntimeString& dstStr = *reinterpret_cast<RuntimeString*>(dst);

        std::string s = fmt::format("{}", c);

        char* copiedData = new char[dstStr.Size + s.size()];
        memcpy(copiedData, dstStr.RawData, dstStr.Size);
        memcpy(copiedData + dstStr.Size, s.data(), s.size());
        delete[] dstStr.RawData;

        dstStr.RawData = copiedData;
        dstStr.Size += s.size();
    }

    void __aria_append_str_short(Context* ctx) {
        void* dst = ctx->GetPointer(-1);
        int16_t i = ctx->GetShort(-2);
        ctx->Pop(2);

        RuntimeString& dstStr = *reinterpret_cast<RuntimeString*>(dst);

        std::string s = fmt::format("{}", i);

        char* copiedData = new char[dstStr.Size + s.size()];
        memcpy(copiedData, dstStr.RawData, dstStr.Size);
        memcpy(copiedData + dstStr.Size, s.data(), s.size());
        delete[] dstStr.RawData;

        dstStr.RawData = copiedData;
        dstStr.Size += s.size();
    }

    void __aria_append_str_ushort(Context* ctx) {
        void* dst = ctx->GetPointer(-1);
        uint16_t i = ctx->GetUShort(-2);
        ctx->Pop(2);

        RuntimeString& dstStr = *reinterpret_cast<RuntimeString*>(dst);

        std::string s = fmt::format("{}", i);

        char* copiedData = new char[dstStr.Size + s.size()];
        memcpy(copiedData, dstStr.RawData, dstStr.Size);
        memcpy(copiedData + dstStr.Size, s.data(), s.size());
        delete[] dstStr.RawData;

        dstStr.RawData = copiedData;
        dstStr.Size += s.size();
    }

    void __aria_append_str_int(Context* ctx) {
        void* dst = ctx->GetPointer(-1);
        int32_t i = ctx->GetInt(-2);
        ctx->Pop(2);

        RuntimeString& dstStr = *reinterpret_cast<RuntimeString*>(dst);

        std::string s = fmt::format("{}", i);

        char* copiedData = new char[dstStr.Size + s.size()];
        memcpy(copiedData, dstStr.RawData, dstStr.Size);
        memcpy(copiedData + dstStr.Size, s.data(), s.size());
        delete[] dstStr.RawData;

        dstStr.RawData = copiedData;
        dstStr.Size += s.size();
    }

    void __aria_append_str_uint(Context* ctx) {
        void* dst = ctx->GetPointer(-1);
        uint32_t i = ctx->GetUInt(-2);
        ctx->Pop(2);

        RuntimeString& dstStr = *reinterpret_cast<RuntimeString*>(dst);

        std::string s = fmt::format("{}", i);

        char* copiedData = new char[dstStr.Size + s.size()];
        memcpy(copiedData, dstStr.RawData, dstStr.Size);
        memcpy(copiedData + dstStr.Size, s.data(), s.size());
        delete[] dstStr.RawData;

        dstStr.RawData = copiedData;
        dstStr.Size += s.size();
    }

    void __aria_append_str_long(Context* ctx) {
        void* dst = ctx->GetPointer(-1);
        int64_t i = ctx->GetLong(-2);
        ctx->Pop(2);

        RuntimeString& dstStr = *reinterpret_cast<RuntimeString*>(dst);

        std::string s = fmt::format("{}", i);

        char* copiedData = new char[dstStr.Size + s.size()];
        memcpy(copiedData, dstStr.RawData, dstStr.Size);
        memcpy(copiedData + dstStr.Size, s.data(), s.size());
        delete[] dstStr.RawData;

        dstStr.RawData = copiedData;
        dstStr.Size += s.size();
    }

    void __aria_append_str_ulong(Context* ctx) {
        void* dst = ctx->GetPointer(-1);
        uint64_t i = ctx->GetULong(-2);
        ctx->Pop(2);

        RuntimeString& dstStr = *reinterpret_cast<RuntimeString*>(dst);

        std::string s = fmt::format("{}", i);

        char* copiedData = new char[dstStr.Size + s.size()];
        memcpy(copiedData, dstStr.RawData, dstStr.Size);
        memcpy(copiedData + dstStr.Size, s.data(), s.size());
        delete[] dstStr.RawData;

        dstStr.RawData = copiedData;
        dstStr.Size += s.size();
    }

    void __aria_append_str_float(Context* ctx) {
        void* dst = ctx->GetPointer(-1);
        float f = ctx->GetFloat(-2);
        ctx->Pop(2);

        RuntimeString& dstStr = *reinterpret_cast<RuntimeString*>(dst);

        std::string s = fmt::format("{}", f);

        char* copiedData = new char[dstStr.Size + s.size()];
        memcpy(copiedData, dstStr.RawData, dstStr.Size);
        memcpy(copiedData + dstStr.Size, s.data(), s.size());
        delete[] dstStr.RawData;

        dstStr.RawData = copiedData;
        dstStr.Size += s.size();
    }

    void __aria_append_str_double(Context* ctx) {
        void* dst = ctx->GetPointer(-1);
        double d = ctx->GetDouble(-2);
        ctx->Pop(2);

        RuntimeString& dstStr = *reinterpret_cast<RuntimeString*>(dst);

        std::string s = fmt::format("{}", d);

        char* copiedData = new char[dstStr.Size + s.size()];
        memcpy(copiedData, dstStr.RawData, dstStr.Size);
        memcpy(copiedData + dstStr.Size, s.data(), s.size());
        delete[] dstStr.RawData;

        dstStr.RawData = copiedData;
        dstStr.Size += s.size();
    }

    void __aria_append_str_string(Context* ctx) {
        void* dst = ctx->GetPointer(-1);
        std::string_view str = ctx->GetString(-2);
        ctx->Pop(2);

        RuntimeString& dstStr = *reinterpret_cast<RuntimeString*>(dst);

        char* copiedData = new char[dstStr.Size + str.size()];
        memcpy(copiedData, dstStr.RawData, dstStr.Size);
        memcpy(copiedData + dstStr.Size, str.data(), str.size());
        delete[] dstStr.RawData;

        dstStr.RawData = copiedData;
        dstStr.Size += str.size();
    }

} // namespace Aria::Internal