#include "ariac/types/type_info.hpp"
#include "ariac/ast/decl.hpp"

namespace ariac {

    static TypeInfo* error_type;
    static TypeInfo* void_type;
    static TypeInfo* bool_type;
    static TypeInfo* char_type;
    static TypeInfo* ichar_type;
    static TypeInfo* short_type;
    static TypeInfo* ushort_type;
    static TypeInfo* int_type;
    static TypeInfo* uint_type;
    static TypeInfo* long_type;
    static TypeInfo* ulong_type;
    static TypeInfo* float_type;
    static TypeInfo* double_type;
    static TypeInfo* void_ptr_type;
    static TypeInfo* char_ptr_type;
    static TypeInfo* char_slice_type;
    static TypeInfo* std_core_string_type;

    TypeInfo* TypeInfo::create_basic(CompilationContext* ctx, TypeKind kind, SourceLoc loc) {
        TypeInfo* t = ctx->allocate<TypeInfo>();
        t->kind = kind;
        t->loc = loc;
        return t;
    }

    TypeInfo* TypeInfo::create_with_base(CompilationContext* ctx, TypeKind kind, TypeInfo* base, SourceLoc loc) {
        TypeInfo* t = create_basic(ctx, kind, loc);
        t->base = base;
        return t;
    }

    TypeInfo* TypeInfo::create_function(CompilationContext* ctx, TypeKind kind, TypeInfo* ret, TinyVector<TypeInfo*> params, bool var_arg, SourceLoc loc) {
        TypeInfo* t = ctx->allocate<TypeInfo>();
        t->kind = kind;
        t->loc = loc;
        t->function = FunctionType(ret, params, var_arg);
        return t;
    }

    TypeInfo* TypeInfo::create_struct(CompilationContext* ctx, Decl* d, SourceLoc loc) {
        TypeInfo* t = create_basic(ctx, TypeKind::Structure, loc);
        t->struct_ = StructType(d->struct_.identifier, d);
        return t;
    }

    TypeInfo* TypeInfo::create_typedef(CompilationContext* ctx, Decl* d, SourceLoc loc) {
        TypeInfo* t = create_basic(ctx, TypeKind::Typedef, loc);
        t->typedef_ = TypedefType(d->typedef_.identifier, d->typedef_.type, d);
        return t;
    }

    TypeInfo* TypeInfo::create_enum(CompilationContext* ctx, Decl* d, SourceLoc loc) {
        TypeInfo* t = create_basic(ctx, TypeKind::Enum, loc);
        t->enum_ = EnumType(d->enum_.identifier, d);
        return t;
    }

    TypeInfo* TypeInfo::create_generic(CompilationContext* ctx, std::string_view name, SourceLoc loc) {
        TypeInfo* t = create_basic(ctx, TypeKind::Generic, loc);
        t->generic = GenericType(name);
        return t;
    }

    TypeInfo* TypeInfo::dup(CompilationContext* ctx, TypeInfo* type) {
        TypeInfo* t = ctx->allocate<TypeInfo>();
        memcpy(reinterpret_cast<void*>(t), type, sizeof(TypeInfo));
        return t;
    }

    TypeInfo* TypeInfo::get_error(CompilationContext* ctx) {
        return get_basic(ctx, TypeKind::Error);
    }

    TypeInfo* TypeInfo::get_void(CompilationContext* ctx) {
        return get_basic(ctx, TypeKind::Void);
    }

    TypeInfo* TypeInfo::get_basic(CompilationContext* ctx, TypeKind kind) {
        #define TYPE(ki, var) \
            case TypeKind::ki: { \
                if (var) { return var; } \
                var = create_basic(ctx, TypeKind::ki); \
                return var; \
            }

        switch (kind) {
            TYPE(Error, error_type)
            TYPE(Void, void_type)
            TYPE(Bool, bool_type)
            TYPE(Char, char_type)
            TYPE(IChar, ichar_type)
            TYPE(Short, short_type)
            TYPE(UShort, ushort_type)
            TYPE(Int, int_type)
            TYPE(UInt, uint_type)
            TYPE(Long, long_type)
            TYPE(ULong, ulong_type)
            TYPE(Float, float_type)
            TYPE(Double, double_type)

            default: ARIA_UNREACHABLE();
        }
    }

    TypeInfo* TypeInfo::get_void_ptr(CompilationContext* ctx) {
        if (void_ptr_type) { return char_ptr_type; }
        void_ptr_type = create_with_base(ctx, TypeKind::Ptr, get_basic(ctx, TypeKind::Void));
        return void_ptr_type;
    }

    TypeInfo* TypeInfo::get_char_ptr(CompilationContext* ctx) {
        if (char_ptr_type) { return char_ptr_type; }
        char_ptr_type = create_with_base(ctx, TypeKind::Ptr, get_basic(ctx, TypeKind::Char));
        return char_ptr_type;
    }

    TypeInfo* TypeInfo::get_string(CompilationContext* ctx) {
        if (ctx->flags.no_stdlib) {
            if (char_slice_type) { return char_slice_type; }
            char_slice_type = create_with_base(ctx, TypeKind::Slice, get_basic(ctx, TypeKind::Char));
            return char_slice_type;
        } else {
            ARIA_ASSERT(ctx->std_core_module->symbols.contains("String"), "std::core module must contain a definition for 'String'");
            Decl* sym = ctx->std_core_module->symbols.at("String");
            
            ARIA_ASSERT(sym->kind == DeclKind::Typedef, "std::core::String must be a typedef");
            ARIA_ASSERT(sym->typedef_.type->kind == TypeKind::Slice && sym->typedef_.type->base->kind == TypeKind::Char, "std::core::String must be a typedef to char[]");

            if (std_core_string_type) { return std_core_string_type; }
            std_core_string_type = create_typedef(ctx, sym);
            return std_core_string_type;
        }
    }

    bool TypeInfo::is_string() const {
        if (std_core_string_type) {
            return is_typdef() && typedef_.source_decl == std_core_string_type->typedef_.source_decl;
        } else {
            return is_slice() && base->kind == TypeKind::Char;
        }
    }

    std::string type_info_to_string(TypeInfo* type, bool pretty) {
        std::string str;

        switch (type->kind) {
            case TypeKind::Error:   str += "error"; break;
            case TypeKind::Void:    str += "void"; break;

            case TypeKind::Bool:    str += "bool"; break;

            case TypeKind::Char:    str += "char"; break;
            case TypeKind::IChar:   str += "ichar"; break;
            case TypeKind::Short:   str += "short"; break;
            case TypeKind::UShort:  str += "ushort"; break;
            case TypeKind::Int:     str += "int"; break;
            case TypeKind::UInt:    str += "uint"; break;
            case TypeKind::Long:    str += "long"; break;
            case TypeKind::ULong:   str += "ulong"; break;

            case TypeKind::Float:   str += "float"; break;
            case TypeKind::Double:  str += "double"; break;

            case TypeKind::Ptr: {
                TypeInfo* t = type->base;
                str = fmt::format("{}*", type_info_to_string(t), pretty);
                break;
            }

            case TypeKind::Array: {
                ArrayType& arr = type->array;
                str = fmt::format("{}[{}]", type_info_to_string(arr.base, pretty), arr.size);
                break;
            }

            case TypeKind::Slice: {
                TypeInfo* t = type->base;
                str = fmt::format("{}[]", type_info_to_string(t), pretty);
                break;
            }

            case TypeKind::Ref: {
                TypeInfo* t = type->base;
                str = fmt::format("{}&", type_info_to_string(t), pretty);
                break;
            }

            case TypeKind::Function: {
                FunctionType ty = type->function;

                str += type_info_to_string(ty.return_type, pretty);
                str += "(";

                for (size_t i = 0; i < ty.param_types.size; i++) {
                    str += type_info_to_string(ty.param_types.items[i], pretty);
                    if (i != ty.param_types.size - 1) {
                        str += ", ";
                    }
                }

                if (ty.var_arg) { str += ", ..."; }

                str += ")";
                break;
            }

            case TypeKind::Method: {
                FunctionType ty = type->function;

                str += type_info_to_string(ty.return_type, pretty);
                str += "(";

                for (size_t i = 0; i < ty.param_types.size; i++) {
                    str += type_info_to_string(ty.param_types.items[i], pretty);
                    if (i != ty.param_types.size - 1) {
                        str += ", ";
                    }
                }

                if (ty.var_arg) { str += ", ..."; }

                str += ")";
                break;
            }

            case TypeKind::Structure: {
                StructType ty = type->struct_;

                str += (ty.source_decl && ty.source_decl->parent_module) ? fmt::format("struct {}::{}", ty.source_decl->parent_module->name, ty.identifier) : fmt::format("struct {}", ty.identifier);
                break;
            }

            case TypeKind::Typedef: {
                TypedefType ty = type->typedef_;

                if (pretty) {
                    str += (ty.source_decl && ty.source_decl->parent_module) ? fmt::format("{}::{} (aka '{}')", ty.source_decl->parent_module->name, ty.identifier, type_info_to_string(ty.base, pretty)) : fmt::format("{} (aka '{}')", ty.identifier, type_info_to_string(ty.base, pretty));
                } else {
                    str += (ty.source_decl && ty.source_decl->parent_module) ? fmt::format("{}::{}':'{}", ty.source_decl->parent_module->name, ty.identifier, type_info_to_string(ty.base, pretty)) : fmt::format("{}':'{}", ty.identifier, type_info_to_string(ty.base, pretty));
                }
                break;
            }

            case TypeKind::Enum: {
                EnumType ty = type->enum_;

                str += (ty.source_decl && ty.source_decl->parent_module) ? fmt::format("enum {}::{}", ty.source_decl->parent_module->name, ty.identifier) : fmt::format("enum {}", ty.identifier);
                break;
            }

            case TypeKind::Generic: {
                str += type->generic.identifier;
                break;
            }

            case TypeKind::GenericDecl: {
                GenericDeclType ty = type->generic_decl;

                str += (ty.generic && ty.generic->parent_module) ? fmt::format("{}::{}", ty.generic->parent_module->name, ty.identifier) : fmt::format("{}", ty.identifier);
                break;
            }

            case TypeKind::GenericInstantiation: {
                str += fmt::format("{}!(", type_info_to_string(type->generic_instantiation.base, pretty));

                for (size_t i = 0; i < type->generic_instantiation.arguments.size; i++) {
                    if (i > 0) { str += ", "; }
                    str += type_info_to_string(type->generic_instantiation.arguments.items[i], pretty);
                }

                str += ")";
                break;
            }

            case TypeKind::Unresolved: {
                str = "unresolved";
                break;
            }

            default: ARIA_UNREACHABLE();
        }

        return str;
    }

} // namespace ariac 
