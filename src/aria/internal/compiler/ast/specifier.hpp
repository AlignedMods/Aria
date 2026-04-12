#pragma once

#include "aria/internal/compiler/core/string_view.hpp"
#include "aria/internal/compiler/compilation_context.hpp"

namespace Aria::Internal {

    enum class SpecifierKind {
        Invalid = 0,

        Scope
    };

    struct ScopeSpecifier {
        ScopeSpecifier(StringView identifier)
            : Identifier(identifier) {}

        StringView Identifier;
        Module* ReferencedModule = nullptr;
    };

    struct Specifier {
        template <typename T>
        static inline Specifier* Create(CompilationContext* ctx, SourceLocation loc, SourceRange range, SpecifierKind kind, T t) { return ctx->Allocate<Specifier>(loc, range, kind, t); }

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
