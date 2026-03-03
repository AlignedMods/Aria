#pragma once

#include "black_lua/core.hpp"
#include "black_lua/internal/compiler/core/string_view.hpp"
#include "black_lua/internal/compiler/compilation_context.hpp"
#include "black_lua/internal/compiler/core/vector.hpp"

#include <variant>
#include <vector>

namespace BlackLua::Internal {

    enum class PrimitiveType {
        Invalid = 0,
        Void,
        Bool,
        Char,
        Short,
        Int,
        Long,
        Float,
        Double,
        StringLiteral,

        String,
        Array,
        Structure
    };

    struct TypeInfo;

    struct ArrayDeclaration {
        TypeInfo* Type = nullptr;
    };

    struct StructFieldDeclaration {
        StringView Identifier;
        size_t Offset = 0;

        TypeInfo* ResolvedType = nullptr;
    };

    struct StructDeclaration {
        StringView Identifier;
        TinyVector<StructFieldDeclaration> Fields;

        size_t Size = 0;
    };

    struct TypeInfo {
        PrimitiveType Type = PrimitiveType::Invalid;
        std::variant<bool, size_t, TypeInfo*, ArrayDeclaration, StructDeclaration> Data;

        static TypeInfo* Create(CompilationContext* ctx, PrimitiveType type, decltype(TypeInfo::Data) data = {});

        static bool IsEqual(TypeInfo* lhs, TypeInfo* rhs) {
            if (lhs->Type != rhs->Type) { return false; }

            if (lhs->IsIntegral() && rhs->IsIntegral()) {
                return lhs->IsSigned() == rhs->IsSigned();
            }

            return true;
        }

        bool IsIntegral() const {
            return Type == PrimitiveType::Char || Type == PrimitiveType::Short || Type == PrimitiveType::Int || Type == PrimitiveType::Long;
        }

        bool IsFloatingPoint() const {
            return Type == PrimitiveType::Float || Type == PrimitiveType::Double;
        }

        bool IsSigned() const {
            if (!IsIntegral()) {
                return false;
            }

            return std::get<bool>(Data);
        }

        size_t GetSize() const {
            switch (Type) {
                case PrimitiveType::Void: return 0;
                case PrimitiveType::Bool: return 1;
                case PrimitiveType::Char: return 1;
                case PrimitiveType::Short: return 2;
                case PrimitiveType::Int: return 4;
                case PrimitiveType::Float: return 4;
                case PrimitiveType::Long: return 8;
                case PrimitiveType::Double: return 8;

                case PrimitiveType::StringLiteral: {
                    size_t size = std::get<size_t>(Data);
                    return size;
                }

                case PrimitiveType::String: {
                    return sizeof(void*);
                }

                case PrimitiveType::Array: {
                    return sizeof(void*);
                }

                case PrimitiveType::Structure: {
                    StructDeclaration decl = std::get<StructDeclaration>(Data);
                    return decl.Size;
                }

                default: BLUA_UNREACHABLE();
            }

            return 0;
        }
    };

    inline std::string TypeInfoToString(TypeInfo* type) {
        std::string str;

        if (!type->IsSigned() && type->IsIntegral()) {
            str = "u";
        }

        switch (type->Type) {
            case PrimitiveType::Invalid: str += "invalid"; break;
            case PrimitiveType::Void:    str += "void"; break;
            case PrimitiveType::Bool:    str += "bool"; break;
            case PrimitiveType::Char:    str += "char"; break;
            case PrimitiveType::Short:   str += "short"; break;
            case PrimitiveType::Int:     str += "int"; break;
            case PrimitiveType::Long:    str += "long"; break;
            case PrimitiveType::Float:   str += "float"; break;
            case PrimitiveType::Double:  str += "double"; break;
            case PrimitiveType::String:  str += "string"; break;

            case PrimitiveType::Array: {
                ArrayDeclaration decl = std::get<ArrayDeclaration>(type->Data);

                str = fmt::format("{}[]", TypeInfoToString(decl.Type));
                break;
            }

            case PrimitiveType::Structure: {
                StructDeclaration decl = std::get<StructDeclaration>(type->Data);

                str = fmt::format("struct {}", decl.Identifier);
                break;
            }
        }

        return str;
    }

} // namespace BlackLua::Internal
