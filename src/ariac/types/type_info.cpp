#include "ariac/types/type_info.hpp"
#include "ariac/ast/decl.hpp"
#include "ariac/ast/expr.hpp"
#include "ariac/compilation_context.hpp"

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
    static TypeInfo* sz_type;
    static TypeInfo* isz_type;
    static TypeInfo* float_type;
    static TypeInfo* double_type;
    static TypeInfo* typeinfo_type;
    static TypeInfo* any_type;
    static TypeInfo* void_ptr_type;
    static TypeInfo* char_ptr_type;
    static TypeInfo* typeinfo_ptr_type;
    static TypeInfo* char_slice_type;
    static TypeInfo* std_core_string_type;

    TypeInfo* TypeInfo::create_basic(TypeKind kind, SourceLoc loc) {
        TypeInfo* t = context.allocate<TypeInfo>();
        t->kind = kind;
        t->loc = loc;
        return t;
    }

    TypeInfo* TypeInfo::create_with_base(TypeKind kind, TypeInfo* base, SourceLoc loc) {
        TypeInfo* t = create_basic(kind, loc);
        t->base = base;
        return t;
    }

    TypeInfo* TypeInfo::create_array(TypeInfo* base, u64 size, SourceLoc loc) {
        TypeInfo* t = create_basic(TypeKind::Array, loc);
        t->array.base = base;
        t->array.size = size;
        return t;
    }

    TypeInfo* TypeInfo::create_function(TypeKind kind, TypeInfo* ret, TinyVector<TypeInfo*> params, VariadicKind variadic, SourceLoc loc) {
        TypeInfo* t = context.allocate<TypeInfo>();
        t->kind = kind;
        t->loc = loc;
        t->function = FunctionType(ret, params, variadic);
        return t;
    }

    TypeInfo* TypeInfo::create_struct(Decl* d, SourceLoc loc) {
        TypeInfo* t = create_basic(TypeKind::Structure, loc);
        t->struct_ = StructType(d->struct_.identifier, d);
        return t;
    }

    TypeInfo* TypeInfo::create_struct(std::string_view name, Decl* d, SourceLoc loc) {
        TypeInfo* t = create_basic(TypeKind::Structure, loc);
        t->struct_ = StructType(name, d);
        return t;
    }

    TypeInfo* TypeInfo::create_typedef(Decl* d, SourceLoc loc) {
        TypeInfo* t = create_basic(TypeKind::Typedef, loc);
        t->typedef_ = TypedefType(d->typedef_.identifier, d->typedef_.type, d);
        return t;
    }

    TypeInfo* TypeInfo::create_enum(Decl* d, SourceLoc loc) {
        TypeInfo* t = create_basic(TypeKind::Enum, loc);
        t->enum_ = EnumType(d->enum_.identifier, d);
        return t;
    }

    TypeInfo* TypeInfo::create_generic_decl(Decl* d, SourceLoc loc) {
        TypeInfo* t = create_basic(TypeKind::GenericDecl, loc);
        t->generic_decl = GenericDeclType(d->generic.decl->struct_.identifier, d);
        return t;
    }

    TypeInfo* TypeInfo::create_generic(std::string_view name, SourceLoc loc) {
        TypeInfo* t = create_basic(TypeKind::Generic, loc);
        t->generic = GenericType(name);
        return t;
    }

    TypeInfo* TypeInfo::create_unresolved(Expr* e, SourceLoc loc) {
        TypeInfo* t = create_basic(TypeKind::Unresolved, loc);
        t->unresolved = UnresolvedType(e);
        return t;
    }

    TypeInfo* TypeInfo::dup(TypeInfo* type) {
        if (type == nullptr) { return nullptr; }

        TypeInfo* t = TypeInfo::create_basic(type->kind, type->loc);

        switch (type->kind) {
            case TypeKind::Error:
            case TypeKind::Void:
            case TypeKind::Bool:
            case TypeKind::Char:
            case TypeKind::IChar:
            case TypeKind::Short:
            case TypeKind::UShort:
            case TypeKind::Int:
            case TypeKind::UInt:
            case TypeKind::Long:
            case TypeKind::ULong:
            case TypeKind::Sz:
            case TypeKind::Isz:
            case TypeKind::Float:
            case TypeKind::Double:
            case TypeKind::TypeInfo:
            case TypeKind::Any: break;

            case TypeKind::Pointer: t->base = TypeInfo::dup(type->base); break;

            case TypeKind::Array: {
                t->array.base = TypeInfo::dup(type->base);
                if (type->array.expression) { t->array.expression = Expr::dup(type->array.expression); }
                t->array.size = type->array.size;
                break;
            }

            case TypeKind::Slice: t->base = TypeInfo::dup(type->base); break;

            case TypeKind::Function:
            case TypeKind::Method: {
                for (TypeInfo* ty : type->function.param_types) {
                    t->function.param_types.append(TypeInfo::dup(ty));
                }

                t->function.return_type = TypeInfo::dup(type->function.return_type);
                t->function.variadic = type->function.variadic;
                break;
            }

            case TypeKind::Structure: {
                t->struct_.identifier = type->struct_.identifier;
                t->struct_.source_decl = type->struct_.source_decl;
                break;
            }

            case TypeKind::Typedef: {
                t->typedef_.identifier = type->typedef_.identifier;
                t->typedef_.base = TypeInfo::dup(type->typedef_.base);
                t->typedef_.source_decl = type->typedef_.source_decl;
                break;
            }

            case TypeKind::Enum: {
                t->enum_.identifier = type->enum_.identifier;
                t->enum_.source_decl = type->enum_.source_decl;
                break;
            }

            case TypeKind::Generic: {
                t->generic.identifier = type->generic.identifier;
                t->generic.resolved_decl = type->generic.resolved_decl;
                break;
            }

            case TypeKind::Unresolved: {
                t->unresolved.ident = type->unresolved.ident;
                break;
            }

            default: ARIA_UNREACHABLE("Invalid type kind");
        }

        return t;
    }

    TypeInfo* TypeInfo::get_error() {
        return get_basic(TypeKind::Error);
    }

    TypeInfo* TypeInfo::get_void() {
        return get_basic(TypeKind::Void);
    }

    TypeInfo* TypeInfo::get_basic(TypeKind kind) {
        #define TYPE(ki, var) \
            case TypeKind::ki: { \
                if (var) { return var; } \
                var = create_basic(TypeKind::ki); \
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
            TYPE(Sz, sz_type)
            TYPE(Isz, isz_type)
            TYPE(Float, float_type)
            TYPE(Double, double_type)
            TYPE(TypeInfo, typeinfo_type)
            TYPE(Any, any_type)

            default: ARIA_UNREACHABLE("Invalid type kind");
        }
    }

    TypeInfo* TypeInfo::get_void_ptr() {
        if (void_ptr_type) { return void_ptr_type; }
        void_ptr_type = create_with_base(TypeKind::Pointer, get_basic(TypeKind::Void));
        return void_ptr_type;
    }

    TypeInfo* TypeInfo::get_char_ptr() {
        if (char_ptr_type) { return char_ptr_type; }
        char_ptr_type = create_with_base(TypeKind::Pointer, get_basic(TypeKind::Char));
        return char_ptr_type;
    }

    TypeInfo* TypeInfo::get_typeinfo_ptr() {
        if (typeinfo_ptr_type) { return typeinfo_ptr_type; }
        typeinfo_ptr_type = create_with_base(TypeKind::Pointer, get_basic(TypeKind::TypeInfo));
        return typeinfo_ptr_type;
    }

    TypeInfo* TypeInfo::get_char_slice() {
        if (char_slice_type) { return char_slice_type; }
        char_slice_type = create_with_base(TypeKind::Slice, get_basic(TypeKind::Char));
        return char_slice_type;
    }

    TypeInfo* TypeInfo::get_string() {
        if (context.opts->no_stdlib) {
            return get_char_slice();
        } else {
            if (std_core_string_type) { return std_core_string_type; }

            ARIA_ASSERT(context.std_core_module->symbols.contains("String"), "std::core module must contain a definition for 'String'");
            Decl* sym = context.std_core_module->symbols.at("String");
            
            ARIA_ASSERT(sym->kind == DeclKind::Typedef, "std::core::String must be a typedef");
            ARIA_ASSERT(sym->typedef_.type->kind == TypeKind::Slice && sym->typedef_.type->base->kind == TypeKind::Char, "std::core::String must be a typedef to char[]");

            std_core_string_type = create_typedef(sym);
            return std_core_string_type;
        }
    }

    bool TypeInfo::is_string() const {
        if (std_core_string_type) {
            return is_typedef() && typedef_.source_decl == std_core_string_type->typedef_.source_decl;
        } else {
            return is_slice() && base->kind == TypeKind::Char;
        }
    }

    bool TypeInfo::has_generic_integral_requirement() const {
        if (kind != TypeKind::Generic) { return false; }
        if (!generic.resolved_decl) { return false; }

        ARIA_ASSERT(generic.resolved_decl->kind == DeclKind::GenericParameter, "Invalid generic parameter");

        for (auto& req : generic.resolved_decl->generic_parameter.requirements) {
            if (req.kind == GenericRequirementKind::Integral) { return true; }
        }

        return false;
    }

    bool TypeInfo::has_generic_floating_requirement() const {
        if (kind != TypeKind::Generic) { return false; }
        if (!generic.resolved_decl) { return false; }

        ARIA_ASSERT(generic.resolved_decl->kind == DeclKind::GenericParameter, "Invalid generic parameter");

        for (auto& req : generic.resolved_decl->generic_parameter.requirements) {
            if (req.kind == GenericRequirementKind::FloatingPoint) { return true; }
        }

        return false;
    }

    u64 TypeInfo::get_size() const {
        switch (kind) {
            case TypeKind::Bool: return 1;

            case TypeKind::Char:
            case TypeKind::IChar: return 1;

            case TypeKind::Short:
            case TypeKind::UShort: return 2;

            case TypeKind::Int:
            case TypeKind::UInt: return 4;

            case TypeKind::Long:
            case TypeKind::ULong: return 8;

            case TypeKind::Sz:
            case TypeKind::Isz: {
                switch (context.opts->triple.getArch()) {
                    case llvm::Triple::x86: return 4;
                    case llvm::Triple::x86_64: return 8;

                    default: ARIA_TODO("Other arch");
                }
            }

            case TypeKind::Float: return 4;
            case TypeKind::Double: return 8;

            case TypeKind::TypeInfo: {
                return (get_string()->get_size() * 2) + (get_basic(TypeKind::Sz)->get_size() * 2) + get_char_slice()->get_size();
            }

            case TypeKind::Any: {
                return get_void_ptr()->get_size() * 2;
            }

            case TypeKind::Pointer: {
                switch (context.opts->triple.getArch()) {
                    case llvm::Triple::x86: return 4;
                    case llvm::Triple::x86_64: return 8;

                    default: ARIA_TODO("Other arch");
                }
            }

            case TypeKind::Array: return array.base->get_size() * array.size;

            case TypeKind::Slice: {
                return get_void_ptr()->get_size() + get_basic(TypeKind::Sz)->get_size();
            }

            case TypeKind::Structure: {
                u64 size = 0;
                u64 alignment = get_alignment();

                for (Decl* field : struct_.source_decl->struct_.fields) {
                    size += align_value(field->field.type->get_size(), alignment);
                }

                return size;
            }

            case TypeKind::Typedef: return typedef_.base->get_size();

            default: ARIA_UNREACHABLE("Invalid type kind");
        }
    }

    u64 TypeInfo::get_bit_size() const {
        switch (kind) {
            case TypeKind::Bool: return 1;

            case TypeKind::Char:
            case TypeKind::IChar: return 8;

            case TypeKind::Short:
            case TypeKind::UShort: return 16;

            case TypeKind::Int:
            case TypeKind::UInt: return 32;

            case TypeKind::Long:
            case TypeKind::ULong: return 64;

            case TypeKind::Sz:
            case TypeKind::Isz: {
                switch (context.opts->triple.getArch()) {
                    case llvm::Triple::x86: return 32;
                    case llvm::Triple::x86_64: return 64;

                    default: ARIA_TODO("Other arch");
                }
            }

            case TypeKind::Float: return 32;
            case TypeKind::Double: return 64;

            case TypeKind::Pointer: {
                switch (context.opts->triple.getArch()) {
                    case llvm::Triple::x86: return 32;
                    case llvm::Triple::x86_64: return 64;

                    default: ARIA_TODO("Other arch");
                }
            }

            case TypeKind::Slice: return get_size() * 8;

            case TypeKind::Structure: return get_size() * 8;

            case TypeKind::Typedef: return typedef_.base->get_bit_size();

            default: ARIA_UNREACHABLE("Invalid type kind");
        }
    }

    u64 TypeInfo::get_alignment() const {
        switch (kind) {
            case TypeKind::Bool:
            case TypeKind::Char:
            case TypeKind::IChar: return 1;
            case TypeKind::Short:
            case TypeKind::UShort: return 2;
            case TypeKind::Int:
            case TypeKind::UInt: return 4;
            case TypeKind::Long:
            case TypeKind::ULong: return 8;

            case TypeKind::Float: return 4;
            case TypeKind::Double: return 8;

            case TypeKind::Sz:
            case TypeKind::Isz: {
                if (context.opts->triple.getArch() == llvm::Triple::x86_64) { return 8; }
                else if (context.opts->triple.getArch() == llvm::Triple::x86) { return 4; }
                else { ARIA_UNREACHABLE("Invalid arch"); }

                return 0;
            }

            case TypeKind::TypeInfo: return TypeInfo::get_string()->get_alignment();
            case TypeKind::Any: return TypeInfo::get_void_ptr()->get_alignment();

            case TypeKind::Pointer: {
                if (context.opts->triple.getArch() == llvm::Triple::x86_64) { return 8; }
                else if (context.opts->triple.getArch() == llvm::Triple::x86) { return 4; }
                else { ARIA_UNREACHABLE("Invalid arch"); }

                return 0;
            }

            case TypeKind::Slice: {
                if (context.opts->triple.getArch() == llvm::Triple::x86_64) { return 8; }
                else if (context.opts->triple.getArch() == llvm::Triple::x86) { return 4; }
                else { ARIA_UNREACHABLE("Invalid arch"); }

                return 0;
            }

            case TypeKind::Structure: {
                u64 alignment = 0;

                for (Decl* field : struct_.source_decl->struct_.fields) {
                    u64 new_alignment = field->field.type->get_alignment();
                    alignment = (new_alignment > alignment) ? new_alignment : alignment;
                }

                return alignment;
            }

            case TypeKind::Typedef: return typedef_.base->get_alignment();

            default: ARIA_UNREACHABLE("Invalid type kind");
        }
    }

    TypeInfo* TypeInfo::get_bottom_type() const {
        TypeInfo* t = const_cast<TypeInfo*>(this);

        while (true) {
            if (t->is_primitive() || t->is_structure() || t->is_typedef() || t->is_enum() || t->is_generic()) {
                break;
            }

            if (t->is_pointer() || t->is_slice()) { t = t->base; continue; }
            if (t->is_array()) { t = t->array.base; continue; }

            ARIA_UNREACHABLE("Invald type");
        }

        return t;
    }

    std::string type_info_to_string(TypeInfo* type, bool pretty) {
        std::string str;

        switch (type->kind) {
            case TypeKind::Error:   str += "<error_type>"; break;
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

            case TypeKind::Sz:   str += "sz"; break;
            case TypeKind::Isz:   str += "isz"; break;

            case TypeKind::Float:   str += "float"; break;
            case TypeKind::Double:  str += "double"; break;

            case TypeKind::TypeInfo:  str += "typeinfo"; break;
            case TypeKind::Any:  str += "any"; break;

            case TypeKind::Pointer: {
                TypeInfo* t = type->base;
                str = fmt::format("*{}", type_info_to_string(t), pretty);
                break;
            }

            case TypeKind::Array: {
                ArrayType& arr = type->array;
                str = fmt::format("[{}]{}", arr.size, type_info_to_string(arr.base, pretty));
                break;
            }

            case TypeKind::Slice: {
                TypeInfo* t = type->base;
                str = fmt::format("[]{}", type_info_to_string(t, pretty));
                break;
            }

            case TypeKind::Function: {
                FunctionType ty = type->function;

                str += "(";

                for (size_t i = 0; i < ty.param_types.size; i++) {
                    str += type_info_to_string(ty.param_types.items[i], pretty);
                    if (i != ty.param_types.size - 1) {
                        str += ", ";
                    }
                }

                if (ty.variadic == VariadicKind::Unnamed) { str += ", ..."; }
                else if (ty.variadic == VariadicKind::Named) { str += "..."; }

                str += ")";
                str += fmt::format(" -> {}", type_info_to_string(ty.return_type, pretty));
                break;
            }

            case TypeKind::Method: {
                FunctionType ty = type->function;

                str += "(self";

                for (size_t i = 0; i < ty.param_types.size; i++) {
                    str += ", ";
                    str += type_info_to_string(ty.param_types.items[i], pretty);
                }

                if (ty.variadic == VariadicKind::Unnamed) { str += ", ..."; }
                else if (ty.variadic == VariadicKind::Named) { str += "..."; }

                str += ")";
                str += fmt::format(" -> {}", type_info_to_string(ty.return_type, pretty));
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
                    str += (ty.source_decl && ty.source_decl->parent_module) ? fmt::format("{}::{}", ty.source_decl->parent_module->name, ty.identifier) : fmt::format("{}", ty.identifier);
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
                str += fmt::format("{}<", type_info_to_string(type->generic_instantiation.base, pretty));

                for (size_t i = 0; i < type->generic_instantiation.arguments.size; i++) {
                    if (i > 0) { str += ", "; }
                    str += type_info_to_string(type->generic_instantiation.arguments.items[i], pretty);
                }

                str += ">";
                break;
            }

            case TypeKind::Unresolved: {
                str = "unresolved";
                break;
            }

            default: ARIA_UNREACHABLE("Invalid type kind");
        }

        return str;
    }

} // namespace ariac 
