#pragma once

#include "ariac/core.hpp"
#include "ariac/compilation_context.hpp"
#include "ariac/core/vector.hpp"

#include <string_view>

namespace ariac {

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
        Method,

        Structure,
        Typedef,

        Unresolved
    };

    struct TypeInfo;

    struct StructDeclaration {
        std::string_view identifier;
        Decl* source_decl = nullptr;
    };

    struct TypedefDeclaration {
        std::string_view identifier;
        TypeInfo* base_type = nullptr;
        Decl* source_decl = nullptr;
    };

    struct FunctionDeclaration {
        TypeInfo* return_type = nullptr;
        TinyVector<TypeInfo*> param_types;
        bool var_arg = false;
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
        SourceLoc loc;
        union {
            TypeInfo* base = nullptr;
            FunctionDeclaration function;
            ArrayDeclaration array;
            StructDeclaration struct_;
            TypedefDeclaration typedef_;
            UnresolvedType unresolved;
        };

        static TypeInfo* Create(CompilationContext* ctx, TypeKind kind);
        static TypeInfo* Dup(CompilationContext* ctx, TypeInfo* type);

        bool is_error() const { return kind == TypeKind::Error; }

        bool is_primitive() const {
            return is_void() || is_boolean() || is_numeric() || is_pointer() || is_slice() || is_reference();
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

        bool is_num_or_ptr() const { return is_numeric() || is_pointer(); }

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

        bool is_method() const {
            return kind == TypeKind::Method;
        }

        bool is_structure() const {
            return kind == TypeKind::Structure;
        }

        bool is_typdef() const {
            return kind == TypeKind::Typedef;
        }

        bool is_signed() const {
            ARIA_ASSERT(is_integral(), "is_signed() cannot operate on a non-integral type");
            return kind == TypeKind::Char || kind == TypeKind::Short || kind == TypeKind::Int || kind == TypeKind::Long;
        }

        bool is_unsigned() const {
            ARIA_ASSERT(is_integral(), "is_unsigned() cannot operate on a non-integral type");
            return kind == TypeKind::UChar || kind == TypeKind::UShort || kind == TypeKind::UInt || kind == TypeKind::ULong;
        }

        size_t get_bit_size() const {
            switch (kind) {
                case TypeKind::Bool: return 1;

                case TypeKind::Char:
                case TypeKind::UChar: return 8;

                case TypeKind::Short:
                case TypeKind::UShort: return 16;

                case TypeKind::Int:
                case TypeKind::UInt: return 32;

                case TypeKind::Long:
                case TypeKind::ULong: return 64;

                case TypeKind::Float: return 32;
                case TypeKind::Double: return 64;

                default: ARIA_UNREACHABLE();
            }
        }

        bool is_reference() const {
            return kind == TypeKind::Ref;
        }
    };

    std::string type_info_to_string(TypeInfo* type, bool pretty = true);

    // To avoid unnecessary allocations of primitive types we declare them here globally
    inline TypeInfo error_type =          { TypeKind::Error };
    inline TypeInfo void_type =           { TypeKind::Void };
    inline TypeInfo bool_type =           { TypeKind::Bool };
    inline TypeInfo char_type =           { TypeKind::Char };
    inline TypeInfo uchar_type =          { TypeKind::UChar };
    inline TypeInfo short_type =          { TypeKind::Short };
    inline TypeInfo ushort_type =         { TypeKind::UShort };
    inline TypeInfo int_type =            { TypeKind::Int };
    inline TypeInfo uint_type =           { TypeKind::UInt };
    inline TypeInfo long_type =           { TypeKind::Long };
    inline TypeInfo ulong_type =          { TypeKind::ULong };
    inline TypeInfo double_type =         { TypeKind::Double };
    inline TypeInfo float_type =          { TypeKind::Float };
    inline TypeInfo void_ptr_type =       { TypeKind::Ptr, {}, &void_type };
    inline TypeInfo char_ptr_type =       { TypeKind::Ptr, {}, &char_type };
    inline TypeInfo char_slice_type =     { TypeKind::Slice, {}, &char_type };

} // namespace ariac
