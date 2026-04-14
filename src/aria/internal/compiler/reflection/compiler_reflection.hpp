#pragma once

#include "aria/internal/vm/op_codes.hpp"

#include <unordered_map>
#include <string>

namespace Aria::Internal {

    enum class ReflectionKind {
        Variable,
        Function
    };

    struct CompilerReflectionDeclaration {
        size_t TypeIndex = 0;
        ReflectionKind Kind = ReflectionKind::Variable;
    };

    struct CompilerReflectionData {
        std::unordered_map<std::string, CompilerReflectionDeclaration> Declarations;
    };

} // namespace Aria::Internal
