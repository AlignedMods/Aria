#pragma once

#include "aria/core.hpp"
#include "aria/internal/compiler/compilation_context.hpp"
#include "aria/internal/compiler/core/vector.hpp"

#include <string_view>

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
        std::string_view Identifier;
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

        bool is_error() const { return Kind == TypeKind::Error; }

        bool is_primitive() const {
            return is_void() || is_boolean() || is_numeric() || is_pointer() || is_slice() || is_string();
        }

        bool is_void() const {
            return Kind == TypeKind::Void;
        }

        bool is_boolean() const {
            return Kind == TypeKind::Bool;
        }

        bool is_integral() const {
            return Kind == TypeKind::Char  || Kind == TypeKind::UChar  ||
                   Kind == TypeKind::Short || Kind == TypeKind::UShort ||
                   Kind == TypeKind::Int   || Kind == TypeKind::UInt   ||
                   Kind == TypeKind::Long  || Kind == TypeKind::ULong;
        }

        bool is_floating_point() const {
            return Kind == TypeKind::Float || Kind == TypeKind::Double;
        }

        bool is_numeric() const {
            return is_integral() || is_floating_point();
        }

        bool is_pointer() const {
            return Kind == TypeKind::Ptr;
        }

        bool is_array() const {
            return Kind == TypeKind::Array;
        }

        bool is_slice() const {
            return Kind == TypeKind::Slice;
        }

        bool is_function() const {
            return Kind == TypeKind::Function;
        }

        bool is_structure() const {
            return Kind == TypeKind::Structure;
        }

        bool is_string() const {
            return Kind == TypeKind::String;
        }

        bool is_signed() const {
            ARIA_ASSERT(is_integral(), "is_signed() cannot operate on a non-integral type");
            return Kind == TypeKind::Char || Kind == TypeKind::Short || Kind == TypeKind::Int || Kind == TypeKind::Long;
        }

        bool is_unsigned() const {
            ARIA_ASSERT(is_integral(), "is_unsigned() cannot operate on a non-integral type");
            return Kind == TypeKind::UChar || Kind == TypeKind::UShort || Kind == TypeKind::UInt || Kind == TypeKind::ULong;
        }

        bool is_reference() const {
            return Reference;
        }
    };

    inline std::string type_info_to_string(TypeInfo* type) {
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
                str = fmt::format("{}*", type_info_to_string(t));
                break;
            }

            case TypeKind::Array: {
                ArrayDeclaration& arr = type->Array;
                str = fmt::format("{}[{}]", type_info_to_string(arr.Type), arr.Size);
                break;
            }

            case TypeKind::Slice: {
                TypeInfo* t = type->Base;
                str = fmt::format("{}[]", type_info_to_string(t));
                break;
            }

            case TypeKind::String: str = "string"; break;

            case TypeKind::Function: {
                FunctionDeclaration decl = type->Function;

                str += type_info_to_string(decl.ReturnType);
                str += "(";

                for (size_t i = 0; i < decl.ParamTypes.Size; i++) {
                    str += type_info_to_string(decl.ParamTypes.Items[i]);
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

        if (type->is_reference()) {
            str += "&";
        }

        return str;
    }

    // To avoid unnecessary allocations of primitive types we declare them here globally
    inline TypeInfo error_type =      { TypeKind::Error };
    inline TypeInfo void_type =       { TypeKind::Void };
    inline TypeInfo bool_type =       { TypeKind::Bool };
    inline TypeInfo char_type =       { TypeKind::Char };
    inline TypeInfo uchar_type =      { TypeKind::UChar };
    inline TypeInfo short_type =      { TypeKind::Short };
    inline TypeInfo ushort_type =     { TypeKind::UShort };
    inline TypeInfo int_type =        { TypeKind::Int };
    inline TypeInfo uint_type =       { TypeKind::UInt };
    inline TypeInfo long_type =       { TypeKind::Long };
    inline TypeInfo ulong_type =      { TypeKind::ULong };
    inline TypeInfo double_type =     { TypeKind::Double };
    inline TypeInfo float_type =      { TypeKind::Float };
    inline TypeInfo string_type =     { TypeKind::String };
    inline TypeInfo void_ptr_type =   { TypeKind::Ptr, &void_type };
    inline TypeInfo char_slice_type = { TypeKind::Slice, &char_type };

} // namespace Aria::Internal
