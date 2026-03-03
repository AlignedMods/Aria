#pragma once

#include "black_lua/core.hpp"

#include <cstring>

namespace BlackLua::Internal {

    class StringView {
    public:
        inline static constexpr size_t npos = SIZE_MAX;

        StringView() = default;

        inline StringView(const char* str)
            : m_Str(str), m_Size(std::strlen(str)) {}

        inline StringView(const char* str, size_t size)
            : m_Str(str), m_Size(size) {}

        inline StringView(const std::string& str)
            : m_Str(str.data()), m_Size(str.size()) {}

        inline StringView(const StringView& other)
            : m_Str(other.m_Str), m_Size(other.m_Size) {}

        inline void operator=(const char* str) {
            m_Str = str;
            m_Size = std::strlen(str);
        }

        inline bool operator==(const StringView other) {
            if (m_Size != other.m_Size) { return false; }
            return std::strncmp(m_Str, other.m_Str, m_Size) == 0;
        }

        inline size_t Size() const { return m_Size; }
        inline const char* Data() const { return m_Str; }

        inline char At(size_t index) {
            BLUA_ASSERT(index < m_Size, "StringView::At() out of bounds!");
            return m_Str[index];
        }

        inline size_t Find(char c) {
            for (size_t i = 0; i < m_Size; i++) {
                if (m_Str[i] == c) {
                    return i;
                }
            }

            return npos;
        }

        inline StringView SubStr(size_t start, size_t end) {
            return StringView(m_Str + start, end - start);
        }

    private:
        const char* m_Str = nullptr;
        size_t m_Size = 0;
    };

} // namespace BlackLua::Internal

template <>
struct fmt::formatter<BlackLua::Internal::StringView> : formatter<string_view> {

    inline auto format(const BlackLua::Internal::StringView& view, format_context& ctx) const -> format_context::iterator {
        return formatter<string_view>::format(string_view(view.Data(), view.Size()), ctx);
    }

};
