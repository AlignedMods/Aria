#pragma once

#include <ariac/ast/decl.hpp>

namespace ariac {

    // NOTE: This struct uses the arena allocator
    struct MangleContext {
    public:
        MangleContext(Decl* decl)
            : decl(decl) {}

        MangleContext(TypeInfo* type)
            : type(type) {}

        std::string_view mangle();

    private:
        void mangle_module(Module* mod);
        void mangle_decl(Decl* decl);
        void mangle_type(TypeInfo* type);

    private:
        Decl* decl = nullptr;
        TypeInfo* type = nullptr;
    };

} // namespace ariac