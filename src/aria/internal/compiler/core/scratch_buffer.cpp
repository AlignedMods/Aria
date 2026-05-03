#include "aria/internal/compiler/core/scratch_buffer.hpp"
#include "aria/internal/compiler/compilation_context.hpp"
#include "aria/core.hpp"

#include <cassert>

namespace Aria::Internal {
    
    constexpr size_t SCRATCH_BUF_SIZE = 1024 * 1024;
    static char buffer[SCRATCH_BUF_SIZE];
    static size_t size = 0;

    void scratch_buffer_clear() {
        size = 0;
    }

    void scratch_buffer_append(char c) {
        ARIA_ASSERT(size + 1 < SCRATCH_BUF_SIZE, "Scratch buffer max size exceeded");
        buffer[size++] = c;
    }

    void scratch_buffer_append(std::string_view str) {
        ARIA_ASSERT(size + str.length() < SCRATCH_BUF_SIZE, "Scratch buffer max size exceeded");
        memcpy(buffer + size, str.data(), str.length());
        size += str.length();
    }

    bool scratch_buffer_cmp(std::string_view str) {
        if (size != str.length()) { return false; }
        return strncmp(buffer, str.data(), str.size()) == 0;
    }

    size_t scratch_buffer_size() {
        return size;
    }

    void scratch_buffer_size(size_t new_size) {
        size = new_size;
    }

    std::string_view scratch_buffer_to_str(CompilationContext* ctx) {
        char* buf = reinterpret_cast<char*>(ctx->allocate_sized(size));
        memcpy(buf, buffer, size);
        return std::string_view(buf, size);
    }

} // namespace Aria::Internal