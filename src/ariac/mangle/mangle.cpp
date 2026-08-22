#include "ariac/mangle/mangle.hpp"
#include "ariac/core/scratch_buffer.hpp"
#include "ariac/types/type_info.hpp"

namespace ariac {

    std::string_view MangleContext::mangle() {
        scratch_buffer_clear();

        if (decl) {
            mangle_decl(decl);
        } else if (type) {
            mangle_type(type);
        }

        return scratch_buffer_to_str();
    }

    void MangleContext::mangle_module(Module* mod) {
        if (mod->parent) {
            mangle_module(mod->parent);
        }

        scratch_buffer_append(mod->name.length());
        scratch_buffer_append(mod->name);
    }

    void MangleContext::mangle_decl(Decl* decl) {
        switch (decl->kind) {
            case DeclKind::Function: {
                FunctionDecl& fn = decl->function;

                scratch_buffer_append("_A");
                mangle_module(decl->parent_module);
                scratch_buffer_append(fn.identifier.length());
                scratch_buffer_append(fn.identifier);

                if (fn.is_specilization) {
                    scratch_buffer_append("I");
                    for (TypeInfo* t : fn.specilization_info.types) {
                        mangle_type(t);
                    }
                }

                scratch_buffer_append("E");

                for (Decl* p : fn.type->function.params) {
                    mangle_type(p->param.type);
                }
                break;
            }

            case DeclKind::Struct: {
                StructDecl& s = decl->struct_;

                mangle_module(decl->parent_module);
                scratch_buffer_append(s.identifier.length());
                scratch_buffer_append(s.identifier);
                break;
            }

            case DeclKind::Method: {
                MethodDecl& m = decl->method;

                scratch_buffer_append("_A");
                mangle_decl(m.parent);
                scratch_buffer_append(m.identifier.length());
                scratch_buffer_append(m.identifier);

                scratch_buffer_append("E");

                for (Decl* p : m.type->function.params) {
                    mangle_type(p->param.type);
                }
                break;
            }

            default: ARIA_UNREACHABLE("Invalid decl kind");
        }
    }

    // Name mangling convention:
    //
    // Primitive types:
    //   Use the first letter of the type
    //   eg. char -> c, int -> i, long -> l
    //
    //   If the type is unsigned and the default is signed,
    //   use the letter AFTER it in the alphabet
    //   eg. ushort -> t, uint -> j
    //
    //   If the type is signed and the default is unsigned,
    //   use the letter BEFORE it in the alphabet
    //   eg. ichar -> b
    //
    //   If that letter is already taken, use the first unused letter
    //   eg. sz -> z, string -> r
    //
    // Pointer types:
    //   Add 'P'
    //   Add 'C' if it's a const pointer or 'M' if it's not const
    //   eg. *int -> PMi, *const char -> PCc
    // 
    // Array types:
    //   Add 'A'
    //   Add array size
    //   Add '_'
    //   Mangle the inner type
    //   eg. [4]int -> A4_i
    // 
    // Slice types:
    //   Add 'S'
    //   Mangle the inner type
    //   eg. []int -> Si
    // 
    // Struct/Enum types:
    //   Mangle the parent module
    //   Add the length of the name
    //   Add the name
    //   eg. std::core::Formatter -> 3std4core9Formatter
    //
    // Typedef types
    //   Mangle the inner type
    void MangleContext::mangle_type(TypeInfo* type) {
        switch (type->kind) {
            case TypeKind::Void: scratch_buffer_append("v"); break;
            case TypeKind::Bool: scratch_buffer_append("b"); break;
            case TypeKind::Char: scratch_buffer_append("c"); break;
            case TypeKind::IChar: scratch_buffer_append("b"); break;
            case TypeKind::Short: scratch_buffer_append("s"); break;
            case TypeKind::UShort: scratch_buffer_append("t"); break;
            case TypeKind::Int: scratch_buffer_append("i"); break;
            case TypeKind::UInt: scratch_buffer_append("j"); break;
            case TypeKind::Long: scratch_buffer_append("l"); break;
            case TypeKind::ULong: scratch_buffer_append("m"); break;
            case TypeKind::Sz: scratch_buffer_append("z"); break;
            case TypeKind::Isz: scratch_buffer_append("y"); break;

            case TypeKind::Float: scratch_buffer_append("f"); break;
            case TypeKind::Double: scratch_buffer_append("d"); break;

            case TypeKind::String: scratch_buffer_append("r"); break;

            case TypeKind::Typeid: scratch_buffer_append("y"); break;
            case TypeKind::Any: scratch_buffer_append("a"); break;

            case TypeKind::Pointer: {
                scratch_buffer_append('P');
                scratch_buffer_append(type->pointer.is_const ? 'C' : 'M');
                mangle_type(type->pointer.base);
                break;
            }

            case TypeKind::Array: {
                scratch_buffer_append('A');
                scratch_buffer_append(type->array.size);
                scratch_buffer_append('_');
                mangle_type(type->array.base);
                break;
            }

            case TypeKind::Slice: {
                scratch_buffer_append('S');
                mangle_type(type->slice.base);
                break;
            }

            case TypeKind::Struct: {
                mangle_module(type->struct_.source_decl->parent_module);
                scratch_buffer_append(type->struct_.identifier.length());
                scratch_buffer_append(type->struct_.identifier);
                break;
            }

            case TypeKind::Enum: {
                mangle_module(type->enum_.source_decl->parent_module);
                scratch_buffer_append(type->enum_.identifier.length());
                scratch_buffer_append(type->enum_.identifier);
                break;
            }

            case TypeKind::Typedef: mangle_type(type->typedef_.base); break;

            default: ARIA_UNREACHABLE("Invalid type kind");
        }
    }

} // namespace ariac