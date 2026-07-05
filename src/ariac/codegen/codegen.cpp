#include "ariac/codegen/codegen.hpp"

#include <filesystem>

#ifdef PLATFORM_WINDOWS
    #include <io.h>
#endif

namespace ariac {

    Codegen::Codegen() {
        gen_impl();
    }

    void Codegen::gen_impl() {
        setup_env();

        try {
            for (Module* mod : context.modules) {
                gen_mod_to_ir(mod);
                gen_mod_to_obj(mod);

                if (context.opts->emit_llvm) {
                    gen_mod_ir_dump(mod);
                }
            }

            link();
        } catch (std::exception& e) {
            bool is_tty = _isatty(_fileno(stdout));

            if (is_tty) {
                fmt::print(fmt::fg(fmt::color::pale_violet_red), "Codegen failed: {}\n", e.what());
            } else {
                fmt::print(stderr, "Codegen failed: {}\n", e.what());
            }
        }
    }

    void Codegen::gen_builtin_types() {
        llvm::StructType::create({
            llvm::PointerType::get(*m_active_module_context.context, 0),
            llvm::Type::getInt64Ty(*m_active_module_context.context)
        }, "$builtin_slice");
    }

    void Codegen::setup_env() {
        llvm::InitializeAllTargetInfos();
        llvm::InitializeAllTargets();
        llvm::InitializeAllTargetMCs();
        llvm::InitializeAllAsmParsers();
        llvm::InitializeAllAsmPrinters();

        std::string error;
        const llvm::Target* target = llvm::TargetRegistry::lookupTarget(context.opts->triple, error);

        if (!target) {
            throw std::runtime_error(fmt::format("{}", error));
        }

        m_target = target;

        llvm::TargetOptions opts;
        m_machine = m_target->createTargetMachine(context.opts->triple, "generic", "", opts, llvm::Reloc::PIC_);
    }

    void Codegen::gen_mod_to_ir(Module* mod) {
        ModuleContext ctx;
        ctx.context = new llvm::LLVMContext();
        ctx.module = new llvm::Module(llvm::StringRef(mod->name), *ctx.context);
        ctx.builder = new llvm::IRBuilder<>(*ctx.context);

        m_active_module_context = ctx;
        m_module_contexts[mod] = ctx;

        gen_builtin_types();

        for (CompilationUnit* unit : mod->units) {
            for (Decl* struct_ : unit->structs) {
                gen_struct_decl(struct_);
            }

            for (Decl* gen : unit->generics) {
                ARIA_ASSERT(gen->kind == DeclKind::Generic, "Invalid generic decl");

                for (Decl* spec : gen->generic.specilizations) {
                    ARIA_ASSERT(spec->kind == DeclKind::StructSpecilization, "Invalid generic specilization");
                    gen_struct_decl(spec->struct_specilization.source);
                }
            }
        }

        for (CompilationUnit* unit : mod->units) {
            for (Decl* global : unit->globals) {
                gen_var_decl(global);
            }
        }

        for (CompilationUnit* unit : mod->units) {
            for (Decl* func : unit->funcs) {
                gen_function_decl(func);
            }
        }

        if (context.main_func && context.main_func->parent_module == mod) {
            llvm::Type* void_type = llvm::Type::getVoidTy(*m_active_module_context.context);
            llvm::Type* int32_type = llvm::Type::getInt32Ty(*m_active_module_context.context);
            llvm::Type* int64_type = llvm::Type::getInt64Ty(*m_active_module_context.context);
            llvm::Type* ptr_type = llvm::PointerType::get(*m_active_module_context.context, 0);
            llvm::Type* slice_type = llvm::StructType::getTypeByName(*m_active_module_context.context, "$builtin_slice");

            llvm::Function* main = llvm::Function::Create(llvm::FunctionType::get(int32_type, { int32_type, ptr_type}, false), llvm::GlobalValue::LinkageTypes::ExternalLinkage, "main", *m_active_module_context.module);
            m_active_module_context.function = main;

            llvm::BasicBlock* bb = llvm::BasicBlock::Create(*m_active_module_context.context, "entry", main);
            m_active_module_context.builder->SetInsertPoint(bb);
            m_active_module_context.alloca_marker = m_active_module_context.builder->CreateUnreachable();

            llvm::FunctionCallee callee = llvm::FunctionCallee(m_active_module_context.functions.at(context.main_func));

            llvm::Value* ret = nullptr;
            
            if (context.main_func->function.parameters.size == 1) {
                llvm::Value* list = alloca_at_entry(main, "list", ptr_type);
                llvm::Value* i = alloca_at_entry(main, "i", int32_type);

                llvm::Function* calloc_fn = m_active_module_context.module->getFunction("calloc");
        
                if (!calloc_fn) {
                    llvm::FunctionType* fn_type = llvm::FunctionType::get(ptr_type, { int64_type, int64_type }, false);
                    calloc_fn = llvm::Function::Create(fn_type, llvm::GlobalValue::ExternalLinkage, "calloc", *m_active_module_context.module);
                }

                llvm::Function* free_fn = m_active_module_context.module->getFunction("free");
        
                if (!free_fn) {
                    llvm::FunctionType* fn_type = llvm::FunctionType::get(void_type, ptr_type, false);
                    free_fn = llvm::Function::Create(fn_type, llvm::GlobalValue::ExternalLinkage, "free", *m_active_module_context.module);
                }

                llvm::Function* strlen_fn = m_active_module_context.module->getFunction("strlen");
        
                if (!strlen_fn) {
                    llvm::FunctionType* fn_type = llvm::FunctionType::get(int64_type, ptr_type, false);
                    strlen_fn = llvm::Function::Create(fn_type, llvm::GlobalValue::ExternalLinkage, "strlen", *m_active_module_context.module);
                }

                llvm::Value* elem_size = m_active_module_context.builder->getInt64(context.main_func->function.parameters.items[0]->param.type->get_size());
                llvm::Value* elem_count = m_active_module_context.builder->CreateSExt(main->getArg(0), int64_type, "sext");
                llvm::Value* calloc_result = m_active_module_context.builder->CreateCall(calloc_fn, { elem_size, elem_count });

                m_active_module_context.builder->CreateStore(calloc_result, list);
                m_active_module_context.builder->CreateStore(m_active_module_context.builder->getInt32(0), i);

                llvm::BasicBlock* for_cond = llvm::BasicBlock::Create(*m_active_module_context.context, "for_cond", main);
                llvm::BasicBlock* for_body = llvm::BasicBlock::Create(*m_active_module_context.context, "for_body", main);
                llvm::BasicBlock* for_end = llvm::BasicBlock::Create(*m_active_module_context.context, "for_end", main);

                m_active_module_context.builder->CreateBr(for_cond);
                m_active_module_context.builder->SetInsertPoint(for_cond);

                {
                    llvm::Value* i_val = m_active_module_context.builder->CreateLoad(int32_type, i);
                    llvm::Value* cmp = m_active_module_context.builder->CreateICmpSLT(i_val, main->getArg(0), "lt");
                    m_active_module_context.builder->CreateCondBr(cmp, for_body, for_end);
                }

                m_active_module_context.builder->SetInsertPoint(for_body);

                {
                    llvm::Value* to_slice = alloca_at_entry(main, "to_slice", slice_type);

                    llvm::Value* i_val = m_active_module_context.builder->CreateLoad(int32_type, i);
                    llvm::Value* sext = m_active_module_context.builder->CreateSExt(i_val, int64_type, "sext");
                    llvm::Value* gep = m_active_module_context.builder->CreateGEP(ptr_type, main->getArg(1), sext);
                    llvm::Value* gep_val = m_active_module_context.builder->CreateLoad(ptr_type, gep);

                    llvm::Value* list_val = m_active_module_context.builder->CreateLoad(ptr_type, list);
                    llvm::Value* list_gep = m_active_module_context.builder->CreateGEP(slice_type, list_val, sext);

                    llvm::Value* to_slice_mem = m_active_module_context.builder->CreateStructGEP(slice_type, to_slice, 0);
                    m_active_module_context.builder->CreateStore(gep_val, to_slice_mem);

                    llvm::Value* to_slice_len = m_active_module_context.builder->CreateStructGEP(slice_type, to_slice, 1);
                    llvm::Value* strlen = m_active_module_context.builder->CreateCall(strlen_fn, gep_val);
                    m_active_module_context.builder->CreateStore(strlen, to_slice_len);

                    llvm::Value* to_slice_val = m_active_module_context.builder->CreateLoad(slice_type, to_slice);
                    m_active_module_context.builder->CreateStore(to_slice_val, list_gep);

                    llvm::Value* inc = m_active_module_context.builder->CreateAdd(i_val, m_active_module_context.builder->getInt32(1), "inc");
                    m_active_module_context.builder->CreateStore(inc, i);
                    m_active_module_context.builder->CreateBr(for_cond);
                }

                m_active_module_context.builder->SetInsertPoint(for_end);

                {
                    llvm::Value* list_val = m_active_module_context.builder->CreateLoad(ptr_type, list);

                    llvm::Value* args = alloca_at_entry(main, "args", slice_type);
                    llvm::Value* args_mem = m_active_module_context.builder->CreateStructGEP(slice_type, args, 0);
                    m_active_module_context.builder->CreateStore(list_val, args_mem);
                    llvm::Value* args_len = m_active_module_context.builder->CreateStructGEP(slice_type, args, 1);
                    m_active_module_context.builder->CreateStore(m_active_module_context.builder->CreateSExt(main->getArg(0), int64_type, "sext"), args_len);

                    ret = m_active_module_context.builder->CreateCall(callee, args);
                    m_active_module_context.builder->CreateCall(free_fn, list_val);
                }
            } else {
                ARIA_ASSERT(context.main_func->function.parameters.size == 0, "Invalid parameter count for main function");
                ret = m_active_module_context.builder->CreateCall(callee, {});
            }

            if (context.main_func->function.type->function.return_type->is_void()) {
                m_active_module_context.builder->CreateRet(m_active_module_context.builder->getInt32(0));
            } else {
                ARIA_ASSERT(context.main_func->function.type->function.return_type->kind == TypeKind::Int, "Invalid return type");
                m_active_module_context.builder->CreateRet(ret);
            }

            m_active_module_context.alloca_marker->eraseFromParent();
            m_active_module_context.alloca_marker = nullptr;
            if (llvm::verifyFunction(*main, &llvm::errs())) { throw std::exception(); }
        }

        if (llvm::verifyModule(*m_active_module_context.module)) { throw std::runtime_error(fmt::format("Module '{}' failed verification", mod->name)); }

        m_active_module_context.module->setDataLayout(m_machine->createDataLayout());
        m_active_module_context.module->setTargetTriple(context.opts->triple);
    }

    void Codegen::gen_mod_to_obj(Module* mod) {
        std::string output = fmt::format(".build\\{}.o", valid_module_name(mod->name));
        std::error_code ec;
        llvm::raw_fd_ostream stream(output, ec, llvm::sys::fs::OF_None);
        
        if (ec) {
            throw std::runtime_error(fmt::format("Could not open file for output '{}': {}", output, ec.message()));
        }

        llvm::legacy::PassManager pass;
        llvm::CodeGenFileType file_type = llvm::CodeGenFileType::ObjectFile;

        if (m_machine->addPassesToEmitFile(pass, stream, nullptr, file_type)) {
            throw std::runtime_error("llvm::TargetMachine couldn't emit a file of this type");
        }

        pass.run(*m_active_module_context.module);
        stream.flush();

        fmt::println("Generated output file '{}'", output);
        m_object_files.push_back(output);
    }

    void Codegen::gen_mod_ir_dump(Module* mod) {
        std::string output = fmt::format(".build\\{}.ll", valid_module_name(mod->name));
        std::error_code ec;
        llvm::raw_fd_ostream stream(output, ec, llvm::sys::fs::OF_None);
        
        if (ec) {
            throw std::runtime_error(fmt::format("Could not open file for output '{}': {}\n", output, ec.message()));
        }

        m_active_module_context.module->print(stream, nullptr);
        fmt::println("Generated LLVM IR file '{}'", output);
    }

    void Codegen::link() {
        if (context.opts->triple.isOSWindows()) {
            link_windows();
        } else {
            ARIA_UNREACHABLE();
        }
    }

    void Codegen::link_windows() {
        std::vector<llvm::StringRef> args;

        std::vector<std::string> libs;
        std::vector<std::string> libdirs;

        for (auto& lib : context.opts->libs) {
            libs.push_back(fmt::format("-l{}", lib));
        }

        for (auto& libdir : context.opts->libdirs) {
            libdirs.push_back(fmt::format("-L{}", libdir));
        }

        args.push_back("clang");
        for (auto& o : m_object_files) {
            args.push_back(o);
        }

        for (auto& lib : libs) {
            args.push_back(lib);
        }

        for (auto& libdir : libdirs) {
            args.push_back(libdir);
        }

        args.push_back("-Wl,--subsystem,console");
        args.push_back("-o");
        args.push_back(".build\\main.exe");

        llvm::ErrorOr<std::string> clang_path = llvm::sys::findProgramByName("clang");
        if (std::error_code ec = clang_path.getError()) {
            throw std::runtime_error(fmt::format("Failed to find clang: '{}'", ec.message()));
        }

        std::string err;
        int code = llvm::sys::ExecuteAndWait(clang_path.get(), args, {}, {}, 0, 0, &err);

        if (code == -1) {
            throw std::runtime_error(fmt::format("Could not invoke linker: {}", err));
        } else if (code == -2) {
            throw std::runtime_error(fmt::format("Failed to run linker: {}", err));
        } else if (code > 0) {
            throw std::runtime_error(fmt::format("Linker failed with exit code {}", code));
        } else {
            fmt::println("Generated executable '{}'", ".build\\main.exe");
        }
    }

    llvm::Type* Codegen::type_info_to_llvm_type(TypeInfo* t) {
        if (t->is_void()) {
            return llvm::Type::getVoidTy(*m_active_module_context.context);
        } else if (t->is_boolean()) {
            return llvm::Type::getInt1Ty(*m_active_module_context.context);
        } else if (t->is_integral()) {
            return llvm::Type::getIntNTy(*m_active_module_context.context, static_cast<unsigned>(t->get_bit_size()));
        } else if (t->kind == TypeKind::Float) {
            return llvm::Type::getFloatTy(*m_active_module_context.context);
        } else if (t->kind == TypeKind::Double) {
            return llvm::Type::getDoubleTy(*m_active_module_context.context);
        } else if (t->kind == TypeKind::Array) {
            return llvm::ArrayType::get(type_info_to_llvm_type(t->array.base), static_cast<size_t>(t->array.size));
        } else if (t->kind == TypeKind::Slice) {
            return llvm::StructType::getTypeByName(*m_active_module_context.context, "$builtin_slice");
        } else if (t->kind == TypeKind::Pointer) {
            return llvm::PointerType::get(*m_active_module_context.context, 0);
        } else if (t->kind == TypeKind::Function || t->kind == TypeKind::Method) {
            std::vector<llvm::Type*> params;

            // Return type
            llvm::Type* ret_type = llvm::Type::getVoidTy(*m_active_module_context.context);
            {
                ABIRetTypeInfo info = get_ret_abi_type_info(t->function.return_type);

                if (info.ret_direct) {
                    ret_type = type_info_to_llvm_type(info.type);
                } else if (info.ret_by_ptr) {
                    // Return via first parameter
                    params.push_back(llvm::PointerType::get(*m_active_module_context.context, 0));
                } else if (info.ret_by_integer) {
                    ret_type = llvm::Type::getIntNTy(*m_active_module_context.context, static_cast<unsigned>(info.int_bits));
                } else {
                    ARIA_UNREACHABLE();
                }
            }
            
            if (t->kind == TypeKind::Method) {
                params.push_back(llvm::PointerType::get(*m_active_module_context.context, 0)); // self
            }

            for (TypeInfo* type : t->function.param_types) {
                ABIParamTypeInfo info = get_param_abi_type_info(type);

                if (info.pass_direct) {
                    params.push_back(type_info_to_llvm_type(type));
                } else if (info.pass_by_ptr) {
                    params.push_back(llvm::PointerType::get(*m_active_module_context.context, 0));
                } else if (info.pass_by_integer) {
                    params.push_back(llvm::Type::getIntNTy(*m_active_module_context.context, static_cast<unsigned>(info.int_bits)));
                } else {
                    ARIA_UNREACHABLE();
                }
            }

            return llvm::FunctionType::get(ret_type, params, t->function.var_arg);
        } else if (t->kind == TypeKind::Structure) {
            std::string name = fmt::format("{}.{}", valid_module_name(t->struct_.source_decl->parent_module->name), t->struct_.identifier);
            llvm::Type* s = llvm::StructType::getTypeByName(*m_active_module_context.context, name);

            if (!s) {
                std::vector<llvm::Type*> types;
                types.reserve(t->struct_.source_decl->struct_.fields.size);
                for (Decl* field : t->struct_.source_decl->struct_.fields) { types.push_back(type_info_to_llvm_type(field->field.type)); }
                s = llvm::StructType::create(*m_active_module_context.context, types, name);
            }

            return s;
        } else if (t->kind == TypeKind::Typedef) {
            return type_info_to_llvm_type(t->typedef_.base);
        } else if (t->kind == TypeKind::Enum) {
            return llvm::IntegerType::getInt32Ty(*m_active_module_context.context);
        } else if (t->kind == TypeKind::GenericInstantiation) {
            Decl* struc = t->generic_instantiation.resolved_decl->struct_specilization.source;
            std::string name = fmt::format("{}.{}<", valid_module_name(struc->parent_module->name), struc->struct_.identifier);

            for (size_t i = 0; i < t->generic_instantiation.arguments.size; i++) {
                if (i > 0) {
                    name += ", ";
                }
                name += type_info_to_string(t->generic_instantiation.arguments.items[i]);
            }
            name += '>';

            llvm::Type* s = llvm::StructType::getTypeByName(*m_active_module_context.context, name);

            if (!s) {
                std::vector<llvm::Type*> types;
                types.reserve(struc->struct_.fields.size);
                for (Decl* field : struc->struct_.fields) { types.push_back(type_info_to_llvm_type(field->field.type)); }
                s = llvm::StructType::create(*m_active_module_context.context, types, name);
            }

            return s;
        } else {
            ARIA_UNREACHABLE();
        }
    }

    llvm::GlobalValue::LinkageTypes Codegen::linkage_kind_to_llvm(LinkageKind kind) {
        switch (kind) {
            case LinkageKind::None: return llvm::GlobalValue::LinkageTypes::ExternalLinkage;
            case LinkageKind::Extern: return llvm::GlobalValue::LinkageTypes::ExternalLinkage;
            case LinkageKind::Static: return llvm::GlobalValue::LinkageTypes::InternalLinkage;

            default: ARIA_UNREACHABLE();
        }
    }

    std::string Codegen::valid_module_name(std::string_view name) {
        std::string result;

        for (size_t i = 0; i < name.length(); i++) {
            if (name[i] == ':') {
                result += ".";
                i++;
                continue;
            }

            result += name[i];
        }

        return result;
    }

    llvm::AllocaInst* Codegen::alloca_at_entry(llvm::Function* f, llvm::StringRef name, TypeInfo* type) {
        llvm::Type* t = type_info_to_llvm_type(type);
        return alloca_at_entry(f, name, t);
    }

    llvm::AllocaInst* Codegen::alloca_at_entry(llvm::Function* f, llvm::StringRef name, llvm::Type* type) {
        ARIA_ASSERT(m_active_module_context.alloca_marker, "alloca_marker needs to be set");
        llvm::IRBuilder<> TmpB(m_active_module_context.alloca_marker);
        return TmpB.CreateAlloca(type, nullptr, name);
    }

} // namespace ariac
