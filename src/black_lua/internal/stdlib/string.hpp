#pragma once

#include "black_lua/context.hpp"

namespace BlackLua::Internal {

    void bl__string__construct__(Context* ctx);
    void bl__string__construct_from_literal__(Context* ctx);

    void bl__string__copy__(Context* ctx);
    void bl__string__assign__(Context* ctx);
    void bl__string__assign_literal__(Context* ctx);

    void bl__string__destruct__(Context* ctx);


} // namespace BlackLua::Internal
