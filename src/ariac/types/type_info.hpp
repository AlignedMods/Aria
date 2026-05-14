#pragma once

#include "common/core.hpp"
#include "ariac/compilation_context.hpp"
#include "ariac/core/vector.hpp"

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
        Ref,

        Function,

        String,
        Structure,

        Unresolved
    };

    struct TypeInfo;

    struct StructDeclaration {
        std::string_view identifier;
        Decl* source_decl = nullptr;
    };

    struct FunctionDeclaration {
        TypeInfo* return_type = nullptr;
        TinyVector<TypeInfo*> param_types;
    };

    struct ArrayDeclaration {
        TypeInfo* type = nullptr;
        Expr* expression = nullptr;
        u64 size = 0;
    };

    struct UnresolvedType {
        Expr* ident = nullptr;
    };

    struct TypeInfo {
        TypeKind kind = TypeKind::Error;
        union {
            TypeInfo* base = nullptr;
            FunctionDeclaration function;
            ArrayDeclaration array;
            StructDeclaration struct_;
            UnresolvedType unresolved;
        };

        static TypeInfo* Create(CompilationContext* ctx, TypeKind kind);
        static TypeInfo* Dup(CompilationContext* ctx, TypeInfo* type);

        bool is_error() const { return kind == TypeKind::Error; }

        bool is_primitive() const {
            return is_void() || is_boolean() || is_numeric() || is_pointer() || is_slice() || is_string() || is_reference();
        }

        bool is_void() const {
            return kind == TypeKind::Void;
        }

        bool is_boolean() const {
            return kind == TypeKind::Bool;
        }

        bool is_integral() const {
            return kind == TypeKind::Char  || kind == TypeKind::UChar  ||
                   kind == TypeKind::Short || kind == TypeKind::UShort ||
                   kind == TypeKind::Int   || kind == TypeKind::UInt   ||
                   kind == TypeKind::Long  || kind == TypeKind::ULong;
        }

        bool is_floating_point() const {
            return kind == TypeKind::Float || kind == TypeKind::Double;
        }

        bool is_numeric() const {
            return is_integral() || is_floating_point();
        }

        bool is_pointer() const {
            return kind == TypeKind::Ptr;
        }

        bool is_array() const {
            return kind == TypeKind::Array;
        }

        bool is_slice() const {
            return kind == TypeKind::Slice;
        }

        bool is_function() const {
            return kind == TypeKind::Function;
        }

        bool is_structure() const {
            return kind == TypeKind::Structure;
        }

        bool is_string() const {
            return kind == TypeKind::String;
        }

        bool is_signed() const {
            ARIA_ASSERT(is_integral(), "is_signed() cannot operate on a non-integral type");
            return kind == TypeKind::Char || kind == TypeKind::Short || kind == TypeKind::Int || kind == TypeKind::Long;
        }

        bool is_unsigned() const {
            ARIA_ASSERT(is_integral(), "is_unsigned() cannot operate on a non-integral type");
            return kind == TypeKind::UChar || kind == TypeKind::UShort || kind == TypeKind::UInt || kind == TypeKind::ULong;
        }

        bool is_reference() const {
            return kind == TypeKind::Ref;
        }
    };

    std::string type_info_to_string(TypeInfo* type);

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
