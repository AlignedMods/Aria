#include "ariac/codegen/codegen.hpp"

namespace ariac {

    void Codegen::gen_var_decl(Decl* decl) {
        VarDecl& var = decl->var;
        set_debug_loc(decl->loc);
        if (var.const_var) { return;}

        llvm::Type* type = type_info_to_llvm_type(var.type);

        llvm::Value* a = nullptr;
        if (var.global_var) {
            std::string ident = fmt::format("{}.{}", valid_module_name(decl->parent_module->name), var.identifier);
            llvm::GlobalVariable* global = new llvm::GlobalVariable(*m_active_module_context.module, type, false, linkage_kind_to_llvm(var.linkage_kind), llvm::Constant::getNullValue(type), ident);
            llvm::Value* initializer = nullptr;
            a = global;

            if (var.dtor || var.initializer) {
                gen_global_init_func(decl->loc, global, var.initializer, var.dtor);
            }
        } else {
            a = alloca_at_entry(m_active_module_context.function, var.identifier, var.type);

            llvm::DILocalVariable* dil = m_active_debug_context.builder->createAutoVariable(m_active_debug_context.scope, var.identifier, m_active_debug_context.scope->getFile(), 
                (unsigned)decl->loc.line, type_info_to_debug_type(var.type));

            m_active_debug_context.builder->insertDeclare(a,
                dil, m_active_debug_context.builder->createExpression(), 
                llvm::DILocation::get(*m_active_module_context.context, (unsigned)decl->loc.line, (unsigned)decl->loc.col, m_active_debug_context.scope), m_active_module_context.builder->GetInsertBlock());

            if (var.initializer) {
                gen_init_expr(var.initializer, a);
            } else {
                m_active_module_context.builder->CreateStore(llvm::Constant::getNullValue(type), a);
            }
        }

        ARIA_ASSERT(a, "Invalid var decl");
        m_active_module_context.named_values[decl] = a;
    }

    void Codegen::gen_function_decl(Decl* decl) {
        FunctionDecl* fn = nullptr;

        switch (decl->kind) {
            case DeclKind::Function: fn = &decl->function; break;
            case DeclKind::FunctionSpecilization: fn = &decl->function_specilization.source->function; break;

            case DeclKind::Generic: {
                for (Decl* gs : decl->generic.specilizations) {
                    gen_function_decl(gs);
                }

                return;
            }
            default: ARIA_UNREACHABLE("Invalid function decl");
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
                function->getName(), {}, m_active_debug_context.unit->getFile(),(unsigned) decl->loc.line,
                m_active_debug_context.builder->createSubroutineType({}), (unsigned)decl->loc.line, llvm::DINode::FlagPrototyped, llvm::DISubprogram::SPFlagDefinition);

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

            m_active_module_context.alloca_marker = m_active_module_context.builder->CreateAlloca(m_active_module_context.builder->getInt8Ty());

            for (Decl* param : fn->parameters) {
                TypeInfo* param_type = param->param.variadic ? TypeInfo::create_slice(param->param.type) : param->param.type;
                ABIParamTypeInfo info = get_param_abi_type_info(param_type);

                llvm::DILocalVariable* dil = nullptr;
                llvm::DIExpression* di_expr = nullptr;

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
                            (unsigned)decl->loc.line, type_info_to_debug_type(param_type));
                        di_expr = m_active_debug_context.builder->createExpression();
                        break;
                    }

                    case ABIParamKind::Pointer: {
                        llvm::AllocaInst* a = alloca_at_entry(function, param->param.identifier, llvm::PointerType::get(*m_active_module_context.context, 0));
                        m_active_module_context.named_values[param] = a;

                        m_active_module_context.builder->CreateStore(function->getArg(static_cast<unsigned>(idx++)), a);

                        dil = m_active_debug_context.builder->createParameterVariable(sp, param->param.identifier, idx + 1, sp->getFile(), 
                            (unsigned)decl->loc.line, type_info_to_debug_type(param_type));
                        di_expr = m_active_debug_context.builder->createExpression(llvm::dwarf::DW_OP_deref);
                        break;
                    }

                    case ABIParamKind::Integer: {
                        llvm::AllocaInst* a = alloca_at_entry(m_active_module_context.function, param->param.identifier, param_type);
                        m_active_module_context.named_values[param] = a;

                        m_active_module_context.builder->CreateStore(function->getArg(static_cast<unsigned>(idx++)), a);

                        dil = m_active_debug_context.builder->createParameterVariable(sp, param->param.identifier, idx + 1, sp->getFile(), 
                            (unsigned)decl->loc.line, type_info_to_debug_type(param_type));
                        di_expr = m_active_debug_context.builder->createExpression();
                        break;
                    }

                    default: ARIA_UNREACHABLE("Invalid ABIParamTypeInfo");
                }

                ARIA_ASSERT(dil, "Must set the debug local variable");
                ARIA_ASSERT(di_expr, "Must set the debug expression");
                m_active_debug_context.builder->insertDeclare(m_active_module_context.named_values.at(param),
                    dil, di_expr, 
                    llvm::DILocation::get(*m_active_module_context.context, (unsigned)decl->loc.line, (unsigned)decl->loc.col, sp), m_active_module_context.builder->GetInsertBlock());
            }

            gen_block_stmt(fn->body);

            if (!m_active_module_context.builder->GetInsertBlock()->getTerminator()) {
                m_active_module_context.builder->CreateRetVoid();
            }

            m_active_module_context.alloca_marker->eraseFromParent();
            m_active_module_context.alloca_marker = nullptr;
            if (llvm::verifyFunction(*function, &llvm::errs())) { throw std::exception(); }
        }
    }

    void Codegen::gen_function_prototype(Decl* decl) {
        std::string sig;
        llvm::Function* function = nullptr;

        if (decl->kind == DeclKind::Function) {
            FunctionDecl& fn = decl->function;
            if (fn.linkage_kind == LinkageKind::Extern) {
                sig = fn.identifier;
            } else {
                sig = fmt::format("_A{}{}{}", mangle_module(decl->parent_module), fn.identifier.length(), fn.identifier);
            }

            llvm::Type* fn_ty = type_info_to_llvm_type(fn.type);
            function = llvm::Function::Create(dyn_cast<llvm::FunctionType>(fn_ty), linkage_kind_to_llvm(fn.linkage_kind), 0, sig, m_active_module_context.module);
        } else if (decl->kind == DeclKind::FunctionSpecilization) {
            FunctionSpecilizationDecl& fn = decl->function_specilization;
            sig = fmt::format("_A{}{}{}G", mangle_module(decl->parent_module), fn.source->function.identifier.length(), fn.source->function.identifier);
            for (TypeInfo* t : fn.types) {
                sig += mangle_type(t);
            }

            llvm::Type* fn_ty = type_info_to_llvm_type(fn.source->function.type);
            function = llvm::Function::Create(dyn_cast<llvm::FunctionType>(fn_ty), linkage_kind_to_llvm(fn.source->function.linkage_kind), 0, sig, m_active_module_context.module);
        } else {
            ARIA_UNREACHABLE("Invalid function prototype");
        }

        m_active_module_context.functions[decl] = function;

        for (auto& attr : decl->attributes) {
            if (attr.kind == DeclAttributeKind::Init) {
                llvm::appendToGlobalCtors(*m_active_module_context.module, function, 65535);
            }
        }
    }

    void Codegen::gen_method_prototype(Decl* decl) {
        MethodDecl& m = decl->method;
        ARIA_ASSERT(m.parent->kind == DeclKind::Struct, "Invalid method parent");
        std::string_view parent_name = m.parent->struct_.identifier;
        std::string sig = fmt::format("{}.{}.{}", valid_module_name(m.parent->parent_module->name), parent_name, m.identifier);

        llvm::Type* fn_ty = type_info_to_llvm_type(m.type);
        llvm::Function* function = llvm::Function::Create(dyn_cast<llvm::FunctionType>(fn_ty), llvm::GlobalValue::LinkageTypes::ExternalLinkage, sig, m_active_module_context.module);
        m_active_module_context.functions[decl] = function;
    }

    void Codegen::gen_destructor_prototype(Decl* decl) {
        DestructorDecl& d = decl->destructor;
        ARIA_ASSERT(d.parent->kind == DeclKind::Struct, "Invalid method parent");
        std::string_view parent_name = d.parent->struct_.identifier;

        std::string sig = fmt::format(".{}.{}.dtor", valid_module_name(d.parent->parent_module->name), parent_name);

        llvm::FunctionType* fn_ty = llvm::FunctionType::get(llvm::Type::getVoidTy(*m_active_module_context.context), llvm::PointerType::get(*m_active_module_context.context, 0), false);
        llvm::Function* function = llvm::Function::Create(fn_ty, llvm::GlobalValue::LinkageTypes::ExternalLinkage, sig, m_active_module_context.module);
        m_active_module_context.functions[decl] = function;
    }

    void Codegen::gen_struct_decl(Decl* decl) {
        StructDecl* struc = nullptr;

        switch (decl->kind) {
            case DeclKind::Struct: struc = &decl->struct_; break;
            case DeclKind::StructSpecilization: struc = &decl->struct_specilization.source->struct_; break;

            case DeclKind::Generic: {
                for (Decl* ss : decl->generic.specilizations) {
                    gen_struct_decl(ss);
                }

                return;
            }

            default: ARIA_UNREACHABLE("Invalid struct decl");
        }

        std::vector<llvm::Type*> fields;
        fields.reserve(struc->fields.size);
        std::string name = fmt::format("{}.{}", valid_module_name(decl->parent_module->name), struc->identifier);

        for (Decl* field : struc->fields) {
            if (field->kind != DeclKind::Field) { continue; }
            fields.push_back(type_info_to_llvm_type(field->field.type));
        }
        llvm::StructType::create(fields, name);

        for (Decl* field : struc->fields) {
            switch (field->kind) {
                case DeclKind::Field: break;
                case DeclKind::Method: gen_method_decl(field); break;
                case DeclKind::Destructor: gen_destructor_decl(field); break;

                default: ARIA_UNREACHABLE("Invalid field kind");
            }
        }
    }

    void Codegen::gen_impl_decl(Decl* decl) {
        ImplDecl& impl = decl->impl;

        for (Decl* field : impl.fields) {
            switch (field->kind) {
                case DeclKind::Method: gen_method_decl(field); break;
                case DeclKind::Destructor: gen_destructor_decl(field); break;
                    
                default: ARIA_UNREACHABLE("Invalid field kind");
            }
        }
    }

    void Codegen::gen_method_decl(Decl* decl) {
        MethodDecl& m = decl->method;

        if (!m_active_module_context.functions.contains(decl)) {
            gen_method_prototype(decl);
        }
        
        llvm::Function* function = m_active_module_context.functions.at(decl);
        m_active_module_context.function = function;
        
        llvm::DISubprogram* sp = m_active_debug_context.builder->createFunction(m_active_debug_context.unit->getFile(),
            function->getName(), {}, m_active_debug_context.unit->getFile(), (unsigned)decl->loc.line,
            m_active_debug_context.builder->createSubroutineType({}), (unsigned)decl->loc.line, llvm::DINode::FlagPrototyped, llvm::DISubprogram::SPFlagDefinition);
        
        function->setSubprogram(sp);
        m_active_debug_context.scope = sp;
        
        // Don't set any source locations for the prologue
        set_debug_loc({});
        
        if (m.body) {
            m_ret_type_abi = get_ret_abi_type_info(m.type->function.return_type);
            unsigned idx = m_ret_type_abi.kind == ABIRetKind::Pointer ? 1 : 0;
        
            llvm::BasicBlock* bb = llvm::BasicBlock::Create(*m_active_module_context.context, "entry", function);
            m_active_module_context.builder->SetInsertPoint(bb);
        
            m_active_module_context.alloca_marker = m_active_module_context.builder->CreateAlloca(m_active_module_context.builder->getInt8Ty());
        
            // self
            llvm::AllocaInst* s = alloca_at_entry(function, "self", llvm::PointerType::get(*m_active_module_context.context, 0));
            m_self_value = s;
            m_active_module_context.builder->CreateStore(function->getArg(idx++), s);
        
            for (Decl* param : m.parameters) {
                TypeInfo* param_type = param->param.variadic ? TypeInfo::create_slice(param->param.type) : param->param.type;
                ABIParamTypeInfo info = get_param_abi_type_info(param->param.type);
        
                llvm::DILocalVariable* dil = nullptr;
                llvm::DIExpression* di_expr = nullptr;
        
                switch (info.kind) {
                    case ABIParamKind::Direct: {
                        llvm::AllocaInst* a = alloca_at_entry(function, param->param.identifier, param_type);
                        m_active_module_context.named_values[param] = a;
        
                        unsigned ui = static_cast<unsigned>(idx++);
        
                        if (param_type->is_boolean()) {
                            function->addParamAttr(ui, llvm::Attribute::ZExt);
                        }
        
                        m_active_module_context.builder->CreateStore(function->getArg(ui), a);
        
                        dil = m_active_debug_context.builder->createParameterVariable(sp, param->param.identifier, idx + 1, sp->getFile(), 
                            (unsigned)decl->loc.line, type_info_to_debug_type(param_type));
                        di_expr = m_active_debug_context.builder->createExpression();
                        break;
                    }
        
                    case ABIParamKind::Pointer: {
                        llvm::AllocaInst* a = alloca_at_entry(function, param->param.identifier, llvm::PointerType::get(*m_active_module_context.context, 0));
                        m_active_module_context.named_values[param] = a;
        
                        m_active_module_context.builder->CreateStore(function->getArg(static_cast<unsigned>(idx++)), a);
        
                        dil = m_active_debug_context.builder->createParameterVariable(sp, param->param.identifier, idx + 1, sp->getFile(), 
                            (unsigned)decl->loc.line, type_info_to_debug_type(param_type));
                        di_expr = m_active_debug_context.builder->createExpression(llvm::dwarf::DW_OP_deref);
                        break;
                    }
        
                    case ABIParamKind::Integer: {
                        llvm::AllocaInst* a = alloca_at_entry(m_active_module_context.function, param->param.identifier, param_type);
                        m_active_module_context.named_values[param] = a;
        
                        m_active_module_context.builder->CreateStore(function->getArg(static_cast<unsigned>(idx++)), a);
        
                        dil = m_active_debug_context.builder->createParameterVariable(sp, param->param.identifier, idx + 1, sp->getFile(), 
                            (unsigned)decl->loc.line, type_info_to_debug_type(param_type));
                        di_expr = m_active_debug_context.builder->createExpression();
                        break;
                    }
        
                    default: ARIA_UNREACHABLE("Invalid ABIParamTypeInfo");
                }
        
                ARIA_ASSERT(dil, "Must set the debug local variable");
                ARIA_ASSERT(di_expr, "Must set the debug expression");
                m_active_debug_context.builder->insertDeclare(m_active_module_context.named_values.at(param),
                    dil, di_expr, 
                    llvm::DILocation::get(*m_active_module_context.context, (unsigned)decl->loc.line, (unsigned)decl->loc.col, sp), m_active_module_context.builder->GetInsertBlock());
            }
        
            gen_block_stmt(m.body);
        
            if (!m_active_module_context.builder->GetInsertBlock()->getTerminator()) {
                m_active_module_context.builder->CreateRetVoid();
            }
        
            m_active_module_context.alloca_marker->eraseFromParent();
            m_active_module_context.alloca_marker = nullptr;
            if (llvm::verifyFunction(*function, &llvm::errs())) { throw std::exception(); }
        }
        
        m_active_module_context.functions[decl];
    }

    void Codegen::gen_destructor_decl(Decl* decl) {
        DestructorDecl& d = decl->destructor;

        if (!m_active_module_context.functions.contains(decl)) {
            gen_destructor_prototype(decl);
        }
        
        llvm::Function* function = m_active_module_context.functions.at(decl);
        m_active_module_context.function = function;
        
        llvm::DISubprogram* sp = m_active_debug_context.builder->createFunction(m_active_debug_context.unit->getFile(),
            function->getName(), {}, m_active_debug_context.unit->getFile(), (unsigned)decl->loc.line,
            m_active_debug_context.builder->createSubroutineType({}), (unsigned)decl->loc.line, llvm::DINode::FlagPrototyped, llvm::DISubprogram::SPFlagDefinition);
        
        function->setSubprogram(sp);
        m_active_debug_context.scope = sp;
        
        // Don't set any source locations for the prologue
        set_debug_loc({});
        
        m_ret_type_abi = get_ret_abi_type_info(TypeInfo::get_void());

        llvm::BasicBlock* bb = llvm::BasicBlock::Create(*m_active_module_context.context, "entry", function);
        m_active_module_context.builder->SetInsertPoint(bb);
        
        m_active_module_context.alloca_marker = m_active_module_context.builder->CreateUnreachable();
        
        // self
        llvm::AllocaInst* s = alloca_at_entry(function, "self", llvm::PointerType::get(*m_active_module_context.context, 0));
        m_self_value = s;
        m_active_module_context.builder->CreateStore(function->getArg(0), s);

        gen_block_stmt(d.body);
        
        if (!m_active_module_context.builder->GetInsertBlock()->getTerminator()) {
            m_active_module_context.builder->CreateRetVoid();
        }
        
        m_active_module_context.alloca_marker->eraseFromParent();
        m_active_module_context.alloca_marker = nullptr;
        if (llvm::verifyFunction(*function, &llvm::errs())) { throw std::exception(); }
        
        m_active_module_context.functions[decl];
    }

    void Codegen::gen_global_init_func(SourceLoc loc, llvm::GlobalVariable* var, Expr* initializer, Decl* dtor) {
        llvm::Function* d = nullptr;
        llvm::Function* at_exit = nullptr;
        llvm::Function* dtor_call = nullptr;

        // Previous state
        llvm::BasicBlock* prevbb = m_active_module_context.builder->GetInsertBlock();
        llvm::Function* prevf = m_active_module_context.function;
        llvm::Instruction* preva = m_active_module_context.alloca_marker;

        if (dtor) {
            if (!m_active_module_context.functions.contains(dtor)) { gen_destructor_prototype(dtor); }
            d = m_active_module_context.functions.at(dtor);

            if (!m_active_module_context.module->getFunction("atexit")) {
                at_exit = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(*m_active_module_context.context), llvm::PointerType::get(*m_active_module_context.context, 0), false),
                    llvm::GlobalValue::LinkageTypes::ExternalLinkage, "atexit", m_active_module_context.module);
            } else {
                at_exit = m_active_module_context.module->getFunction("atexit");
            }

            // Create a wrapper function to call the dtor
            dtor_call = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(*m_active_module_context.context), false),
                llvm::GlobalValue::LinkageTypes::InternalLinkage, fmt::format(".__aria_global_call_dtor.{}", var->getName().str()), m_active_module_context.module);

            llvm::BasicBlock* entry = llvm::BasicBlock::Create(*m_active_module_context.context, "entry", dtor_call);
            m_active_module_context.builder->SetInsertPoint(entry);

            llvm::DISubprogram* sp = m_active_debug_context.builder->createFunction(m_active_debug_context.unit->getFile(),
                dtor_call->getName(), {}, m_active_debug_context.unit->getFile(), (unsigned)loc.line,
                m_active_debug_context.builder->createSubroutineType({}), (unsigned)loc.line, llvm::DINode::FlagPrototyped, llvm::DISubprogram::SPFlagDefinition);
            
            dtor_call->setSubprogram(sp);
            m_active_debug_context.scope = sp;

            set_debug_loc(loc);

            m_active_module_context.builder->CreateCall(d, var);
            m_active_module_context.builder->CreateRetVoid();
        }

        // Create the function that will set the initializer and destructor
        llvm::Function* init_var = llvm::Function::Create(llvm::FunctionType::get(llvm::Type::getVoidTy(*m_active_module_context.context), false),
            llvm::GlobalValue::LinkageTypes::InternalLinkage, fmt::format(".__aria_global_var_init.{}", var->getName().str()), m_active_module_context.module);

        llvm::BasicBlock* entry = llvm::BasicBlock::Create(*m_active_module_context.context, "entry", init_var);
        m_active_module_context.builder->SetInsertPoint(entry);
        m_active_module_context.function = init_var;
        m_active_module_context.alloca_marker = m_active_module_context.builder->CreateUnreachable();

        llvm::DISubprogram* sp = m_active_debug_context.builder->createFunction(m_active_debug_context.unit->getFile(),
            init_var->getName(), {}, m_active_debug_context.unit->getFile(), (unsigned)loc.line,
            m_active_debug_context.builder->createSubroutineType({}), (unsigned)loc.line, llvm::DINode::FlagPrototyped, llvm::DISubprogram::SPFlagDefinition);
        
        init_var->setSubprogram(sp);
        m_active_debug_context.scope = sp;

        set_debug_loc(loc);

        if (initializer) {
            llvm::Value* val = gen_expr(initializer);
            if (llvm::Constant* c = llvm::dyn_cast<llvm::Constant>(val)) {
                var->setInitializer(c);
            } else {
                m_active_module_context.builder->CreateStore(val, var);
            }
        }

        if (dtor) {
            m_active_module_context.builder->CreateCall(at_exit, dtor_call);
        }

        m_active_module_context.builder->CreateRetVoid();
        m_active_module_context.alloca_marker->removeFromParent();
        m_active_module_context.global_initializers.push_back(init_var);

        // Restore the old state
        m_active_module_context.builder->SetInsertPoint(prevbb);
        m_active_module_context.function = prevf;
        m_active_module_context.alloca_marker = preva;
    }

    void Codegen::gen_decl(Decl* decl) {
        switch (decl->kind) {
            case DeclKind::Var: return gen_var_decl(decl);
            case DeclKind::Function: return gen_function_decl(decl);

            default: ARIA_UNREACHABLE("Invalid decl kind");
        }
    }

} // namespace ariac