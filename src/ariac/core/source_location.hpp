#pragma once

#include <cstddef>

namespace Aria::Internal {

    struct SourceLocation {
        size_t line = 0;
        size_t column = 0;

        SourceLocation() = default;
        SourceLocation(size_t line, size_t column)
            : line(line), column(column) {}

        bool is_valid() { return column != 0; }
    };

    struct SourceRange {
        SourceLocation start;
        SourceLocation end;

        SourceRange() = default;
        SourceRange(SourceLocation start, SourceLocation end)
            : start(start), end(end) {}
        SourceRange(size_t sl, size_t sc, size_t el, size_t ec)
            : start(sl, sc), end(el, ec) {}
    };

} // namespace Aria::Internal
