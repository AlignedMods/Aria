#pragma once

#include "ariac/core.hpp"

namespace ariac {

    struct TypeInfo;

    enum class GenericRequirementKind : u8 {
        Integral,
        FloatingPoint,
        ConvertibleTo
    };

    inline const char* generic_requirement_kind_to_string(GenericRequirementKind req) {
        switch (req) {
            case GenericRequirementKind::Integral: return "@Integral";
            case GenericRequirementKind::FloatingPoint: return "@FloatingPoint";
            case GenericRequirementKind::ConvertibleTo: return "@ConvertibleTo";

            default: ARIA_UNREACHABLE("Invalid generic requirement");
        }
    }

    struct GenericRequirement {
        GenericRequirement(GenericRequirementKind kind, TypeInfo* arg = nullptr)
            : kind(kind), arg(arg) {}

        GenericRequirementKind kind;
        TypeInfo* arg;
    };

} // namespace ariac