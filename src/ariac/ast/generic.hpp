#pragma once

#include "ariac/core.hpp"

namespace ariac {

    struct TypeInfo;

    struct GenericRequirement {
        GenericRequirement(TypeInfo* arg = nullptr)
            : arg(arg) {}

        TypeInfo* arg;
    };

} // namespace ariac