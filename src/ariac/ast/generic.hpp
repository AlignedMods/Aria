#pragma once

#include "ariac/core.hpp"

namespace ariac {

    struct TypeInfo;

    // Info for a FunctionDecl which serves as a function specilization
    // The fields of this struct should only be used if 'is_specilization' is true
    struct FunctionSpecilizationInfo {
        TinyVector<TypeInfo*> types;
        SourceLoc instantiation_loc;
    };

    struct GenericRequirement {
        GenericRequirement(TypeInfo* arg = nullptr)
            : arg(arg) {}

        TypeInfo* arg;
    };

} // namespace ariac