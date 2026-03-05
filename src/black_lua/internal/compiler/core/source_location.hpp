#pragma once

#include <cstddef>

namespace BlackLua::Internal {

    struct SourceLocation {
        size_t Line = 0;
        size_t Column = 0;

        SourceLocation() = default;
        SourceLocation(size_t line, size_t column)
            : Line(line), Column(column) {}

        inline bool IsValid() const { return Column != 0; }
    };

    struct SourceRange {
        SourceLocation Start;
        SourceLocation End;

        SourceRange() = default;
        SourceRange(SourceLocation start, SourceLocation end)
            : Start(start), End(end) {}
        SourceRange(size_t startLine, size_t startColumn, size_t endLine, size_t endColumn)
            : Start(startLine, startColumn), End(endLine, endColumn) {}

        inline bool IsValid() const { return Start.IsValid() && End.IsValid(); }
    };

} // namespace BlackLua::Internal
