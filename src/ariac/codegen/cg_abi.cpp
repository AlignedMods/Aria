#include "ariac/codegen/codegen.hpp"

namespace ariac {
    
    Codegen::ABIParamTypeInfo Codegen::get_param_abi_type_info(TypeInfo* t) {
        ABIParamTypeInfo info;
        info.type = t;
        
        switch (context.opts->triple.getOS()) {
            case llvm::Triple::OSType::Win32: {
                switch (context.opts->triple.getArch()) {
                    case llvm::Triple::ArchType::x86_64: {
                        while (t->is_typedef()) {
                            t = t->typedef_.base;
                        }
                        
                        if (t->is_slice()) {
                            info.pass_by_ptr = true;
                        } else if (t->is_structure()) {
                            u64 size = t->get_size();

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

                    default: ARIA_UNREACHABLE("Invalid arch");
                }

                break;
            } // Win32

            default: ARIA_UNREACHABLE("Invalid OS");
        }

        return info;
    }

    Codegen::ABIRetTypeInfo Codegen::get_ret_abi_type_info(TypeInfo* t) {
        ABIRetTypeInfo info;
        info.type = t;

        switch (context.opts->triple.getOS()) {
            case llvm::Triple::OSType::Win32: {
                switch (context.opts->triple.getArch()) {
                    case llvm::Triple::ArchType::x86_64: {
                        if (t->is_slice()) {
                            info.ret_by_ptr = true;
                        } else if (t->is_structure()) {
                            u64 size = t->get_size();

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

                    default: ARIA_UNREACHABLE("Invalid arch");
                }

                break;
            } // Win32

            default: ARIA_UNREACHABLE("Invalid OS");
        }

        return info;
    }

    void Codegen::gen_call_param(std::vector<llvm::Value*>* args, llvm::Value* val, TypeInfo* type) {
        ABIParamTypeInfo info = get_param_abi_type_info(type);

        if (info.pass_direct) {
            args->push_back(val);
        } else if (info.pass_by_ptr) {
            llvm::Value* copy = alloca_at_entry(m_active_module_context.function, "calltemp", type);
            m_active_module_context.builder->CreateStore(val, copy);
            args->push_back(copy);
        } else if (info.pass_by_integer) {
            llvm::Value* copy = alloca_at_entry(m_active_module_context.function, "calltemp", type);
            m_active_module_context.builder->CreateStore(val, copy);
            llvm::Value* i = m_active_module_context.builder->CreateLoad(llvm::Type::getIntNTy(*m_active_module_context.context, static_cast<unsigned>(info.int_bits)), copy);
            args->push_back(i);
        } else {
            ARIA_UNREACHABLE("Invalid ABIParamTypeInfo");
        }
    }

    void Codegen::gen_call_variadic(std::vector<llvm::Value*>* args, const std::vector<llvm::Value*>& vals, const std::vector<TypeInfo*>& types) {
        ARIA_ASSERT(vals.size() == types.size(), "Sizes must be same");

        llvm::Type* slice_type = llvm::StructType::getTypeByName(*m_active_module_context.context, "$builtin_slice");
        llvm::Type* any_type = llvm::StructType::getTypeByName(*m_active_module_context.context, "$builtin_any");
        llvm::Type* arr_type = llvm::ArrayType::get(any_type, vals.size());
        llvm::Value* slots = alloca_at_entry(m_active_module_context.function, "varargslots", arr_type);

        size_t arg_idx = 0;
        for (llvm::Value* val : vals) {
            llvm::Value* taddr = alloca_at_entry(m_active_module_context.function, "tempaddr", val->getType());

            llvm::Value* ti = get_typeinfo(types[arg_idx]);
            m_active_module_context.builder->CreateStore(val, taddr);

            llvm::Value* undef = llvm::UndefValue::get(any_type);
            llvm::Value* any = m_active_module_context.builder->CreateInsertValue(undef, ti, 0);
            any = m_active_module_context.builder->CreateInsertValue(any, taddr, 1);

            llvm::Value* zero = m_active_module_context.builder->getInt32(0);
            llvm::Value* idx = m_active_module_context.builder->getInt32(static_cast<i32>(arg_idx));
            llvm::Value* curr_arg = m_active_module_context.builder->CreateGEP(arr_type, slots, { zero, idx });

            m_active_module_context.builder->CreateStore(any, curr_arg);
            arg_idx++;
        }

        llvm::Value* slice = alloca_at_entry(m_active_module_context.function, "arr_to_slice", slice_type);
        
        llvm::Value* mem = m_active_module_context.builder->CreateStructGEP(slice_type, slice, 0);
        m_active_module_context.builder->CreateStore(slots, mem);

        llvm::Value* len = m_active_module_context.builder->CreateStructGEP(slice_type, slice, 1);
        m_active_module_context.builder->CreateStore(m_active_module_context.builder->getInt(llvm::APInt(TypeInfo::get_basic(TypeKind::Sz)->get_bit_size(), arr_type->getArrayNumElements())), len);

        ABIParamTypeInfo abi_info = get_param_abi_type_info(TypeInfo::get_char_slice());

        if (abi_info.pass_by_integer) {
            args->push_back(m_active_module_context.builder->CreateLoad(llvm::Type::getIntNTy(*m_active_module_context.context, static_cast<unsigned>(abi_info.int_bits)), slice));
        } else if (abi_info.pass_by_ptr) {
            args->push_back(slice);
        } else {
            ARIA_UNREACHABLE("Invalid ABIParamTypeInfo");
        }
    }

} // namespace ariac