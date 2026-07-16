#include "ariac/codegen/codegen.hpp"

#include <filesystem>

#ifdef PLATFORM_WINDOWS
    #include <io.h>

    #define isatty _isatty
    #define fileno _fileno
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
            bool is_tty = isatty(fileno(stdout));

            if (is_tty) {
                fmt::print(fmt::fg(fmt::color::pale_violet_red), "Codegen failed: {}\n", e.what());
            } else {
                fmt::print(stderr, "Codegen failed: {}\n", e.what());
            }
        }
    }

    void Codegen::gen_builtin_types() {
        llvm::Type* ptr_type = llvm::PointerType::get(*m_active_module_context.context, 0);
        llvm::Type* sz_type = type_info_to_llvm_type(TypeInfo::get_basic(TypeKind::Sz));

        llvm::StructType* slice_type = llvm::StructType::create({
            ptr_type,
            sz_type
        }, "$builtin_slice");

        llvm::StructType::create({ slice_type, slice_type, sz_type, sz_type, slice_type }, "$builtin_typeinfo");
        llvm::StructType::create({ ptr_type, ptr_type }, "$builtin_any");
    }

    void Codegen::setup_env() {
        llvm::InitializeAllTargetInfos();
        llvm::InitializeAllTargets();
        llvm::InitializeAllTargetMCs();
        llvm::InitializeAllAsmParsers();
        llvm::InitializeAllAsmPrinters();

        std::string error;
        const llvm::Target* target = llvm::TargetRegistry::lookupTarget(context.opts->triple.str(), error);

        if (!target) {
            throw std::runtime_error(fmt::format("{}", error));
        }

        m_target = target;

        llvm::TargetOptions opts;
        m_machine = m_target->createTargetMachine(context.opts->triple.str(), "generic", "", opts, llvm::Reloc::PIC_);
    }

    void Codegen::gen_mod_to_ir(Module* mod) {
        ModuleContext ctx;
        ctx.context = new llvm::LLVMContext();
        ctx.module = new llvm::Module(llvm::StringRef(mod->name), *ctx.context);
        ctx.builder = new llvm::IRBuilder<>(*ctx.context);

        m_active_module_context = ctx;
        m_module_contexts[mod] = ctx;

        gen_builtin_types();

        // Create debug contexts
        for (CompilationUnit* unit : mod->units) {
            if (!m_active_module_context.debug_contexts.contains(unit->filename)) {
                DebugContext ctx;
                ctx.builder = new llvm::DIBuilder(*m_active_module_context.module);

                std::filesystem::path p = unit->filename;
                ctx.unit = ctx.builder->createCompileUnit(llvm::dwarf::DW_LANG_C,
                    ctx.builder->createFile(p.filename().string(), p.parent_path().string()), "ariac", false, "", 0);

                m_active_module_context.debug_contexts[unit->filename] = ctx;
            }
        }

        for (CompilationUnit* unit : mod->units) {
            context.active_comp_unit = unit;
            m_active_debug_context = m_active_module_context.debug_contexts.at(unit->filename);

            for (Decl* struct_ : unit->structs) {
                gen_struct_decl(struct_);
            }

            for (Decl* gen : unit->generics) {
                ARIA_ASSERT(gen->kind == DeclKind::Generic, "Invalid generic decl");

                for (Decl* spec : gen->generic.specilizations) {
                    if (spec->kind != DeclKind::StructSpecilization) { continue; }
                    gen_struct_decl(spec->struct_specilization.source);
                }
            }
        }

        for (CompilationUnit* unit : mod->units) {
            context.active_comp_unit = unit;
            m_active_debug_context = m_active_module_context.debug_contexts.at(unit->filename);

            for (Decl* global : unit->globals) {
                gen_var_decl(global);
            }
        }

        for (CompilationUnit* unit : mod->units) {
            context.active_comp_unit = unit;
            m_active_debug_context = m_active_module_context.debug_contexts.at(unit->filename);

            for (Decl* func : unit->funcs) {
                gen_function_decl(func);
            }

            for (Decl* gen : unit->generics) {
                ARIA_ASSERT(gen->kind == DeclKind::Generic, "Invalid generic decl");

                for (Decl* spec : gen->generic.specilizations) {
                    if (spec->kind != DeclKind::FunctionSpecilization) { continue; }
                    ARIA_ASSERT(spec->function_specilization.source, "Invalid generic function specilization");
                    gen_function_decl(spec);
                }
            }
        }

        if (context.main_func && context.main_func->parent_module == mod) {
            context.active_comp_unit = context.main_func->parent_unit;
            m_active_debug_context = m_active_module_context.debug_contexts.at(context.main_func->parent_unit->filename);

            llvm::Type* void_type = llvm::Type::getVoidTy(*m_active_module_context.context);
            llvm::Type* int32_type = llvm::Type::getInt32Ty(*m_active_module_context.context);
            llvm::Type* int64_type = llvm::Type::getInt64Ty(*m_active_module_context.context);
            llvm::Type* ptr_type = llvm::PointerType::get(*m_active_module_context.context, 0);
            llvm::Type* slice_type = llvm::StructType::getTypeByName(*m_active_module_context.context, "$builtin_slice");

            if (context.opts->triple.isWindowsMSVCEnvironment()) {
                // Fix for undefined symbol '_fltused' on MSVC when not using CRT
                llvm::GlobalVariable* global = new llvm::GlobalVariable(*m_active_module_context.module, int32_type, false, llvm::GlobalValue::ExternalLinkage, llvm::Constant::getNullValue(int32_type), "_fltused");
            }

            llvm::Function* main = llvm::Function::Create(llvm::FunctionType::get(int32_type, { int32_type, ptr_type}, false), llvm::GlobalValue::LinkageTypes::ExternalLinkage, "main", *m_active_module_context.module);
            m_active_module_context.function = main;
            main->setDSOLocal(true);

            llvm::DISubprogram* sp = m_active_debug_context.builder->createFunction(m_active_debug_context.unit->getFile(),
                "main", {}, m_active_debug_context.unit->getFile(), (unsigned)context.main_func->loc.line,
                m_active_debug_context.builder->createSubroutineType({}), (unsigned)context.main_func->loc.line, llvm::DINode::FlagPrototyped, llvm::DISubprogram::SPFlagDefinition);

            main->setSubprogram(sp);
            m_active_debug_context.scope = sp;
            set_debug_loc(context.main_func->loc);

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
        m_active_module_context.module->setTargetTriple(context.opts->triple.str());

        // Finalize all DIBuilders
        for (auto& [_, b] : m_active_module_context.debug_contexts) {
            b.builder->finalize();
        }
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
            ARIA_UNREACHABLE("Invalid OS");
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
        } else if (t->kind == TypeKind::TypeInfo) {
            return llvm::StructType::getTypeByName(*m_active_module_context.context, "$builtin_typeinfo");
        } else if (t->kind == TypeKind::Any) {
            return llvm::StructType::getTypeByName(*m_active_module_context.context, "$builtin_any");
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

                switch (info.kind) {
                    case ABIRetKind::Direct: {
                        ret_type = type_info_to_llvm_type(info.type);
                        break;
                    }

                    case ABIRetKind::Pointer: {
                        params.push_back(llvm::PointerType::get(*m_active_module_context.context, 0));
                        break;
                    }

                    case ABIRetKind::Integer: {
                        ret_type = llvm::Type::getIntNTy(*m_active_module_context.context, static_cast<unsigned>(info.int_bits));
                        break;
                    }

                    default: ARIA_UNREACHABLE("Invalid ABIRetTypeInfo");
                }
            }
            
            if (t->kind == TypeKind::Method) {
                params.push_back(llvm::PointerType::get(*m_active_module_context.context, 0)); // self
            }

            for (size_t i = 0; i < t->function.param_types.size; i++) {
                if (t->function.variadic == VariadicKind::Named && i == t->function.param_types.size - 1) {
                    break;
                }

                TypeInfo* type = t->function.param_types.items[i];
                ABIParamTypeInfo info = get_param_abi_type_info(type);

                switch (info.kind) {
                    case ABIParamKind::Direct: {
                        params.push_back(type_info_to_llvm_type(type));
                        break;
                    }

                    case ABIParamKind::Pointer: {
                        params.push_back(llvm::PointerType::get(*m_active_module_context.context, 0));
                        break;
                    }

                    case ABIParamKind::Integer: {
                        params.push_back(llvm::Type::getIntNTy(*m_active_module_context.context, static_cast<unsigned>(info.int_bits)));
                        break;
                    }

                    default: ARIA_UNREACHABLE("Invalid ABIParamTypeInfo");
                }
            }

            if (t->function.variadic == VariadicKind::Named) {
                ABIParamTypeInfo info = get_param_abi_type_info(TypeInfo::get_char_slice());

                switch (info.kind) {
                    case ABIParamKind::Pointer: {
                        params.push_back(llvm::PointerType::get(*m_active_module_context.context, 0));
                        break;
                    }

                    case ABIParamKind::Integer: {
                        params.push_back(llvm::Type::getIntNTy(*m_active_module_context.context, static_cast<unsigned>(info.int_bits)));
                        break;
                    }

                    default: ARIA_UNREACHABLE("Invalid ABIParamTypeInfo");
                }
            }

            return llvm::FunctionType::get(ret_type, params, t->function.variadic == VariadicKind::Unnamed);
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
            ARIA_UNREACHABLE("Invalid type");
        }
    }

    llvm::DIType* Codegen::type_info_to_debug_type(TypeInfo* t) {
        std::string str_type = type_info_to_string(t, false);

        if (m_active_debug_context.cached_types.contains(str_type)) {
            return m_active_debug_context.cached_types.at(str_type);
        }

        llvm::DIType* dit = nullptr;

        if (t->is_void()) {
            dit = m_active_debug_context.builder->createBasicType(str_type, 0, llvm::dwarf::DW_ATE_boolean);
        } else if (t->is_boolean()) {
            dit = m_active_debug_context.builder->createBasicType(str_type, 1, llvm::dwarf::DW_ATE_boolean);
        } else if (t->is_integral()) {
            unsigned encoding = 0;

            if (t->is_unsigned()) {
                if (t->kind == TypeKind::Char) { encoding = llvm::dwarf::DW_ATE_unsigned_char; }
                else { encoding = llvm::dwarf::DW_ATE_unsigned; }
            } else {
                if (t->kind == TypeKind::IChar) { encoding = llvm::dwarf::DW_ATE_signed_char; }
                else { encoding = llvm::dwarf::DW_ATE_signed; }
            }

            dit = m_active_debug_context.builder->createBasicType(str_type, t->get_bit_size(), encoding);
        } else if (t->is_floating_point()) {
            dit = m_active_debug_context.builder->createBasicType(str_type, t->get_bit_size(), llvm::dwarf::DW_ATE_float);
        } else if (t->is_typeinfo()) {
            std::array<llvm::Metadata*, 5> elems{};

            // u64 offset_bits = 0;
            // elems[0] = m_active_debug_context.active_builder->createMemberType(m_active_debug_context.scope, "name", 
            //     m_active_debug_context.scope->getFile(), 0, TypeInfo::get_string()->get_bit_size(), TypeInfo::get_string()->get_alignment() * 8, offset_bits, llvm::DINode::DIFlags::FlagExplicit,
            //     type_info_to_debug_type(TypeInfo::get_string()));
            // 
            // offset_bits += TypeInfo::get_string()->get_bit_size();
            // 
            // elems[1] = m_active_debug_context.active_builder->createMemberType(m_active_debug_context.scope, "kind", 
            //     m_active_debug_context.scope->getFile(), 0, TypeInfo::get_string()->get_bit_size(), TypeInfo::get_string()->get_alignment() * 8, offset_bits, llvm::DINode::DIFlags::FlagExplicit,
            //     type_info_to_debug_type(TypeInfo::get_string()));
            // 
            // offset_bits += TypeInfo::get_string()->get_bit_size();
            // 
            // elems[2] = m_active_debug_context.active_builder->createMemberType(m_active_debug_context.scope, "size", 
            //     m_active_debug_context.scope->getFile(), 0, TypeInfo::get_basic(TypeKind::Sz)->get_bit_size(), TypeInfo::get_basic(TypeKind::Sz)->get_alignment() * 8, offset_bits, llvm::DINode::DIFlags::FlagExplicit,
            //     type_info_to_debug_type(TypeInfo::get_basic(TypeKind::Sz)));
            // 
            // offset_bits += TypeInfo::get_basic(TypeKind::Sz)->get_bit_size();
            // 
            // elems[3] = m_active_debug_context.active_builder->createMemberType(m_active_debug_context.scope, "len", 
            //     m_active_debug_context.scope->getFile(), 0, TypeInfo::get_basic(TypeKind::Sz)->get_bit_size(), TypeInfo::get_basic(TypeKind::Sz)->get_alignment() * 8, offset_bits, llvm::DINode::DIFlags::FlagExplicit,
            //     type_info_to_debug_type(TypeInfo::get_basic(TypeKind::Sz)));
            // 
            // offset_bits += TypeInfo::get_basic(TypeKind::Sz)->get_bit_size();
            // 
            // TypeInfo* elem_type = TypeInfo::create_with_base(TypeKind::Slice, TypeInfo::get_typeinfo_ptr());
            // 
            // elems[4] = m_active_debug_context.active_builder->createMemberType(m_active_debug_context.scope, "len", 
            //     m_active_debug_context.scope->getFile(), 0, elem_type->get_bit_size(),elem_type->get_alignment() * 8, offset_bits, llvm::DINode::DIFlags::FlagExplicit,
            //     type_info_to_debug_type(elem_type));

            dit = m_active_debug_context.builder->createStructType(m_active_debug_context.scope,
                str_type, m_active_debug_context.scope->getFile(), 0, (unsigned)t->get_size() * 8, (unsigned)t->get_alignment() * 8,
                llvm::DINode::DIFlags::FlagExplicit, nullptr, m_active_debug_context.builder->getOrCreateArray(elems));
        } else if (t->is_any()) {
            std::array<llvm::Metadata*, 2> elems{};

            u64 offset_bits = 0;
            elems[0] = m_active_debug_context.builder->createMemberType(m_active_debug_context.scope, "type", 
                m_active_debug_context.scope->getFile(), 0, TypeInfo::get_typeinfo_ptr()->get_bit_size(), (unsigned)TypeInfo::get_typeinfo_ptr()->get_alignment() * 8, offset_bits, llvm::DINode::DIFlags::FlagExplicit,
                type_info_to_debug_type(TypeInfo::get_typeinfo_ptr()));

            offset_bits += TypeInfo::get_typeinfo_ptr()->get_bit_size();

            elems[1] = m_active_debug_context.builder->createMemberType(m_active_debug_context.scope, "value", 
                m_active_debug_context.scope->getFile(), 0, TypeInfo::get_void_ptr()->get_bit_size(), (unsigned)TypeInfo::get_void_ptr()->get_alignment() * 8, offset_bits, llvm::DINode::DIFlags::FlagExplicit,
                type_info_to_debug_type(TypeInfo::get_void_ptr()));

            dit = m_active_debug_context.builder->createStructType(m_active_debug_context.scope,
                str_type, m_active_debug_context.scope->getFile(), 0, (unsigned)t->get_size() * 8, (unsigned)t->get_alignment() * 8,
                llvm::DINode::DIFlags::FlagExplicit, nullptr, m_active_debug_context.builder->getOrCreateArray(elems));
        } else if (t->is_pointer()) {
            dit = m_active_debug_context.builder->createPointerType(type_info_to_debug_type(t->pointer.base), t->get_bit_size());
        } else if (t->is_array()) {
            dit = m_active_debug_context.builder->createArrayType(t->array.size, (unsigned)t->array.base->get_alignment() * 8, type_info_to_debug_type(t->array.base), {});
        } else if (t->is_slice()) {
            std::array<llvm::Metadata*, 2> elems{};

            u64 offset_bits = 0;
            elems[0] = m_active_debug_context.builder->createMemberType(m_active_debug_context.scope, "mem", 
                m_active_debug_context.scope->getFile(), 0, TypeInfo::get_void_ptr()->get_bit_size(), (unsigned)TypeInfo::get_void_ptr()->get_alignment() * 8, offset_bits, llvm::DINode::DIFlags::FlagExplicit,
                type_info_to_debug_type(TypeInfo::create_pointer(t->slice.base, false)));

            offset_bits += TypeInfo::get_void_ptr()->get_bit_size();

            elems[1] = m_active_debug_context.builder->createMemberType(m_active_debug_context.scope, "len", 
                m_active_debug_context.scope->getFile(), 0, TypeInfo::get_basic(TypeKind::Sz)->get_bit_size(), (unsigned)TypeInfo::get_basic(TypeKind::Sz)->get_alignment() * 8, offset_bits, llvm::DINode::DIFlags::FlagExplicit,
                type_info_to_debug_type(TypeInfo::get_basic(TypeKind::Sz)));

            dit = m_active_debug_context.builder->createStructType(m_active_debug_context.scope,
                str_type, m_active_debug_context.scope->getFile(), 0, (unsigned)t->get_size() * 8, (unsigned)t->get_alignment() * 8,
                llvm::DINode::DIFlags::FlagExplicit, nullptr, m_active_debug_context.builder->getOrCreateArray(elems));
        } else if (t->is_structure()) {
            std::vector<llvm::Metadata*> elems;

            u64 offset_bits = 0;
            for (Decl* d : t->struct_.source_decl->struct_.fields) {
                ARIA_ASSERT(d->kind == DeclKind::Field, "Invalid field");

                llvm::DIType* mem = m_active_debug_context.builder->createMemberType(m_active_debug_context.scope, d->field.identifier, 
                    m_active_debug_context.scope->getFile(), (unsigned)d->loc.line, (unsigned)d->field.type->get_bit_size(), (unsigned)d->field.type->get_alignment() * 8, offset_bits, llvm::DINode::DIFlags::FlagExplicit,
                    type_info_to_debug_type(d->field.type));

                elems.push_back(mem);
                offset_bits += align_value(d->field.type->get_bit_size(), t->get_alignment() * 8);
            }

            dit = m_active_debug_context.builder->createStructType(m_active_debug_context.scope,
                str_type, m_active_debug_context.scope->getFile(), (unsigned)t->struct_.source_decl->loc.line, (unsigned)t->get_size() * 8, (unsigned)t->get_alignment() * 8,
                llvm::DINode::DIFlags::FlagExplicit, nullptr, m_active_debug_context.builder->getOrCreateArray(elems));
        } else if (t->is_typedef()) {
            dit = m_active_debug_context.builder->createTypedef(type_info_to_debug_type(t->typedef_.base), t->typedef_.identifier, 
                m_active_debug_context.unit->getFile(), (unsigned)t->typedef_.source_decl->loc.line, m_active_debug_context.scope);
        } else {
            ARIA_UNREACHABLE("Invalid type");
        }

        m_active_debug_context.cached_types[str_type] = dit;
        return dit;
    }

    std::string Codegen::mangle_type(TypeInfo* t) {
        switch (t->kind) {
            case TypeKind::Void: return "v";
            case TypeKind::Bool: return "b";
            case TypeKind::Char: return "c";
            case TypeKind::IChar: return "ic";
            case TypeKind::Short: return "s";
            case TypeKind::UShort: return "us";
            case TypeKind::Int: return "i";
            case TypeKind::UInt: return "ui";
            case TypeKind::Long: return "l";
            case TypeKind::ULong: return "ul";

            case TypeKind::Sz: return "z";
            case TypeKind::Isz: return "iz";

            case TypeKind::Float: return "f";
            case TypeKind::Double: return "d";

            case TypeKind::TypeInfo: return "ti";
            case TypeKind::Any: return "a";

            case TypeKind::Pointer: return fmt::format("P{}", mangle_type(t->pointer.base));
            case TypeKind::Array: return fmt::format("A{}.{}", t->array.size, mangle_type(t->array.base));
            case TypeKind::Slice: return fmt::format("S{}", mangle_type(t->slice.base));

            case TypeKind::Typedef: return mangle_type(t->typedef_.base);

            default: ARIA_UNREACHABLE("Invalid type kind");
        }
    }

    llvm::GlobalValue::LinkageTypes Codegen::linkage_kind_to_llvm(LinkageKind kind) {
        switch (kind) {
            case LinkageKind::None: return llvm::GlobalValue::LinkageTypes::ExternalLinkage;
            case LinkageKind::Extern: return llvm::GlobalValue::LinkageTypes::ExternalLinkage;
            case LinkageKind::Static: return llvm::GlobalValue::LinkageTypes::InternalLinkage;

            default: ARIA_UNREACHABLE("Invalid linkage kind");
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
        if (type->isIntegerTy(1)) { type = llvm::Type::getInt8Ty(*m_active_module_context.context); }

        ARIA_ASSERT(m_active_module_context.alloca_marker, "alloca_marker needs to be set");
        llvm::IRBuilder<> TmpB(m_active_module_context.alloca_marker);
        return TmpB.CreateAlloca(type, nullptr, name);
    }

    llvm::Constant* Codegen::get_sz(u64 i) {
        return m_active_module_context.builder->getInt(llvm::APInt((unsigned)TypeInfo::get_basic(TypeKind::Sz)->get_bit_size(), i));
    }

    llvm::Constant* Codegen::get_i64(u64 i) {
        return m_active_module_context.builder->getInt64(i);
    }

    llvm::Constant* Codegen::get_null() {
        return llvm::Constant::getNullValue(llvm::PointerType::get(*m_active_module_context.context, 0));
    }

    llvm::Constant* Codegen::get_string(std::string_view s, std::string_view name) {
        llvm::GlobalVariable* str = m_active_module_context.builder->CreateGlobalString(s, name, 0, nullptr);
        llvm::Constant* vals[2] = { str, m_active_module_context.builder->getInt64(s.length()) };
        return llvm::ConstantStruct::get(llvm::StructType::getTypeByName(*m_active_module_context.context, "$builtin_slice"), llvm::ArrayRef(vals));
    }

    llvm::Constant* Codegen::get_typeinfo(TypeInfo* t) {
        llvm::StructType* typeinfo_type = llvm::StructType::getTypeByName(*m_active_module_context.context, "$builtin_typeinfo");
        llvm::StructType* slice_type = llvm::StructType::getTypeByName(*m_active_module_context.context, "$builtin_slice");
        TypeInfo* type = t->is_typedef() ? t->typedef_.base : t;
        std::string name = type_info_to_string(type);

        if (m_active_module_context.typeinfos.contains(name)) {
            return m_active_module_context.typeinfos.at(name);
        }

        std::string_view kind;
        u64 len = 0;
        std::vector<llvm::Constant*> types;

        if (type->is_pointer()) {
            kind = "pointer";
        } else if (type->is_slice()) {
            kind = "slice";
        } else if (type->is_array()) {
            kind = "array";
            len = type->array.size;
            types.push_back(get_typeinfo(type->array.base));
        }

        std::array<llvm::Constant*, 5> args{};
        args[0] = get_string(name); // name
        args[1] = get_string(kind); // kind
        args[2] = m_active_module_context.builder->getInt(llvm::APInt((unsigned)TypeInfo::get_basic(TypeKind::Sz)->get_bit_size(), t->get_size())); // size
        args[3] = m_active_module_context.builder->getInt(llvm::APInt((unsigned)TypeInfo::get_basic(TypeKind::Sz)->get_bit_size(), len)); // len
        
        if (types.size() == 1) {
            args[4] = llvm::ConstantStruct::get(slice_type, { types[0], 
                m_active_module_context.builder->getInt(llvm::APInt((unsigned)TypeInfo::get_basic(TypeKind::Sz)->get_bit_size(), 1)) }); // types
        } else {
            ARIA_ASSERT(types.size() == 0, "Invalid amount of types");
            args[4] = llvm::Constant::getNullValue(slice_type); // types
        }

        llvm::Constant* initializer = llvm::ConstantStruct::get(typeinfo_type, args);

        std::string ti_name = fmt::format("type_infoZ{}", mangle_type(t));
        llvm::GlobalVariable* ti = new llvm::GlobalVariable(*m_active_module_context.module, typeinfo_type, true, llvm::GlobalValue::LinkageTypes::InternalLinkage, initializer, ti_name);
        m_active_module_context.typeinfos[name] = ti;
        return ti;
    }

    llvm::Function* Codegen::get_assert_func() {
        if (!context.assert_func) { return nullptr; }

        if (!m_active_module_context.functions.contains(context.assert_func)) { gen_function_prototype(context.assert_func); }
        return m_active_module_context.functions.at(context.assert_func);
    }

    void Codegen::call_assert(llvm::Value* cond, u64 line, const std::string& fmt, const std::vector<llvm::Value*>& args, const std::vector<TypeInfo*>& types) {
        llvm::Function* fn = get_assert_func();
        if (!fn) { return; }

        std::vector<llvm::Value*> aargs;
        gen_call_param(&aargs, cond, TypeInfo::get_basic(TypeKind::Bool));
        gen_call_param(&aargs, get_string(context.active_comp_unit->filename, ".file"), TypeInfo::get_string());
        gen_call_param(&aargs, get_i64(line), TypeInfo::get_basic(TypeKind::ULong));
        gen_call_param(&aargs, get_string(fmt, ".assert_msg"), TypeInfo::get_string());
        gen_call_variadic(&aargs, args, types);

        m_active_module_context.builder->CreateCall(fn, aargs);
    }

    void Codegen::set_debug_loc(const SourceLoc& loc) {
        if (!loc.is_valid()) {
            m_active_module_context.builder->SetCurrentDebugLocation(llvm::DebugLoc());
            return;
        }

        m_active_module_context.builder->SetCurrentDebugLocation(llvm::DILocation::get(*m_active_module_context.context, (unsigned)loc.line, (unsigned)loc.col, m_active_debug_context.scope));
    }

} // namespace ariac
