#pragma once

#include "aria/core.hpp"
#include "aria/internal/compiler/core/string_view.hpp"
#include "aria/internal/compiler/compilation_context.hpp"
#include "aria/internal/compiler/core/vector.hpp"

#include <variant>
#include <vector>

namespace Aria::Internal {

    struct Expr;
    struct Decl;

    enum class TypeKind {
        Error = 0,
        Void,

        Bool,

        Char, UChar,
        Short, UShort,
        Int, UInt,
        Long, ULong,

        Float,
        Double,

        Ptr,
        Array,
        Slice,

        Function,

        String,
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
        Expr* Expression = nullptr;
        u64 Size = 0;
    };

    struct UnresolvedType {
        Expr* Ident = nullptr;
    };

    struct TypeInfo {
        TypeKind Kind = TypeKind::Error;
        union {
            TypeInfo* Base = nullptr;
            FunctionDeclaration Function;
            ArrayDeclaration Array;
            StructDeclaration Struct;
            UnresolvedType Unresolved;
        };
        bool Reference = false;

        static TypeInfo* Create(CompilationContext* ctx, TypeKind kind, bool isReference);
        static TypeInfo* Dup(CompilationContext* ctx, TypeInfo* type);

        bool IsError() const { return Kind == TypeKind::Error; }

        bool IsPrimitive() const {
            return IsVoid() || IsBoolean() || IsNumeric() || IsPointer() || IsSlice() || IsString();
        }

        bool IsVoid() const {
            return Kind == TypeKind::Void;
        }

        bool IsBoolean() const {
            return Kind == TypeKind::Bool;
        }

        bool IsIntegral() const {
            return Kind == TypeKind::Char  || Kind == TypeKind::UChar  ||
                   Kind == TypeKind::Short || Kind == TypeKind::UShort ||
                   Kind == TypeKind::Int   || Kind == TypeKind::UInt   ||
                   Kind == TypeKind::Long  || Kind == TypeKind::ULong;
        }

        bool IsFloatingPoint() const {
            return Kind == TypeKind::Float || Kind == TypeKind::Double;
        }

        bool IsNumeric() const {
            return IsIntegral() || IsFloatingPoint();
        }

        bool IsPointer() const {
            return Kind == TypeKind::Ptr;
        }

        bool IsArray() const {
            return Kind == TypeKind::Array;
        }

        bool IsSlice() const {
            return Kind == TypeKind::Slice;
        }

        bool IsFunction() const {
            return Kind == TypeKind::Function;
        }

        bool IsStructure() const {
            return Kind == TypeKind::Structure;
        }

        bool IsString() const {
            return Kind == TypeKind::String;
        }

        bool IsSigned() const {
            ARIA_ASSERT(IsIntegral(), "IsSigned() cannot operate on a non-integral type");
            return Kind == TypeKind::Char || Kind == TypeKind::Short || Kind == TypeKind::Int || Kind == TypeKind::Long;
        }

        bool IsUnsigned() const {
            ARIA_ASSERT(IsIntegral(), "IsUnsigned() cannot operate on a non-integral type");
            return Kind == TypeKind::UChar || Kind == TypeKind::UShort || Kind == TypeKind::UInt || Kind == TypeKind::ULong;
        }

        bool IsReference() const {
            return Reference;
        }
    };

    inline std::string TypeInfoToString(TypeInfo* type) {
        std::string str;

        switch (type->Kind) {
            case TypeKind::Error:   str = "error"; break;
            case TypeKind::Void:    str = "void"; break;

            case TypeKind::Bool:    str = "bool"; break;

            case TypeKind::Char:    str = "char"; break;
            case TypeKind::UChar:   str = "uchar"; break;
            case TypeKind::Short:   str = "short"; break;
            case TypeKind::UShort:  str = "ushort"; break;
            case TypeKind::Int:     str = "int"; break;
            case TypeKind::UInt:    str = "uint"; break;
            case TypeKind::Long:    str = "long"; break;
            case TypeKind::ULong:   str = "ulong"; break;

            case TypeKind::Float:   str = "float"; break;
            case TypeKind::Double:  str = "double"; break;

            case TypeKind::Ptr: {
                TypeInfo* t = type->Base;
                str = fmt::format("{}*", TypeInfoToString(t));
                break;
            }

            case TypeKind::Array: {
                ArrayDeclaration& arr = type->Array;
                str = fmt::format("{}[{}]", TypeInfoToString(arr.Type), arr.Size);
                break;
            }

            case TypeKind::Slice: {
                TypeInfo* t = type->Base;
                str = fmt::format("{}[]", TypeInfoToString(t));
                break;
            }

            case TypeKind::String: str = "string"; break;

            case TypeKind::Function: {
                FunctionDeclaration decl = type->Function;

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

            case TypeKind::Structure: {
                StructDeclaration decl = type->Struct;

                str = fmt::format("struct {}", decl.Identifier);
                break;
            }

            case TypeKind::Unresolved: {
                str = "unresolved";
                break;
            }

            default: ARIA_UNREACHABLE();
        }

        if (type->IsReference()) {
            str += "&";
        }

        return str;
    }

    // To avoid unnecessary allocations of primitive types we declare them here globally
    inline TypeInfo ErrorType =   { TypeKind::Error };
    inline TypeInfo VoidType =    { TypeKind::Void };
    inline TypeInfo BoolType =    { TypeKind::Bool };
    inline TypeInfo CharType =    { TypeKind::Char };
    inline TypeInfo UCharType =   { TypeKind::UChar };
    inline TypeInfo ShortType =   { TypeKind::Short };
    inline TypeInfo UShortType =  { TypeKind::UShort };
    inline TypeInfo IntType =     { TypeKind::Int };
    inline TypeInfo UIntType =    { TypeKind::UInt };
    inline TypeInfo LongType =    { TypeKind::Long };
    inline TypeInfo ULongType =   { TypeKind::ULong };
    inline TypeInfo DoubleType =  { TypeKind::Double };
    inline TypeInfo FloatType =   { TypeKind::Float };
    inline TypeInfo StringType =  { TypeKind::String };
    inline TypeInfo VoidPtrType = { TypeKind::Ptr, &VoidType };

} // namespace Aria::Internal
