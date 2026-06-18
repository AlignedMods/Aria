#pragma once

#include "ariac/compilation_context.hpp"

#include <string_view>

namespace ariac {

    struct Decl;

    enum class SpecifierKind {
        Invalid = 0,

        Name
    };

    struct NameSpecifier {
        NameSpecifier(std::string_view identifier)
            : identifier(identifier) {}

        std::string_view identifier;
        Module* referenced_module = nullptr;
    };

    struct Specifier {
        template <typename T>
        static inline Specifier* Create(CompilationContext* ctx, SourceLoc loc, SpecifierKind kind, T t) { return ctx->allocate<Specifier>(loc, kind, t); }

        SpecifierKind kind = SpecifierKind::Invalid;

        SourceLoc loc;

        union {
            NameSpecifier name;
        };

        Specifier(SourceLoc loc, SpecifierKind kind, NameSpecifier name)
            : loc(loc), kind(kind), name(name) {}
    };

} // namespace ariac
