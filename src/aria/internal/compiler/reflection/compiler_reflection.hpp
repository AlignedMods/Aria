#pragma once

#include <unordered_map>
#include <string>

namespace Aria::Internal {

    struct TypeInfo;

    enum class ReflectionType {
        Variable,
        Function
    };

    struct CompilerReflectionDeclaration {
        TypeInfo* ResolvedType = nullptr;
        size_t ResolvedTypeSize = 0;
        ReflectionType Type = ReflectionType::Variable;
    };

    struct CompilerReflectionData {
        std::unordered_map<std::string, CompilerReflectionDeclaration> Declarations;
    };

} // namespace Aria::Internal
