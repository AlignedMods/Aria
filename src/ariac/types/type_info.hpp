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

    struct FunctionType {
        FunctionType(TypeInfo* ret, TinyVector<TypeInfo*> params, bool var_arg)
            : return_type(ret), param_types(params), var_arg(var_arg) {}

        TypeInfo* return_type = nullptr;
        TinyVector<TypeInfo*> param_types;
        bool var_arg = false;
    };

    struct ArrayType {
        ArrayType(TypeInfo* base, Expr* expr)
            : base(base), expression(expr) {}

        TypeInfo* base = nullptr;
        Expr* expression = nullptr;
        u64 size = 0;
    };

    struct StructType {
        StructType(std::string_view identifer, Decl* source)
            : identifier(identifer), source_decl(source) {}

        std::string_view identifier;
        Decl* source_decl = nullptr;
    };

    struct TypedefType {
        TypedefType(std::string_view identifer, TypeInfo* base, Decl* source)
            : identifier(identifer), base(base), source_decl(source) {}

        std::string_view identifier;
        TypeInfo* base = nullptr;
        Decl* source_decl = nullptr;
    };

    struct UnresolvedType {
        Expr* ident = nullptr;
    };

    struct TypeInfo {
        TypeKind kind = TypeKind::Error;
        SourceLoc loc;
        union {
            TypeInfo* base = nullptr;
            FunctionType function;
            ArrayType array;
            StructType struct_;
            TypedefType typedef_;
            UnresolvedType unresolved;
        };

        static TypeInfo* create_basic(CompilationContext* ctx, TypeKind kind, SourceLoc loc = {});
        static TypeInfo* create_with_base(CompilationContext* ctx, TypeKind kind, TypeInfo* base, SourceLoc loc = {});
        static TypeInfo* create_function(CompilationContext* ctx, TypeKind kind, TypeInfo* ret, TinyVector<TypeInfo*> params, bool var_arg, SourceLoc loc = {});
        static TypeInfo* create_struct(CompilationContext* ctx, Decl* d, SourceLoc loc = {});
        static TypeInfo* create_typedef(CompilationContext* ctx, Decl* d, SourceLoc loc = {});

        static TypeInfo* get_error(CompilationContext* ctx);
        static TypeInfo* get_void(CompilationContext* ctx);
        static TypeInfo* get_basic(CompilationContext* ctx, TypeKind kind);
        static TypeInfo* get_void_ptr(CompilationContext* ctx);
        static TypeInfo* get_char_ptr(CompilationContext* ctx);
        static TypeInfo* get_string(CompilationContext* ctx);

        static TypeInfo* dup(CompilationContext* ctx, TypeInfo* type);

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

        bool is_string() const;

        bool is_signed() const {
            return kind == TypeKind::Char || kind == TypeKind::Short || kind == TypeKind::Int || kind == TypeKind::Long;
            ARIA_ASSERT(is_integral(), "is_signed() cannot operate on a non-integral type");
        }

        bool is_unsigned() const {
            ARIA_ASSERT(is_integral(), "is_unsigned() cannot operate on a non-integral type");
            return kind == TypeKind::UChar || kind == TypeKind::UShort || kind == TypeKind::UInt || kind == TypeKind::ULong;
        }

        bool is_reference() const {
            return kind == TypeKind::Ref;
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
    };

    std::string type_info_to_string(TypeInfo* type, bool pretty = true);

} // namespace ariac
