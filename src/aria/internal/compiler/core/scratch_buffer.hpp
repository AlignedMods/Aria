#pragma once

#include <string_view>

namespace Aria::Internal {

    struct CompilationContext;

    void scratch_buffer_clear();
    void scratch_buffer_append(char c);
    void scratch_buffer_append(std::string_view str);
    bool scratch_buffer_cmp(std::string_view str);
    size_t scratch_buffer_size();
    void scratch_buffer_size(size_t new_size);
    std::string_view scratch_buffer_to_str(CompilationContext* ctx);

}