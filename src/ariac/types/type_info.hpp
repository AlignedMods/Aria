#pragma once

#include "ariac/core.hpp"
#include "ariac/core/vector.hpp"
#include "ariac/core/source_location.hpp"
#include "ariac/types.hpp"

#include <string_view>

namespace ariac {

    struct Expr;
    struct Decl;

    enum class TypeKind {
        Error = 0,
        Void,

        Bool,

        Char, IChar,
        Short, UShort,
        Int, UInt,
        Long, ULong,
        Sz, Isz,

        Float,
        Double,

        Pointer,
        Array,
        Slice,

        Function,
        Method,

        Structure,
        Typedef,
        Enum,

        Generic,
        GenericDecl,
        GenericInstantiation,

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

    struct EnumType {
        EnumType(std::string_view identifier, Decl* source)
            : identifier(identifier), source_decl(source) {}

        std::string_view identifier;
        Decl* source_decl = nullptr;
    };

    struct GenericType {
        std::string_view identifier;
    };

    struct GenericDeclType {
        GenericDeclType(std::string_view identifier, Decl* source)
            : identifier(identifier), generic(source) {}

        std::string_view identifier;
        Decl* generic = nullptr;
    };

    struct GenericInstantiationType {
        GenericInstantiationType(TypeInfo* base, TinyVector<TypeInfo*> args)
            : base(base), arguments(args) {}

        TypeInfo* base = nullptr;
        TinyVector<TypeInfo*> arguments;
        Decl* resolved_decl = nullptr;
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
            EnumType enum_;
            GenericType generic;
            GenericDeclType generic_decl;
            GenericInstantiationType generic_instantiation;
            UnresolvedType unresolved;
        };

        static TypeInfo* create_basic(TypeKind kind, SourceLoc loc = {});
        static TypeInfo* create_with_base(TypeKind kind, TypeInfo* base, SourceLoc loc = {});
        static TypeInfo* create_function(TypeKind kind, TypeInfo* ret, TinyVector<TypeInfo*> params, bool var_arg, SourceLoc loc = {});
        static TypeInfo* create_struct(Decl* d, SourceLoc loc = {});
        static TypeInfo* create_struct(std::string_view name, Decl* d, SourceLoc loc = {});
        static TypeInfo* create_typedef(Decl* d, SourceLoc loc = {});
        static TypeInfo* create_enum(Decl* d, SourceLoc loc = {});
        static TypeInfo* create_generic_decl(Decl* d, SourceLoc loc = {});
        static TypeInfo* create_generic(std::string_view name, SourceLoc loc = {});

        static TypeInfo* get_error();
        static TypeInfo* get_void();
        static TypeInfo* get_basic(TypeKind kind);
        static TypeInfo* get_void_ptr();
        static TypeInfo* get_char_ptr();
        static TypeInfo* get_string();

        static TypeInfo* dup(TypeInfo* type);

        bool is_error() const { return kind == TypeKind::Error; }

        bool is_primitive() const {
            return is_void() || is_boolean() || is_numeric();
        }

        bool is_void() const {
            return kind == TypeKind::Void;
        }

        bool is_boolean() const {
            return kind == TypeKind::Bool;
        }

        bool is_integral() const {
            return kind == TypeKind::Char  || kind == TypeKind::IChar  ||
                   kind == TypeKind::Short || kind == TypeKind::UShort ||
                   kind == TypeKind::Int   || kind == TypeKind::UInt   ||
                   kind == TypeKind::Long  || kind == TypeKind::ULong  ||
                   kind == TypeKind::Sz  || kind == TypeKind::Isz;
        }

        bool is_floating_point() const {
            return kind == TypeKind::Float || kind == TypeKind::Double;
        }

        bool is_numeric() const {
            return is_integral() || is_floating_point();
        }

        bool is_num_or_ptr() const { return is_numeric() || is_pointer(); }

        bool is_pointer() const {
            return kind == TypeKind::Pointer;
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

        bool is_typedef() const {
            return kind == TypeKind::Typedef;
        }

        bool is_enum() const {
            return kind == TypeKind::Enum;
        }

        bool is_string() const;

        bool is_signed() const {
            return kind == TypeKind::IChar || kind == TypeKind::Short || kind == TypeKind::Int || kind == TypeKind::Long;
            ARIA_ASSERT(is_integral(), "is_signed() cannot operate on a non-integral type");
        }

        bool is_unsigned() const {
            ARIA_ASSERT(is_integral(), "is_unsigned() cannot operate on a non-integral type");
            return kind == TypeKind::Char || kind == TypeKind::UShort || kind == TypeKind::UInt || kind == TypeKind::ULong;
        }

        u64 get_size() const;
        u64 get_bit_size() const;
        u64 get_alignment() const;
    };

    std::string type_info_to_string(TypeInfo* type, bool pretty = true);

} // namespace ariac
