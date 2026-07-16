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
                            info.kind = ABIParamKind::Pointer;
                        } else if (t->is_structure()) {
                            u64 size = t->get_size();

                            if (size > 8) {
                                info.kind = ABIParamKind::Pointer;
                            } else {
                                if (size == 1 || size == 2 || size == 4 || size == 8) {
                                    info.kind = ABIParamKind::Integer;
                                    info.int_bits = size * 8;
                                } else {
                                    info.kind = ABIParamKind::Pointer;
                                }
                            }
                        } else {
                            info.kind = ABIParamKind::Direct;
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
                            info.kind = ABIRetKind::Pointer;
                        } else if (t->is_structure()) {
                            u64 size = t->get_size();

                            if (size > 8) {
                                info.kind = ABIRetKind::Pointer;
                            } else {
                                if (size == 1 || size == 2 || size == 4 || size == 8) {
                                    info.kind = ABIRetKind::Integer;
                                    info.int_bits = size * 8;
                                } else {
                                    info.kind = ABIRetKind::Pointer;
                                }
                            }
                        } else {
                            info.kind = ABIRetKind::Direct;
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

        switch (info.kind) {
            case ABIParamKind::Direct: {
                args->push_back(val);                
                break;
            }

            case ABIParamKind::Pointer: {
                llvm::Value* copy = alloca_at_entry(m_active_module_context.function, "calltemp", type);
                m_active_module_context.builder->CreateStore(val, copy);
                args->push_back(copy);
                break;
            }

            case ABIParamKind::Integer: {
                llvm::Value* copy = alloca_at_entry(m_active_module_context.function, "calltemp", type);
                m_active_module_context.builder->CreateStore(val, copy);
                llvm::Value* i = m_active_module_context.builder->CreateLoad(llvm::Type::getIntNTy(*m_active_module_context.context, static_cast<unsigned>(info.int_bits)), copy);
                args->push_back(i);
                break;
            }

            default: ARIA_UNREACHABLE("Invalid ABIParamTypeInfo");
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
            llvm::Value* curr_arg = m_active_module_context.builder->CreateGEP(arr_type, slots, { zero, idx }, "ptradd");

            m_active_module_context.builder->CreateStore(any, curr_arg);
            arg_idx++;
        }

        llvm::Value* slice = alloca_at_entry(m_active_module_context.function, "arr_to_slice", slice_type);
        
        llvm::Value* mem = m_active_module_context.builder->CreateStructGEP(slice_type, slice, 0, "ptradd");
        m_active_module_context.builder->CreateStore(slots, mem);

        llvm::Value* len = m_active_module_context.builder->CreateStructGEP(slice_type, slice, 1, "ptradd");
        m_active_module_context.builder->CreateStore(m_active_module_context.builder->getInt(llvm::APInt((unsigned)TypeInfo::get_basic(TypeKind::Sz)->get_bit_size(), arr_type->getArrayNumElements())), len);

        ABIParamTypeInfo abi_info = get_param_abi_type_info(TypeInfo::get_char_slice());

        switch (abi_info.kind) {
            case ABIParamKind::Pointer: {
                args->push_back(slice);
                break;
            }

            case ABIParamKind::Integer: {
                args->push_back(m_active_module_context.builder->CreateLoad(llvm::Type::getIntNTy(*m_active_module_context.context, static_cast<unsigned>(abi_info.int_bits)), slice));
                break;
            }

            default: ARIA_UNREACHABLE("Invalid ABIParamTypeInfo");
        }
    }

    llvm::Value* Codegen::gen_call_raw(std::vector<llvm::Value*>& args, llvm::Function* func, TypeInfo* ret_type) {
        ABIRetTypeInfo ret_info = get_ret_abi_type_info(ret_type);
        switch (ret_info.kind) {
            case ABIRetKind::Direct: {
                return m_active_module_context.builder->CreateCall(func, args, ret_type->is_void() ? "" : "call");
            }

            case ABIRetKind::Pointer: {
                llvm::Type* ret_type = type_info_to_llvm_type(ret_info.type);
                llvm::Value* ret_val = alloca_at_entry(m_active_module_context.function, "ptrret", ret_type);
                args.insert(args.begin(), ret_val);

                m_active_module_context.builder->CreateCall(func, args);
                return m_active_module_context.builder->CreateLoad(ret_type, ret_val);
            }

            case ABIRetKind::Integer: {
                llvm::Type* ret_type = type_info_to_llvm_type(ret_info.type);
                llvm::Value* temp = alloca_at_entry(m_active_module_context.function, "intret", ret_type);
                llvm::Value* call = m_active_module_context.builder->CreateCall(func, args, "call");

                m_active_module_context.builder->CreateStore(call, temp);
                return m_active_module_context.builder->CreateLoad(ret_type, temp);
            }

            default: ARIA_UNREACHABLE("Invalid ABIRetTypeInfo");
        }
    }

} // namespace ariac