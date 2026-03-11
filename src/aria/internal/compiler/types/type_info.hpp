#pragma once

#include "aria/core.hpp"
#include "aria/internal/compiler/core/string_view.hpp"
#include "aria/internal/compiler/compilation_context.hpp"
#include "aria/internal/compiler/core/vector.hpp"

#include <variant>
#include <vector>

namespace Aria::Internal {

    enum class PrimitiveType {
        Invalid = 0,
        Void,

        Bool,

        Char, UChar,
        Short, UShort,
        Int, UInt,
        Long, ULong,

        Float,
        Double,

        Function,

        StringLiteral,

        String,
        Array,
        Structure
    };

    struct TypeInfo;

    struct FunctionDeclaration {
        TypeInfo* ReturnType = nullptr;
        TinyVector<TypeInfo*> ParamTypes;
        bool External = false;
    };

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
        std::variant<size_t, TypeInfo*, FunctionDeclaration, ArrayDeclaration, StructDeclaration> Data;

        static TypeInfo* Create(CompilationContext* ctx, PrimitiveType type, decltype(TypeInfo::Data) data = {});

        static bool IsEqual(TypeInfo* lhs, TypeInfo* rhs) {
            if (lhs->Type != rhs->Type) { return false; }

            return true;
        }

        bool IsIntegral() const {
            return Type == PrimitiveType::Char || Type == PrimitiveType::UChar ||
                Type == PrimitiveType::Short || Type == PrimitiveType::UShort ||
                Type == PrimitiveType::Int || Type == PrimitiveType::UInt ||
                Type == PrimitiveType::Long || Type == PrimitiveType::ULong;
        }

        bool IsFloatingPoint() const {
            return Type == PrimitiveType::Float || Type == PrimitiveType::Double;
        }

        bool IsSigned() const {
            ARIA_ASSERT(IsIntegral(), "IsSigned() cannot operate on a non-integral type");
            return Type == PrimitiveType::Char || Type == PrimitiveType::Short || Type == PrimitiveType::Int || Type == PrimitiveType::Long;
        }

        bool IsUnsigned() const {
            ARIA_ASSERT(IsIntegral(), "IsUnsigned() cannot operate on a non-integral type");
            return Type == PrimitiveType::UChar || Type == PrimitiveType::UShort || Type == PrimitiveType::UInt || Type == PrimitiveType::ULong;
        }

        size_t GetSize() const {
            switch (Type) {
                case PrimitiveType::Void: return 0;

                case PrimitiveType::Bool: return 1;

                case PrimitiveType::Char:
                case PrimitiveType::UChar: return 1;
                case PrimitiveType::Short:
                case PrimitiveType::UShort: return 2;
                case PrimitiveType::Int:
                case PrimitiveType::UInt: return 4;
                case PrimitiveType::Long:
                case PrimitiveType::ULong: return 8;

                case PrimitiveType::Float: return 4;
                case PrimitiveType::Double: return 8;

                case PrimitiveType::Function: {
                    FunctionDeclaration decl = std::get<FunctionDeclaration>(Data);
                    return decl.ReturnType->GetSize();
                }

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

                default: ARIA_UNREACHABLE();
            }

            return 0;
        }
    };

    inline std::string TypeInfoToString(TypeInfo* type) {
        std::string str;

        switch (type->Type) {
            case PrimitiveType::Invalid: str = "invalid"; break;
            case PrimitiveType::Void:    str = "void"; break;

            case PrimitiveType::Bool:    str = "bool"; break;

            case PrimitiveType::Char:    str = "char"; break;
            case PrimitiveType::UChar:   str = "uchar"; break;
            case PrimitiveType::Short:   str = "short"; break;
            case PrimitiveType::UShort:  str = "ushort"; break;
            case PrimitiveType::Int:     str = "int"; break;
            case PrimitiveType::UInt:    str = "uint"; break;
            case PrimitiveType::Long:    str = "long"; break;
            case PrimitiveType::ULong:   str = "ulong"; break;

            case PrimitiveType::Float:   str = "float"; break;
            case PrimitiveType::Double:  str = "double"; break;

            case PrimitiveType::String:  str = "string"; break;

            case PrimitiveType::Function: {
                FunctionDeclaration decl = std::get<FunctionDeclaration>(type->Data);

                if (decl.External) {
                    str = "extern ";
                }

                str += TypeInfoToString(decl.ReturnType);
                str += "(";

                for (size_t i = 0; i < decl.ParamTypes.Size; i++) {
                    str += TypeInfoToString(decl.ParamTypes.Items[i]);
                    if (i != decl.ParamTypes.Size - 1) {
                        str += ", ";
                    }
                }

                str += ")";
                break;
            }

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

} // namespace Aria::Internal
