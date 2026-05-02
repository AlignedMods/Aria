#pragma once

#include "aria/internal/compiler/compilation_context.hpp"

#include <string_view>

namespace Aria::Internal {

    enum class SpecifierKind {
        Invalid = 0,

        Scope
    };

    struct ScopeSpecifier {
        ScopeSpecifier(std::string_view identifier)
            : Identifier(identifier) {}

        std::string_view Identifier;
        Module* ReferencedModule = nullptr;
    };

    struct Specifier {
        template <typename T>
        static inline Specifier* Create(CompilationContext* ctx, SourceLocation loc, SourceRange range, SpecifierKind kind, T t) { return ctx->allocate<Specifier>(loc, range, kind, t); }

        SpecifierKind Kind = SpecifierKind::Invalid;

        SourceLocation Loc;
        SourceRange Range;

        union {
            ScopeSpecifier Scope;
        };

        Specifier(SourceLocation loc, SourceRange range, SpecifierKind kind, ScopeSpecifier scope)
            : Loc(loc), Range(range), Kind(kind), Scope(scope) {}
    };

} // namespace Aria::Internal
