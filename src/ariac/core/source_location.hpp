#pragma once

#include <cstddef>

namespace ariac {

    struct SourceLoc {
        size_t line = 0;
        size_t col = 0;
        size_t offset = 0;
        size_t len = 0;

        SourceLoc() = default;

        SourceLoc(size_t line, size_t col, size_t offset)
            : line(line), col(col), offset(offset), len(0) {}

        SourceLoc(size_t line, size_t col, size_t offset, size_t len)
            : line(line), col(col), offset(offset), len(len) {}

        bool is_valid() const { return len != 0; }
    };

    inline SourceLoc extend_source_loc(const SourceLoc& lhs, const SourceLoc& rhs) {
        SourceLoc new_loc = lhs;
	    new_loc.len = rhs.offset + rhs.len - lhs.offset;
	    return new_loc;
    }

    inline SourceLoc operator+(const SourceLoc& lhs, const SourceLoc& rhs) { return extend_source_loc(lhs, rhs); }
    inline SourceLoc& operator+=(SourceLoc& lhs, const SourceLoc& rhs) { return lhs = extend_source_loc(lhs, rhs); }

} // namespace ariac
