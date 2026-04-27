#pragma once

#include "aria/internal/compiler/compilation_context.hpp"
#include "aria/internal/compiler/core/string_view.hpp"

#include <cstring>

namespace Aria::Internal {

    class StringBuilder {
    public:
        StringBuilder() = default;

        inline bool operator==(const StringBuilder other) {
            if (m_Size != other.m_Size) { return false; }
            return std::strncmp(m_Str, other.m_Str, m_Size) == 0;
        }

        inline bool operator==(const StringView other) {
            if (m_Size != other.Size()) { return false; }
            return std::strncmp(m_Str, other.Data(), m_Size) == 0;
        }

        inline size_t Size() const { return m_Size; }
        inline const char* Data() const { return m_Str; }

        inline void Clear() { m_Size = 0; }

        inline void Append(CompilationContext* ctx, const StringView str) {
            m_Capacity += str.Size();
            char* newStr = reinterpret_cast<char*>(ctx->AllocateSized(m_Capacity));
            if (m_Str) {
                memcpy(newStr, m_Str, m_Size);
            }
            
            for (size_t i = 0; i < str.Size(); i++) {
                newStr[m_Size + i] = *(str.Data() + i);
            }

            m_Str = newStr;
            m_Size += str.Size();
        }

        inline void Append(CompilationContext* ctx, char c) {
            m_Size += 1;

            if (m_Size >= m_Capacity) {
                if (m_Capacity == 0) {
                    m_Capacity = 16;
                } else {
                    m_Capacity *= 2;
                }

                char* newStr = reinterpret_cast<char*>(ctx->AllocateSized(m_Capacity));
                if (m_Str) {
                    memcpy(newStr, m_Str, m_Size);
                }

                m_Str = newStr;
            }
            
            m_Str[m_Size - 1] = c;
        }

        operator StringView() {
            return StringView(m_Str, m_Size);
        }

    private:
        char* m_Str = nullptr;
        size_t m_Capacity = 0;
        size_t m_Size = 0;
    };

} // namespace Aria::Internal

template <> struct fmt::formatter<Aria::Internal::StringBuilder> : formatter<string_view> {

    inline auto format(Aria::Internal::StringBuilder builder, format_context& ctx) const -> format_context::iterator {
        return formatter<string_view>::format(string_view(builder.Data(), builder.Size()), ctx);
    }

};
