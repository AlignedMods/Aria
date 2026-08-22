#pragma once

#include "ariac/core.hpp"
#include "ariac/core/vector.hpp"
#include "ariac/core/source_location.hpp"

namespace ariac {

    struct TypeInfo;

    // Info for a FunctionDecl which serves as a function specilization
    // The fields of this struct should only be used if 'is_specilization' is true
    struct FunctionSpecilizationInfo {
        TinyVector<TypeInfo*> types;
        SourceLoc instantiation_loc;
    };

    struct TemplateArgument {
        enum class Kind : u8 {
            Type
        };

        TemplateArgument(TypeInfo* type)
            : kind(Kind::Type), type(type) {}

        Kind kind;

        union {
            TypeInfo* type;
        };
    };

} // namespace ariac