#pragma once

#include "ariac/compilation_context.hpp"

#include <string_view>

namespace Aria::Internal {

    enum class SpecifierKind {
        Invalid = 0,

        Scope
    };

    struct ScopeSpecifier {
        ScopeSpecifier(std::string_view identifier)
            : identifier(identifier) {}

        std::string_view identifier;
        Module* referenced_module = nullptr;
    };

    struct Specifier {
        template <typename T>
        static inline Specifier* Create(CompilationContext* ctx, SourceLocation loc, SourceRange range, SpecifierKind kind, T t) { return ctx->allocate<Specifier>(loc, range, kind, t); }

        SpecifierKind kind = SpecifierKind::Invalid;

        SourceLocation loc;
        SourceRange range;

        union {
            ScopeSpecifier scope;
        };

        Specifier(SourceLocation loc, SourceRange range, SpecifierKind kind, ScopeSpecifier scope)
            : loc(loc), range(range), kind(kind), scope(scope) {}
    };

} // namespace Aria::Internal
