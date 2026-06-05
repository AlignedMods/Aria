#pragma once

#include <unordered_map>
#include <string>

namespace ariac {

    enum class ReflectionKind {
        Variable,
        Function
    };

    struct CompilerReflectionDeclaration {
        size_t type_index = 0;
        ReflectionKind kind = ReflectionKind::Variable;
    };

    struct CompilerReflectionData {
        std::unordered_map<std::string, CompilerReflectionDeclaration> declarations;
    };

} // namespace ariac
