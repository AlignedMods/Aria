#pragma once

#include "black_lua/internal/compiler/types/type_info.hpp"

#include <unordered_map>
#include <string>

namespace BlackLua::Internal {

    enum class ReflectionType {
        Variable,
        Function
    };

    struct CompilerReflectionDeclaration {
        TypeInfo* ResolvedType = nullptr;
        ReflectionType Type = ReflectionType::Variable;
        std::variant<int32_t> Data;
    };

    struct CompilerReflectionData {
        std::unordered_map<std::string, CompilerReflectionDeclaration> Declarations;
    };

} // namespace BlackLua::Internal
