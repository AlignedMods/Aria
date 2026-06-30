#include "ariac/codegen/codegen.hpp"

namespace ariac {

    Codegen::ABIParamTypeInfo Codegen::get_param_abi_type_info(TypeInfo* t) {
        ABIParamTypeInfo info;
        info.type = t;
        
        switch (m_triple.getOS()) {
            case llvm::Triple::OSType::Win32: {
                switch (m_triple.getArch()) {
                    case llvm::Triple::ArchType::x86_64: {
                        while (t->is_typedef()) {
                            t = t->typedef_.base;
                        }
                        
                        if (t->is_slice()) {
                            info.pass_by_ptr = true;
                        } else if (t->is_structure()) {
                            size_t size = get_type_size(t);

                            if (size > 8) {
                                info.pass_by_ptr = true;
                            } else {
                                if (size == 1 || size == 2 || size == 4 || size == 8) {
                                    info.pass_by_integer = true; 
                                    info.int_bits = size * 8;
                                } else {
                                    info.pass_by_ptr = true;
                                }
                            }
                        } else {
                            info.pass_direct = true;
                        }

                        break;
                    }

                    default: ARIA_UNREACHABLE();
                }

                break;
            } // Win32

            default: ARIA_UNREACHABLE();
        }

        return info;
    }

    Codegen::ABIRetTypeInfo Codegen::get_ret_abi_type_info(TypeInfo* t) {
        ABIRetTypeInfo info;
        info.type = t;

        switch (m_triple.getOS()) {
            case llvm::Triple::OSType::Win32: {
                switch (m_triple.getArch()) {
                    case llvm::Triple::ArchType::x86_64: {
                        if (t->is_slice()) {
                            info.ret_by_ptr = true;
                        } else if (t->is_structure()) {
                            size_t size = get_type_size(t);

                            if (size > 8) {
                                info.ret_by_ptr = true;
                            } else {
                                if (size == 1 || size == 2 || size == 4 || size == 8) {
                                    info.ret_by_integer = true; 
                                    info.int_bits = size * 8;
                                } else {
                                    info.ret_by_ptr = true;
                                }
                            }
                        } else {
                            info.ret_direct = true;
                        }

                        break;
                    }

                    default: ARIA_UNREACHABLE();
                }

                break;
            } // Win32

            default: ARIA_UNREACHABLE();
        }

        return info;
    }

} // namespace ariac