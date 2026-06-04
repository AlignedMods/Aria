#include "ariac/codegen/codegen.hpp"

namespace Aria::Internal {

    void Codegen::gen_var_decl(Decl* decl) {
        VarDecl& var = decl->var;
        llvm::Type* type = type_info_to_llvm_type(var.type);

        llvm::AllocaInst* a = nullptr;
        if (var.global_var) {
            std::string ident = fmt::format("{}.{}", valid_module_name(decl->parent_module->name), var.identifier);
            llvm::GlobalVariable* global = new llvm::GlobalVariable(*m_active_module_context.module, type, false, llvm::GlobalValue::LinkageTypes::ExternalLinkage, llvm::Constant::getNullValue(type), ident);
        } else {
            a = alloca_at_entry(m_active_module_context.function, var.identifier, var.type);
        }
        m_named_values[decl] = a;

        if (var.global_var) { return; }

        if (var.initializer) {
            gen_init_expr(var.initializer, a);
        } else {
            m_active_module_context.builder->CreateStore(llvm::Constant::getNullValue(type), m_named_values.at(decl));
        }
    }

    void Codegen::gen_function_decl(Decl* decl) {
        FunctionDecl& fn = decl->function;

        if (fn.linkage_kind == LinkageKind::Extern) { return; }

        if (!m_functions.contains(decl)) {
            gen_function_prototype(decl);
        }

        llvm::Function* function = m_functions.at(decl);
        m_active_module_context.function = function;

        if (fn.body) {
            llvm::BasicBlock* bb = llvm::BasicBlock::Create(*m_active_module_context.context, "entry", function);
            m_active_module_context.builder->SetInsertPoint(bb);

            m_active_module_context.alloca_marker = m_active_module_context.builder->CreateUnreachable();

            size_t idx = 0;
            for (Decl* param : fn.parameters) {
                ABITypeInfo info = get_abi_type_info(param->param.type);

                if (info.pass_direct) {
                    llvm::AllocaInst* a = alloca_at_entry(function, param->param.identifier, param->param.type);
                    m_named_values[param] = a;

                    m_active_module_context.builder->CreateStore(function->getArg(static_cast<unsigned>(idx++)), a);
                } else if (info.pass_by_ptr) {
                    llvm::AllocaInst* a = alloca_at_entry(function, param->param.identifier, &void_ptr_type);
                    m_named_values[param] = a;

                    m_active_module_context.builder->CreateStore(function->getArg(static_cast<unsigned>(idx++)), a);
                }
            }

            gen_stmt(fn.body);
            m_active_module_context.alloca_marker->eraseFromParent();
            m_active_module_context.alloca_marker = nullptr;
            llvm::verifyFunction(*function, &llvm::errs());
        }
    }

    void Codegen::gen_function_prototype(Decl* decl) {
        FunctionDecl& fn = decl->function;
        std::string sig = mangle_function(decl);

        std::vector<llvm::Type*> params;
        for (Decl* p : fn.parameters) {
            ABITypeInfo info = get_abi_type_info(p->param.type);

            if (info.pass_by_ptr) {
                params.push_back(type_info_to_llvm_type(&void_ptr_type));
            } else if (info.pass_direct) {
                params.push_back(type_info_to_llvm_type(p->param.type));
            }
        }

        llvm::FunctionType* fn_ty = llvm::FunctionType::get(type_info_to_llvm_type(fn.type->function.return_type), params, fn.type->function.var_arg);
        llvm::Function* function = llvm::Function::Create(fn_ty, llvm::GlobalValue::LinkageTypes::ExternalLinkage, sig, m_active_module_context.module);
        m_functions[decl] = function;
    }

    void Codegen::gen_method_prototype(Decl* decl) {
        MethodDecl& m = decl->method;
        std::string sig = mangle_method(&m);

        std::vector<llvm::Type*> params;
        params.push_back(type_info_to_llvm_type(&void_ptr_type));

        for (Decl* p : m.parameters) {
            ABITypeInfo info = get_abi_type_info(p->param.type);

            if (info.pass_by_ptr) {
                params.push_back(type_info_to_llvm_type(&void_ptr_type));
            } else if (info.pass_direct) {
                params.push_back(type_info_to_llvm_type(p->param.type));
            }
        }

        llvm::FunctionType* fn_ty = llvm::FunctionType::get(type_info_to_llvm_type(m.type->function.return_type), params, m.type->function.var_arg);
        llvm::Function* function = llvm::Function::Create(fn_ty, llvm::GlobalValue::LinkageTypes::ExternalLinkage, sig, m_active_module_context.module);
        m_functions[decl] = function;
    }

    void Codegen::gen_dtor_prototype(Decl* decl) {
        DestructorDecl& dtor = decl->destructor;
        std::string sig = mangle_dtor(&dtor);

        llvm::FunctionType* fn_ty = llvm::FunctionType::get(type_info_to_llvm_type(&void_type), { type_info_to_llvm_type(&void_ptr_type) }, false);
        llvm::Function* function = llvm::Function::Create(fn_ty, llvm::GlobalValue::LinkageTypes::ExternalLinkage, sig, m_active_module_context.module);
        m_functions[decl] = function;
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

                    if (!m_functions.contains(field)) {
                        gen_method_prototype(field);
                    }

                    llvm::Function* function = m_functions.at(field);
                    m_active_module_context.function = function;

                    if (m.body) {
                        llvm::BasicBlock* bb = llvm::BasicBlock::Create(*m_active_module_context.context, "entry", function);
                        m_active_module_context.builder->SetInsertPoint(bb);

                        m_active_module_context.alloca_marker = m_active_module_context.builder->CreateUnreachable();

                        // self
                        llvm::AllocaInst* s = alloca_at_entry(function, "self", &void_ptr_type);
                        m_self_value = s;
                        m_active_module_context.builder->CreateStore(function->getArg(0), s);

                        size_t idx = 1;
                        for (Decl* param : m.parameters) {
                            ABITypeInfo info = get_abi_type_info(param->param.type);

                            if (info.pass_direct) {
                                llvm::AllocaInst* a = alloca_at_entry(function, param->param.identifier, param->param.type);
                                m_named_values[param] = a;

                                m_active_module_context.builder->CreateStore(function->getArg(static_cast<unsigned>(idx++)), a);
                            } else if (info.pass_by_ptr) {
                                llvm::AllocaInst* a = alloca_at_entry(function, param->param.identifier, &void_ptr_type);
                                m_named_values[param] = a;

                                m_active_module_context.builder->CreateStore(function->getArg(static_cast<unsigned>(idx++)), a);
                            }
                        }

                        gen_stmt(m.body);
                        m_active_module_context.alloca_marker->eraseFromParent();
                        m_active_module_context.alloca_marker = nullptr;
                        llvm::verifyFunction(*function, &llvm::errs());
                    }

                    m_functions[field];
                    break;
                }

                case DeclKind::Destructor: {
                    DestructorDecl& dtor = field->destructor;

                    if (!m_functions.contains(field)) {
                        gen_dtor_prototype(field);
                    }

                    llvm::Function* function = m_functions.at(field);
                    m_active_module_context.function = function;

                    llvm::BasicBlock* bb = llvm::BasicBlock::Create(*m_active_module_context.context, "entry", function);
                    m_active_module_context.builder->SetInsertPoint(bb);

                    m_active_module_context.alloca_marker = m_active_module_context.builder->CreateUnreachable();

                    // self
                    llvm::AllocaInst* s = alloca_at_entry(function, "self", &void_ptr_type);
                    m_self_value = s;
                    m_active_module_context.builder->CreateStore(function->getArg(0), s);

                    gen_stmt(dtor.body);
                    m_active_module_context.alloca_marker->eraseFromParent();
                    m_active_module_context.alloca_marker = nullptr;
                    llvm::verifyFunction(*function, &llvm::errs());

                    m_functions[field];
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

} // namespace Aria::Internal