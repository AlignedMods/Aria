#include "ariac/codegen/codegen.hpp"

namespace ariac {

    void Codegen::gen_var_decl(Decl* decl) {
        VarDecl& var = decl->var;
        if (var.const_var) { return;}

        llvm::Type* type = type_info_to_llvm_type(var.type);

        llvm::AllocaInst* a = nullptr;
        if (var.global_var) {
            std::string ident = fmt::format("{}.{}", valid_module_name(decl->parent_module->name), var.identifier);
            llvm::GlobalVariable* global = new llvm::GlobalVariable(*m_active_module_context.module, type, false, llvm::GlobalValue::LinkageTypes::ExternalLinkage, llvm::Constant::getNullValue(type), ident);
        } else {
            a = alloca_at_entry(m_active_module_context.function, var.identifier, var.type);
        }
        m_active_module_context.named_values[decl] = a;

        if (var.global_var) { return; }

        if (var.initializer) {
            gen_init_expr(var.initializer, a);
        } else {
            m_active_module_context.builder->CreateStore(llvm::Constant::getNullValue(type), m_active_module_context.named_values.at(decl));
        }
    }

    void Codegen::gen_function_decl(Decl* decl) {
        FunctionDecl& fn = decl->function;

        if (fn.linkage_kind == LinkageKind::Extern) { return; }

        if (!m_active_module_context.functions.contains(decl)) {
            gen_function_prototype(decl);
        }

        if (fn.body) {
            llvm::Function* function = m_active_module_context.functions.at(decl);
            m_active_module_context.function = function;

            m_ret_type_abi = get_ret_abi_type_info(fn.type->function.return_type);
            unsigned idx = m_ret_type_abi.ret_by_ptr ? 1 : 0;

            llvm::BasicBlock* bb = llvm::BasicBlock::Create(*m_active_module_context.context, "entry", function);
            m_active_module_context.builder->SetInsertPoint(bb);

            m_active_module_context.alloca_marker = m_active_module_context.builder->CreateUnreachable();

            for (Decl* param : fn.parameters) {
                ABIParamTypeInfo info = get_param_abi_type_info(param->param.type);

                if (info.pass_direct) {
                    llvm::AllocaInst* a = alloca_at_entry(function, param->param.identifier, param->param.type);
                    m_active_module_context.named_values[param] = a;

                    m_active_module_context.builder->CreateStore(function->getArg(static_cast<unsigned>(idx++)), a);
                } else if (info.pass_by_ptr) {
                    llvm::AllocaInst* a = alloca_at_entry(function, param->param.identifier, llvm::PointerType::get(*m_active_module_context.context, 0));
                    m_active_module_context.named_values[param] = a;

                    m_active_module_context.builder->CreateStore(function->getArg(static_cast<unsigned>(idx++)), a);
                } else if (info.pass_by_integer) {
                    llvm::AllocaInst* a = alloca_at_entry(m_active_module_context.function, param->param.identifier, param->param.type);
                    m_active_module_context.named_values[param] = a;

                    m_active_module_context.builder->CreateStore(function->getArg(static_cast<unsigned>(idx++)), a);
                } else {
                    ARIA_UNREACHABLE();
                }
            }

            gen_stmt(fn.body);
            m_active_module_context.alloca_marker->eraseFromParent();
            m_active_module_context.alloca_marker = nullptr;
            if (llvm::verifyFunction(*function, &llvm::errs())) { throw std::exception(); }
        }
    }

    void Codegen::gen_function_prototype(Decl* decl) {
        FunctionDecl& fn = decl->function;
        std::string sig;
        
        if (fn.linkage_kind == LinkageKind::Extern || fn.identifier == "main") {
            sig = fn.identifier;
        } else {
            sig = fmt::format("{}.{}", valid_module_name(decl->parent_module->name), fn.identifier);
        }

        llvm::Type* fn_ty = type_info_to_llvm_type(fn.type);
        llvm::Function* function = llvm::Function::Create(dyn_cast<llvm::FunctionType>(fn_ty), llvm::GlobalValue::LinkageTypes::ExternalLinkage, sig, m_active_module_context.module);
        m_active_module_context.functions[decl] = function;
    }

    void Codegen::gen_method_prototype(Decl* decl) {
        MethodDecl& m = decl->method;
        std::string_view parent_name;

        ARIA_ASSERT(m.parent->kind == DeclKind::Impl, "Invalid method parent");

        switch (m.parent->impl.parent->kind) {
            case DeclKind::Struct: parent_name = m.parent->impl.parent->struct_.identifier; break;
            case DeclKind::Typedef: parent_name = m.parent->impl.parent->typedef_.identifier; break;
            default: ARIA_UNREACHABLE();
        }

        std::string sig = fmt::format("{}.{}.{}", valid_module_name(m.parent->parent_module->name), parent_name, m.identifier);

        llvm::Type* fn_ty = type_info_to_llvm_type(m.type);
        llvm::Function* function = llvm::Function::Create(dyn_cast<llvm::FunctionType>(fn_ty), llvm::GlobalValue::LinkageTypes::ExternalLinkage, sig, m_active_module_context.module);
        m_active_module_context.functions[decl] = function;
    }

    void Codegen::gen_struct_decl(Decl* decl) {
        StructDecl& struc = decl->struct_;

        std::vector<llvm::Type*> fields;
        fields.reserve(struc.fields.size);
        std::string name = fmt::format("{}.{}", valid_module_name(decl->parent_module->name), struc.identifier);

        for (Decl* field : struc.fields) {
            fields.push_back(type_info_to_llvm_type(field->field.type));
        }

        llvm::StructType* type = llvm::StructType::create(fields, name);

        for (Decl* impl : struc.impls) {
            gen_impl_decl(impl);
        }
    }

    void Codegen::gen_impl_decl(Decl* decl) {
        ImplDecl& impl = decl->impl;

        for (Decl* field : impl.fields) {
            switch (field->kind) {
                case DeclKind::Method: {
                    MethodDecl& m = field->method;

                    if (!m_active_module_context.functions.contains(field)) {
                        gen_method_prototype(field);
                    }

                    llvm::Function* function = m_active_module_context.functions.at(field);
                    m_active_module_context.function = function;

                    if (m.body) {
                        m_ret_type_abi = get_ret_abi_type_info(m.type->function.return_type);
                        unsigned idx = m_ret_type_abi.ret_by_ptr ? 1 : 0;

                        llvm::BasicBlock* bb = llvm::BasicBlock::Create(*m_active_module_context.context, "entry", function);
                        m_active_module_context.builder->SetInsertPoint(bb);

                        m_active_module_context.alloca_marker = m_active_module_context.builder->CreateUnreachable();

                        // self
                        llvm::AllocaInst* s = alloca_at_entry(function, "self", llvm::PointerType::get(*m_active_module_context.context, 0));
                        m_self_value = s;
                        m_active_module_context.builder->CreateStore(function->getArg(idx++), s);

                        for (Decl* param : m.parameters) {
                            ABIParamTypeInfo info = get_param_abi_type_info(param->param.type);

                            if (info.pass_direct) {
                                llvm::AllocaInst* a = alloca_at_entry(function, param->param.identifier, param->param.type);
                                m_active_module_context.named_values[param] = a;

                                m_active_module_context.builder->CreateStore(function->getArg(static_cast<unsigned>(idx++)), a);
                            } else if (info.pass_by_ptr) {
                                llvm::AllocaInst* a = alloca_at_entry(function, param->param.identifier, llvm::PointerType::get(*m_active_module_context.context, 0));
                                m_active_module_context.named_values[param] = a;

                                m_active_module_context.builder->CreateStore(function->getArg(static_cast<unsigned>(idx++)), a);
                            } else if (info.pass_by_integer) {
                                llvm::AllocaInst* a = alloca_at_entry(m_active_module_context.function, param->param.identifier, param->param.type);
                                m_active_module_context.named_values[param] = a;

                                m_active_module_context.builder->CreateStore(function->getArg(static_cast<unsigned>(idx++)), a);
                            } else {
                                ARIA_UNREACHABLE();
                            }
                        }

                        gen_stmt(m.body);
                        m_active_module_context.alloca_marker->eraseFromParent();
                        m_active_module_context.alloca_marker = nullptr;
                        if (llvm::verifyFunction(*function, &llvm::errs())) { throw std::exception(); }
                    }

                    m_active_module_context.functions[field];
                    break;
                }
            }
        }
    }

    void Codegen::gen_decl(Decl* decl) {
        switch (decl->kind) {
            case DeclKind::Var: return gen_var_decl(decl);
            case DeclKind::Function: return gen_function_decl(decl);

            default: ARIA_UNREACHABLE();
        }
    }

} // namespace ariac