#pragma once

#include "aria/core.hpp"
#include "aria/internal/compiler/core/string_view.hpp"
#include "aria/internal/compiler/compilation_context.hpp"
#include "aria/internal/compiler/core/vector.hpp"

#include <variant>
#include <vector>

namespace Aria::Internal {

    struct Decl;

    enum class PrimitiveType {
        Error = 0,
        Void,

        Bool,

        Char, UChar,
        Short, UShort,
        Int, UInt,
        Long, ULong,

        Float,
        Double,

        Function,

        String,
        Array,
        Structure,

        Unresolved
    };

    struct TypeInfo;

    struct StructDeclaration {
        StringView Identifier;
        Decl* SourceDecl = nullptr;
    };

    struct FunctionDeclaration {
        TypeInfo* ReturnType = nullptr;
        TinyVector<TypeInfo*> ParamTypes;
    };

    struct ArrayDeclaration {
        TypeInfo* Type = nullptr;
    };

    struct UnresolvedType {
        StringView Identifier;
    };

    struct TypeInfo {
        PrimitiveType Type = PrimitiveType::Error;
        std::variant<size_t, TypeInfo*, FunctionDeclaration, ArrayDeclaration, StructDeclaration, UnresolvedType> Data;
        bool Reference = false;

        static TypeInfo* Create(CompilationContext* ctx, PrimitiveType type, bool isReference, decltype(TypeInfo::Data) data = {});

        bool IsError() const { return Type == PrimitiveType::Error; }

        bool IsTrivial() const {
            return IsVoid() || IsBoolean() || IsNumeric();
        }

        bool IsVoid() const {
            return Type == PrimitiveType::Void;
        }

        bool IsBoolean() const {
            return Type == PrimitiveType::Bool;
        }

        bool IsIntegral() const {
            return Type == PrimitiveType::Char  || Type == PrimitiveType::UChar  ||
                   Type == PrimitiveType::Short || Type == PrimitiveType::UShort ||
                   Type == PrimitiveType::Int   || Type == PrimitiveType::UInt   ||
                   Type == PrimitiveType::Long  || Type == PrimitiveType::ULong;
        }

        bool IsFloatingPoint() const {
            return Type == PrimitiveType::Float || Type == PrimitiveType::Double;
        }

        bool IsNumeric() const {
            return IsIntegral() || IsFloatingPoint();
        }

        bool IsFunction() const {
            return Type == PrimitiveType::Function;
        }

        bool IsStructure() const {
            return Type == PrimitiveType::Structure;
        }

        bool IsString() const {
            return Type == PrimitiveType::String;
        }

        bool IsSigned() const {
            ARIA_ASSERT(IsIntegral(), "IsSigned() cannot operate on a non-integral type");
            return Type == PrimitiveType::Char || Type == PrimitiveType::Short || Type == PrimitiveType::Int || Type == PrimitiveType::Long;
        }

        bool IsUnsigned() const {
            ARIA_ASSERT(IsIntegral(), "IsUnsigned() cannot operate on a non-integral type");
            return Type == PrimitiveType::UChar || Type == PrimitiveType::UShort || Type == PrimitiveType::UInt || Type == PrimitiveType::ULong;
        }

        bool IsReference() const {
            return Reference;
        }
    };

    inline std::string TypeInfoToString(TypeInfo* type) {
        std::string str;

        switch (type->Type) {
            case PrimitiveType::Error:   str = "error"; break;
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

        if (type->IsReference()) {
            str += "&";
        }

        return str;
    }

    // To avoid unnecessary allocations of primitive types we declare them here globally
    inline TypeInfo ErrorType =  { PrimitiveType::Error };
    inline TypeInfo BoolType =   { PrimitiveType::Bool };
    inline TypeInfo CharType =   { PrimitiveType::Char };
    inline TypeInfo UCharType =  { PrimitiveType::UChar };
    inline TypeInfo ShortType =  { PrimitiveType::Short };
    inline TypeInfo UShortType = { PrimitiveType::UShort };
    inline TypeInfo IntType =    { PrimitiveType::Int };
    inline TypeInfo UIntType =   { PrimitiveType::UInt };
    inline TypeInfo LongType =   { PrimitiveType::Long };
    inline TypeInfo ULongType =  { PrimitiveType::ULong };
    inline TypeInfo DoubleType = { PrimitiveType::Double };
    inline TypeInfo FloatType =  { PrimitiveType::Float };
    inline TypeInfo StringType = { PrimitiveType::String };

} // namespace Aria::Internal
