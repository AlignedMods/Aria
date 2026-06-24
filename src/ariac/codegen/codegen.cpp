#include "ariac/codegen/codegen.hpp"

#include <filesystem>

#ifdef PLATFORM_WINDOWS
    #include <io.h>
#endif

namespace ariac {

    Codegen::Codegen(CompilationContext* ctx) {
        m_context = ctx;
        gen_impl();
    }

    Codegen::~Codegen() {
    }

    void Codegen::gen_impl() {
        setup_env();

        try {
            for (Module* mod : m_context->modules) {
                gen_mod_to_ir(mod);
                gen_mod_to_obj(mod);

                if (m_context->flags.dump_ir) {
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

        std::string target_triple = llvm::sys::getDefaultTargetTriple();
        m_triple = llvm::Triple(target_triple);

        ARIA_ASSERT(m_triple.isOSWindows() && m_triple.isX86_64(), "Unsupported platform");
        m_triple.setEnvironment(llvm::Triple::GNU);

        std::string error;
        const llvm::Target* target = llvm::TargetRegistry::lookupTarget(m_triple, error);

        if (!target) {
            throw std::runtime_error(fmt::format("{}", error));
        }

        m_target = target;

        llvm::TargetOptions opts;
        m_machine = m_target->createTargetMachine(m_triple, "generic", "", opts, llvm::Reloc::PIC_);
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

        if (m_context->main_func->parent_module == mod) {
            llvm::Type* int32 = llvm::Type::getInt32Ty(*m_active_module_context.context);
            llvm::Type* ptr = llvm::PointerType::get(*m_active_module_context.context, 0);
            llvm::Function* main = llvm::Function::Create(llvm::FunctionType::get(int32, { int32, ptr}, false), llvm::GlobalValue::LinkageTypes::ExternalLinkage, "main", *m_active_module_context.module);
            m_active_module_context.function = main;

            llvm::BasicBlock* bb = llvm::BasicBlock::Create(*m_active_module_context.context, "entry", main);
            m_active_module_context.builder->SetInsertPoint(bb);
            m_active_module_context.alloca_marker = m_active_module_context.builder->CreateUnreachable();

            llvm::Value* argc = alloca_at_entry(main, "argc", int32);
            llvm::Value* argv = alloca_at_entry(main, "argv", ptr);

            m_active_module_context.builder->CreateStore(main->getArg(0), argc);
            m_active_module_context.builder->CreateStore(main->getArg(1), argv);

            llvm::FunctionCallee callee = llvm::FunctionCallee(m_active_module_context.functions.at(m_context->main_func));

            llvm::Value* ret = m_active_module_context.builder->CreateCall(callee, {});

            if (m_context->main_func->function.type->function.return_type->is_void()) {
                m_active_module_context.builder->CreateRet(m_active_module_context.builder->getInt32(0));
            } else {
                ARIA_ASSERT(m_context->main_func->function.type->function.return_type->kind == TypeKind::Int, "Invalid return type");
                m_active_module_context.builder->CreateRet(ret);
            }

            m_active_module_context.alloca_marker->eraseFromParent();
            m_active_module_context.alloca_marker = nullptr;
            if (llvm::verifyFunction(*main, &llvm::errs())) { throw std::exception(); }
        }

        if (llvm::verifyModule(*m_active_module_context.module)) { throw std::runtime_error(fmt::format("Module '{}' failed verification", mod->name)); }

        m_active_module_context.module->setDataLayout(m_machine->createDataLayout());
        m_active_module_context.module->setTargetTriple(m_triple);
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
        if (m_triple.isOSWindows()) {
            link_windows();
        } else {
            ARIA_UNREACHABLE();
        }
    }

    void Codegen::link_windows() {
        std::vector<llvm::StringRef> args;

        std::vector<std::string> libs;
        std::vector<std::string> libdirs;

        for (auto& lib : m_context->flags.libs) {
            libs.push_back(fmt::format("-l{}", lib));
        }

        for (auto& libdir : m_context->flags.libdirs) {
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
        } else if (t->kind == TypeKind::Ptr) {
            return llvm::PointerType::get(*m_active_module_context.context, 0);
        } else if (t->kind == TypeKind::Ref) {
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

    uint64_t Codegen::get_type_size(TypeInfo* t) {
        switch (t->kind) {
            case TypeKind::Bool:
            case TypeKind::Char:
            case TypeKind::IChar: return 1;
            case TypeKind::Short:
            case TypeKind::UShort: return 2;
            case TypeKind::Int:
            case TypeKind::UInt: return 4;
            case TypeKind::Long:
            case TypeKind::ULong: return 8;

            case TypeKind::Float: return 4;
            case TypeKind::Double: return 8;

            case TypeKind::Ptr: {
                if (m_triple.isX86_64()) { return 8; }
                else if (m_triple.isX86_32()) { return 4; }
                else { ARIA_UNREACHABLE(); }

                return 0;
            }

            case TypeKind::Slice: {
                if (m_triple.isX86_64()) { return 16; }
                else if (m_triple.isX86_32()) { return 12; }
                else { ARIA_UNREACHABLE(); }

                return 0;
            }

            case TypeKind::Structure: {
                size_t size = 0;
                size_t alignment = get_type_alignment(t);

                for (Decl* field : t->struct_.source_decl->struct_.fields) {
                    size += align_value(get_type_size(field->field.type), alignment);
                }

                return size;
            }

            default: ARIA_UNREACHABLE();
        }
    }

    uint64_t Codegen::get_type_alignment(TypeInfo* t) {
        switch (t->kind) {
            case TypeKind::Bool:
            case TypeKind::Char:
            case TypeKind::IChar: return 1;
            case TypeKind::Short:
            case TypeKind::UShort: return 2;
            case TypeKind::Int:
            case TypeKind::UInt: return 4;
            case TypeKind::Long:
            case TypeKind::ULong: return 8;

            case TypeKind::Float: return 4;
            case TypeKind::Double: return 8;

            case TypeKind::Ptr: {
                if (m_triple.isX86_64()) { return 8; }
                else if (m_triple.isX86_32()) { return 4; }
                else { ARIA_UNREACHABLE(); }

                return 0;
            }

            case TypeKind::Slice: {
                if (m_triple.isX86_64()) { return 8; }
                else if (m_triple.isX86_32()) { return 4; }
                else { ARIA_UNREACHABLE(); }

                return 0;
            }

            case TypeKind::Structure: {
                size_t alignment = 0;

                for (Decl* field : t->struct_.source_decl->struct_.fields) {
                    size_t new_alignment = get_type_alignment(field->field.type);
                    alignment = (new_alignment > alignment) ? new_alignment : alignment;
                }

                return alignment;
            }

            default: ARIA_UNREACHABLE();
        }
    }

    u64 Codegen::align_value(u64 val, u64 alignment) {
        return ((val + alignment - 1) / alignment) * alignment;
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
