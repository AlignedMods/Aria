#include "ariac/codegen/codegen.hpp"

namespace ariac {

    void Codegen::gen_var_decl(Decl* decl) {
        VarDecl& var = decl->var;
        set_debug_loc(decl->loc);
        if (var.const_var) { return;}

        llvm::Type* type = type_info_to_llvm_type(var.type);
        if (type->isIntegerTy(1)) { type = llvm::Type::getInt8Ty(*m_active_module_context.context); }

        llvm::Value* a = nullptr;
        if (var.global_var) {
            std::string ident = fmt::format("{}.{}", valid_module_name(decl->parent_module->name), var.identifier);
            llvm::GlobalVariable* global = new llvm::GlobalVariable(*m_active_module_context.module, type, false, linkage_kind_to_llvm(var.linkage_kind), llvm::Constant::getNullValue(type), ident);
            a = global;
        } else {
            a = alloca_at_entry(m_active_module_context.function, var.identifier, var.type);

            llvm::DILocalVariable* dil = m_active_debug_context.builder->createAutoVariable(m_active_debug_context.scope, var.identifier, m_active_debug_context.scope->getFile(), 
                decl->loc.line, type_info_to_debug_type(var.type));

            m_active_debug_context.builder->insertDeclare(a,
                dil, m_active_debug_context.builder->createExpression(), 
                llvm::DILocation::get(*m_active_module_context.context, decl->loc.line, decl->loc.col, m_active_debug_context.scope), m_active_module_context.builder->GetInsertBlock());
        }

        ARIA_ASSERT(a, "Invalid var decl");
        m_active_module_context.named_values[decl] = a;

        if (var.global_var) { return; }

        if (var.initializer) {
            gen_init_expr(var.initializer, a);
        } else {
            m_active_module_context.builder->CreateStore(llvm::Constant::getNullValue(type), m_active_module_context.named_values.at(decl));
        }
    }

    void Codegen::gen_function_decl(Decl* decl) {
        FunctionDecl* fn = nullptr;

        if (decl->kind == DeclKind::Function) {
            fn = &decl->function;
        } else if (decl->kind == DeclKind::FunctionSpecilization) {
            fn = &decl->function_specilization.source->function;
        } else {
            ARIA_UNREACHABLE("Invalid function decl");
        }

        if (fn->linkage_kind == LinkageKind::Extern) { return; }

        if (!m_active_module_context.functions.contains(decl)) {
            gen_function_prototype(decl);
        }

        if (fn->body) {
            llvm::Function* function = m_active_module_context.functions.at(decl);
            m_active_module_context.function = function;
            function->setDSOLocal(true);

            llvm::DISubprogram* sp = m_active_debug_context.builder->createFunction(m_active_debug_context.unit->getFile(),
                function->getName(), {}, m_active_debug_context.unit->getFile(), decl->loc.line,
                m_active_debug_context.builder->createSubroutineType({}), decl->loc.line, llvm::DINode::FlagPrototyped, llvm::DISubprogram::SPFlagDefinition);

            function->setSubprogram(sp);
            m_active_debug_context.scope = sp;

            // Do not set any source locations for the function prologue
            set_debug_loc({});

            m_ret_type_abi = get_ret_abi_type_info(fn->type->function.return_type);
            unsigned idx = m_ret_type_abi.kind == ABIRetKind::Pointer ? 1 : 0;

            if (m_ret_type_abi.type->is_boolean()) {
                function->addRetAttr(llvm::Attribute::ZExt);
            }

            llvm::BasicBlock* bb = llvm::BasicBlock::Create(*m_active_module_context.context, "entry", function);
            m_active_module_context.builder->SetInsertPoint(bb);

            m_active_module_context.alloca_marker = m_active_module_context.builder->CreateUnreachable();

            for (Decl* param : fn->parameters) {
                TypeInfo* param_type = param->param.variadic ? TypeInfo::create_slice(param->param.type) : param->param.type;
                ABIParamTypeInfo info = get_param_abi_type_info(param_type);

                llvm::DILocalVariable* dil = nullptr;

                switch (info.kind) {
                    case ABIParamKind::Direct: {
                        llvm::AllocaInst* a = alloca_at_entry(function, param->param.identifier, param_type);
                        m_active_module_context.named_values[param] = a;

                        unsigned ui = static_cast<unsigned>(idx++);

                        if (param_type->is_boolean()) {
                            function->addParamAttr(ui, llvm::Attribute::ZExt);
                            llvm::Value* zext = m_active_module_context.builder->CreateZExt(function->getArg(ui), llvm::Type::getInt8Ty(*m_active_module_context.context), "zext");
                            m_active_module_context.builder->CreateStore(zext, a);
                        } else {
                            m_active_module_context.builder->CreateStore(function->getArg(ui), a);
                        }

                        dil = m_active_debug_context.builder->createParameterVariable(sp, param->param.identifier, idx + 1, sp->getFile(), 
                            decl->loc.line, type_info_to_debug_type(param_type));
                        break;
                    }

                    case ABIParamKind::Pointer: {
                        llvm::AllocaInst* a = alloca_at_entry(function, param->param.identifier, llvm::PointerType::get(*m_active_module_context.context, 0));
                        m_active_module_context.named_values[param] = a;

                        m_active_module_context.builder->CreateStore(function->getArg(static_cast<unsigned>(idx++)), a);

                        dil = m_active_debug_context.builder->createParameterVariable(sp, param->param.identifier, idx + 1, sp->getFile(), 
                            decl->loc.line, type_info_to_debug_type(TypeInfo::create_pointer(param_type, false)));
                        break;
                    }

                    case ABIParamKind::Integer: {
                        llvm::AllocaInst* a = alloca_at_entry(m_active_module_context.function, param->param.identifier, param_type);
                        m_active_module_context.named_values[param] = a;

                        m_active_module_context.builder->CreateStore(function->getArg(static_cast<unsigned>(idx++)), a);

                        dil = m_active_debug_context.builder->createParameterVariable(sp, param->param.identifier, idx + 1, sp->getFile(), 
                            decl->loc.line, type_info_to_debug_type(param_type));
                        break;
                    }

                    default: ARIA_UNREACHABLE("Invalid ABIParamTypeInfo");
                }

                ARIA_ASSERT(dil, "Must set the debug local variable");
                m_active_debug_context.builder->insertDeclare(m_active_module_context.named_values.at(param),
                    dil, m_active_debug_context.builder->createExpression(), 
                    llvm::DILocation::get(*m_active_module_context.context, decl->loc.line, decl->loc.col, sp), m_active_module_context.builder->GetInsertBlock());
            }

            gen_stmt(fn->body);
            m_active_module_context.alloca_marker->eraseFromParent();
            m_active_module_context.alloca_marker = nullptr;
            if (llvm::verifyFunction(*function, &llvm::errs())) { throw std::exception(); }
        }
    }

    void Codegen::gen_function_prototype(Decl* decl) {
        std::string sig;

        if (decl->kind == DeclKind::Function) {
            FunctionDecl& fn = decl->function;
            if (fn.linkage_kind == LinkageKind::Extern) {
                sig = fn.identifier;
            } else {
                sig = fmt::format("{}.{}", valid_module_name(decl->parent_module->name), fn.identifier);
            }

            llvm::Type* fn_ty = type_info_to_llvm_type(fn.type);
            llvm::Function* function = llvm::Function::Create(dyn_cast<llvm::FunctionType>(fn_ty), linkage_kind_to_llvm(fn.linkage_kind), 0, sig, m_active_module_context.module);
            m_active_module_context.functions[decl] = function;

            return;
        } else if (decl->kind == DeclKind::FunctionSpecilization) {
            FunctionSpecilizationDecl& fn = decl->function_specilization;
            sig = fmt::format("{}.{}__G{}", valid_module_name(decl->parent_module->name), fn.source->function.identifier, fn.types.size);
            for (TypeInfo* t : fn.types) { sig += mangle_type(t); }

            llvm::Type* fn_ty = type_info_to_llvm_type(fn.source->function.type);
            llvm::Function* function = llvm::Function::Create(dyn_cast<llvm::FunctionType>(fn_ty), linkage_kind_to_llvm(fn.source->function.linkage_kind), 0, sig, m_active_module_context.module);
            m_active_module_context.functions[decl] = function;

            return;
        }

        ARIA_UNREACHABLE("Invalid function prototype");
    }

    void Codegen::gen_method_prototype(Decl* decl) {
        MethodDecl& m = decl->method;
        std::string_view parent_name;

        ARIA_ASSERT(m.parent->kind == DeclKind::Impl, "Invalid method parent");

        switch (m.parent->impl.parent->kind) {
            case DeclKind::Struct: parent_name = m.parent->impl.parent->struct_.identifier; break;
            case DeclKind::Typedef: parent_name = m.parent->impl.parent->typedef_.identifier; break;
            default: ARIA_UNREACHABLE("Invalid method parent");
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

                    llvm::DISubprogram* sp = m_active_debug_context.builder->createFunction(m_active_debug_context.unit->getFile(),
                        function->getName(), {}, m_active_debug_context.unit->getFile(), decl->loc.line,
                        m_active_debug_context.builder->createSubroutineType({}), decl->loc.line, llvm::DINode::FlagPrototyped, llvm::DISubprogram::SPFlagDefinition);

                    function->setSubprogram(sp);
                    m_active_debug_context.scope = sp;

                    // Don't set any source locations for the prologue
                    set_debug_loc({});

                    if (m.body) {
                        m_ret_type_abi = get_ret_abi_type_info(m.type->function.return_type);
                        unsigned idx = m_ret_type_abi.kind == ABIRetKind::Pointer ? 1 : 0;

                        llvm::BasicBlock* bb = llvm::BasicBlock::Create(*m_active_module_context.context, "entry", function);
                        m_active_module_context.builder->SetInsertPoint(bb);

                        m_active_module_context.alloca_marker = m_active_module_context.builder->CreateUnreachable();

                        // self
                        llvm::AllocaInst* s = alloca_at_entry(function, "self", llvm::PointerType::get(*m_active_module_context.context, 0));
                        m_self_value = s;
                        m_active_module_context.builder->CreateStore(function->getArg(idx++), s);

                        for (Decl* param : m.parameters) {
                            TypeInfo* param_type = param->param.variadic ? TypeInfo::create_slice(param->param.type) : param->param.type;
                            ABIParamTypeInfo info = get_param_abi_type_info(param->param.type);

                            llvm::DILocalVariable* dil = nullptr;

                            switch (info.kind) {
                                case ABIParamKind::Direct: {
                                    llvm::AllocaInst* a = alloca_at_entry(function, param->param.identifier, param_type);
                                    m_active_module_context.named_values[param] = a;

                                    unsigned ui = static_cast<unsigned>(idx++);

                                    if (param_type->is_boolean()) {
                                        function->addParamAttr(ui, llvm::Attribute::ZExt);
                                        llvm::Value* zext = m_active_module_context.builder->CreateZExt(function->getArg(ui), llvm::Type::getInt8Ty(*m_active_module_context.context), "zext");
                                        m_active_module_context.builder->CreateStore(zext, a);
                                    } else {
                                        m_active_module_context.builder->CreateStore(function->getArg(ui), a);
                                    }

                                    dil = m_active_debug_context.builder->createParameterVariable(sp, param->param.identifier, idx + 1, sp->getFile(), 
                                        decl->loc.line, type_info_to_debug_type(param_type));
                                    break;
                                }

                                case ABIParamKind::Pointer: {
                                    llvm::AllocaInst* a = alloca_at_entry(function, param->param.identifier, llvm::PointerType::get(*m_active_module_context.context, 0));
                                    m_active_module_context.named_values[param] = a;

                                    m_active_module_context.builder->CreateStore(function->getArg(static_cast<unsigned>(idx++)), a);

                                    dil = m_active_debug_context.builder->createParameterVariable(sp, param->param.identifier, idx + 1, sp->getFile(), 
                                        decl->loc.line, type_info_to_debug_type(TypeInfo::create_pointer(param_type, false)));
                                    break;
                                }

                                case ABIParamKind::Integer: {
                                    llvm::AllocaInst* a = alloca_at_entry(m_active_module_context.function, param->param.identifier, param_type);
                                    m_active_module_context.named_values[param] = a;

                                    m_active_module_context.builder->CreateStore(function->getArg(static_cast<unsigned>(idx++)), a);

                                    dil = m_active_debug_context.builder->createParameterVariable(sp, param->param.identifier, idx + 1, sp->getFile(), 
                                        decl->loc.line, type_info_to_debug_type(param_type));
                                    break;
                                }

                                default: ARIA_UNREACHABLE("Invalid ABIParamTypeInfo");
                            }

                            ARIA_ASSERT(dil, "Must set the debug local variable");
                            m_active_debug_context.builder->insertDeclare(m_active_module_context.named_values.at(param),
                                dil, m_active_debug_context.builder->createExpression(), 
                                llvm::DILocation::get(*m_active_module_context.context, decl->loc.line, decl->loc.col, sp), m_active_module_context.builder->GetInsertBlock());
                        }

                        gen_stmt(m.body);
                        m_active_module_context.alloca_marker->eraseFromParent();
                        m_active_module_context.alloca_marker = nullptr;
                        if (llvm::verifyFunction(*function, &llvm::errs())) { throw std::exception(); }
                    }

                    m_active_module_context.functions[field];
                    break;
                }

                default: ARIA_UNREACHABLE("Invalid field kind");
            }
        }
    }

    void Codegen::gen_decl(Decl* decl) {
        switch (decl->kind) {
            case DeclKind::Var: return gen_var_decl(decl);
            case DeclKind::Function: return gen_function_decl(decl);

            default: ARIA_UNREACHABLE("Invalid decl kind");
        }
    }

} // namespace ariac