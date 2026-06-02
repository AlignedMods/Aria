#include "ariac/types/type_info.hpp"
#include "ariac/ast/decl.hpp"

namespace Aria::Internal {

    TypeInfo* TypeInfo::Create(CompilationContext* ctx, TypeKind kind) {
        TypeInfo* t = ctx->allocate<TypeInfo>();
        t->kind = kind;
    
        return t;
    }

    TypeInfo* TypeInfo::Dup(CompilationContext* ctx, TypeInfo* type) {
        TypeInfo* t = ctx->allocate<TypeInfo>();
        memcpy(reinterpret_cast<void*>(t), type, sizeof(TypeInfo));
        return t;
    }

    std::string type_info_to_string(TypeInfo* type) {
        std::string str;

        switch (type->kind) {
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
                TypeInfo* t = type->base;
                str = fmt::format("{}*", type_info_to_string(t));
                break;
            }

            case TypeKind::Array: {
                ArrayDeclaration& arr = type->array;
                str = fmt::format("{}[{}]", type_info_to_string(arr.type), arr.size);
                break;
            }

            case TypeKind::Slice: {
                TypeInfo* t = type->base;
                str = fmt::format("{}[]", type_info_to_string(t));
                break;
            }

            case TypeKind::Ref: {
                TypeInfo* t = type->base;
                str = fmt::format("{}&", type_info_to_string(t));
                break;
            }

            case TypeKind::String: str = "string"; break;

            case TypeKind::Function: {
                FunctionDeclaration decl = type->function;

                str += type_info_to_string(decl.return_type);
                str += "(";

                for (size_t i = 0; i < decl.param_types.size; i++) {
                    str += type_info_to_string(decl.param_types.items[i]);
                    if (i != decl.param_types.size - 1) {
                        str += ", ";
                    }
                }

                if (decl.var_arg) { str += ", ..."; }

                str += ")";
                break;
            }

            case TypeKind::Method: {
                FunctionDeclaration decl = type->function;

                str += type_info_to_string(decl.return_type);
                str += "(";

                for (size_t i = 0; i < decl.param_types.size; i++) {
                    str += type_info_to_string(decl.param_types.items[i]);
                    if (i != decl.param_types.size - 1) {
                        str += ", ";
                    }
                }

                if (decl.var_arg) { str += ", ..."; }

                str += ")";
                break;
            }

            case TypeKind::Structure: {
                StructDeclaration decl = type->struct_;

                str = (decl.source_decl && decl.source_decl->parent_module) ? fmt::format("struct {}::{}", decl.source_decl->parent_module->name, decl.identifier) : fmt::format("struct {}", decl.identifier);
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

} // namespace Aria::Internal 
