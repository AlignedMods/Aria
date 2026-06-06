#include "ariac/codegen/codegen.hpp"

namespace ariac {

    Codegen::Codegen(CompilationContext* ctx) {
        m_context = ctx;
        gen_impl();
    }

    Codegen::~Codegen() {
    }

    void Codegen::gen_impl() {
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
            fmt::print(stderr, "{}\n", error);
            return;
        }

        std::vector<std::string> object_files;

        for (Module* mod : m_context->modules) {
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

            if (llvm::verifyModule(*m_active_module_context.module)) { continue; }

            const char* cpu = "generic";
            const char* features = "";

            llvm::TargetOptions opts;
            llvm::TargetMachine* machine = target->createTargetMachine(m_triple, cpu, features, opts, llvm::Reloc::PIC_);

            m_active_module_context.module->setDataLayout(machine->createDataLayout());
            m_active_module_context.module->setTargetTriple(m_triple);

            std::string output = fmt::format(".build\\{}.o", valid_module_name(mod->name));
            std::error_code ec;
            llvm::raw_fd_ostream stream(output, ec, llvm::sys::fs::OF_None);
            
            if (ec) {
                fmt::print(stderr, "Could not open file for output '{}': {}\n", output, ec.message());
                continue;
            }

            llvm::legacy::PassManager pass;
            llvm::CodeGenFileType file_type = llvm::CodeGenFileType::ObjectFile;

            if (machine->addPassesToEmitFile(pass, stream, nullptr, file_type)) {
                fmt::print(stderr, "llvm::TargetMachine couldn't emit a file of this type\n");
                continue;
            }

            pass.run(*m_active_module_context.module);
            stream.flush();

            fmt::println("Generated output file '{}'", output);
            object_files.push_back(output);

            if (m_context->flags.dump_ir) {
                m_active_module_context.module->print(llvm::outs(), nullptr);
            }
        }

        std::vector<llvm::StringRef> args;
        args.push_back("clang");
        for (auto& o : object_files) {
            args.push_back(o);
        }
        args.push_back("-o");
        args.push_back(".build\\main.exe");

        llvm::ErrorOr<llvm::StringRef> clang_path = llvm::sys::findProgramByName("clang");
        if (std::error_code ec = clang_path.getError()) {
            fmt::print(stderr, "Failed to find clang: '{}'", ec.message());
            return;
        }

        int code = llvm::sys::ExecuteAndWait(clang_path.get(), args);

        if (code == -1) {
            fmt::print(stderr, "Could not invoke clang to run linker\n");
            return;
        } else if (code == -2) {
            fmt::print(stderr, "Failed to run linker");
            return;
        }
    }

    void Codegen::gen_builtin_types() {
        llvm::StructType::create({
            llvm::PointerType::get(*m_active_module_context.context, 0),
            llvm::Type::getInt64Ty(*m_active_module_context.context)
        }, "$builtin_slice");
    }

    llvm::Type* Codegen::type_info_to_llvm_type(TypeInfo* t) {
        if (t->is_void()) {
            return llvm::Type::getVoidTy(*m_active_module_context.context);
        } else if (t->is_integral()) {
            return llvm::Type::getIntNTy(*m_active_module_context.context, static_cast<unsigned>(t->get_bit_size()));
        } else if (t->kind == TypeKind::Float) {
            return llvm::Type::getFloatTy(*m_active_module_context.context);
        } else if (t->kind == TypeKind::Double) {
            return llvm::Type::getDoubleTy(*m_active_module_context.context);
        } else if (t->kind == TypeKind::Array) {
            return llvm::ArrayType::get(type_info_to_llvm_type(t->array.type), static_cast<size_t>(t->array.size));
        } else if (t->kind == TypeKind::Slice) {
            return llvm::StructType::getTypeByName(*m_active_module_context.context, "$builtin_slice");
        } else if (t->kind == TypeKind::Ptr) {
            return llvm::PointerType::get(*m_active_module_context.context, 0);
        } else if (t->kind == TypeKind::Ref) {
            return llvm::PointerType::get(*m_active_module_context.context, 0);
        } else if (t->kind == TypeKind::Function) {
            std::vector<llvm::Type*> args;
            args.reserve(t->function.param_types.size);

            for (TypeInfo* type : t->function.param_types) {
                ABITypeInfo info = get_abi_type_info(type);
                if (info.pass_direct) {
                    args.push_back(type_info_to_llvm_type(type));
                } else if (info.pass_by_ptr) {
                    args.push_back(type_info_to_llvm_type(&void_ptr_type));
                }
            }

            return llvm::FunctionType::get(type_info_to_llvm_type(t->function.return_type), args, t->function.var_arg);
        } else if (t->kind == TypeKind::Method) {
            std::vector<llvm::Type*> args;
            args.reserve(t->function.param_types.size + 1);
            args.push_back(type_info_to_llvm_type(&void_ptr_type)); // self

            for (TypeInfo* type : t->function.param_types) {
                ABITypeInfo info = get_abi_type_info(type);
                if (info.pass_direct) {
                    args.push_back(type_info_to_llvm_type(type));
                } else if (info.pass_by_ptr) {
                    args.push_back(type_info_to_llvm_type(&void_ptr_type));
                }
            }

            return llvm::FunctionType::get(type_info_to_llvm_type(t->function.return_type), args, t->function.var_arg);
        } else if (t->kind == TypeKind::Structure) {
            std::vector<llvm::Type*> types;
            types.reserve(t->struct_.source_decl->struct_.fields.size);
            for (Decl* field : t->struct_.source_decl->struct_.fields) { types.push_back(type_info_to_llvm_type(field->field.type)); }
            return llvm::StructType::get(*m_active_module_context.context, types);
        } else if (t->kind == TypeKind::Typedef) {
            return type_info_to_llvm_type(t->typedef_.base_type);
        } else {
            ARIA_UNREACHABLE();
        }
    }

    Codegen::ABITypeInfo Codegen::get_abi_type_info(TypeInfo* t) {
        ABITypeInfo info;
        info.type = t;
        
        switch (m_triple.getOS()) {
            case llvm::Triple::OSType::Win32: {
                switch (m_triple.getArch()) {
                    case llvm::Triple::ArchType::x86_64: {
                        if (t->is_slice()) {
                            info.pass_by_ptr = true;
                        } else if (t->is_structure()) {
                            size_t size = get_type_size(t);
                            if (size > 8) { info.pass_by_ptr = true; }
                            else { info.pass_by_integer = true; }
                        } else {
                            info.pass_direct = true;
                        }

                        break;
                    }

                    default: ARIA_UNREACHABLE();
                }

                break;
            }

            default: ARIA_UNREACHABLE();
        }

        return info;
    }

    uint64_t Codegen::get_type_size(TypeInfo* t) {
        switch (t->kind) {
            case TypeKind::Bool:
            case TypeKind::Char:
            case TypeKind::UChar: return 1;
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
            case TypeKind::UChar: return 1;
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

    std::string Codegen::mangle_function(Decl* fn) {
        if (fn->function.identifier == "main") { return "main"; }
        if (fn->function.linkage_kind == LinkageKind::Extern) { return fmt::format("{}", fn->function.identifier); }

        std::string mod_name = valid_module_name(fn->parent_module->name);
        std::string sig = fmt::format("A_{}{}{}{}", mod_name.length(), mod_name, fn->function.identifier.length(), fn->function.identifier);

        for (size_t i = 0; i < fn->function.parameters.size; i++) {
            sig += mangle_type(fn->function.parameters.items[i]->param.type);
        }

        return sig;
    }

    std::string Codegen::mangle_dtor(DestructorDecl* dtor) {
        std::string mod_name = valid_module_name(dtor->parent->parent_module->name);
        std::string sig = fmt::format("A_{}{}{}{}D", mod_name.length(), mod_name, dtor->parent->struct_.identifier.length(), dtor->parent->struct_.identifier);

        return sig;
    }

    std::string Codegen::mangle_method(MethodDecl* md) {
        std::string mod_name = valid_module_name(md->parent->parent_module->name);
        std::string sig = fmt::format("A_{}{}{}{}{}{}", mod_name.length(), mod_name, md->parent->struct_.identifier.length(), md->parent->struct_.identifier, md->identifier.length(), md->identifier);

        for (size_t i = 0; i < md->parameters.size; i++) {
            sig += mangle_type(md->parameters.items[i]->param.type);
        }

        return sig;
    }

    std::string Codegen::mangle_type(TypeInfo* t) {
        switch (t->kind) {
            case TypeKind::Bool: return "b";
            case TypeKind::Char: return "c";
            case TypeKind::UChar: return "uc";
            case TypeKind::Short: return "s";
            case TypeKind::UShort: return "us";
            case TypeKind::Int: return "i";
            case TypeKind::UInt: return "ui";
            case TypeKind::Long: return "l";
            case TypeKind::ULong: return "ul";
            case TypeKind::Float: return "f";
            case TypeKind::Double: return "d";

            case TypeKind::Ptr: {
                return fmt::format("P{}", mangle_type(t->base));
            }

            case TypeKind::Array: {
                return fmt::format("A{}{}", t->array.size, mangle_type(t->array.type));
            }

            case TypeKind::Slice: {
                return fmt::format("S{}", mangle_type(t->base));
            }

            case TypeKind::Ref: {
                return fmt::format("R{}", mangle_type(t->base));
            }

            case TypeKind::Structure: {
                return fmt::format("{}{}{}{}", t->struct_.source_decl->parent_module->name.length(), valid_module_name(t->struct_.source_decl->parent_module->name), t->struct_.identifier.length(), t->struct_.identifier);
            }

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
        ARIA_ASSERT(m_active_module_context.alloca_marker, "alloca_marker needs to be set");

        llvm::IRBuilder<> TmpB(m_active_module_context.alloca_marker);
        llvm::Type* t = type_info_to_llvm_type(type);
        return TmpB.CreateAlloca(t, nullptr, name);
    }

} // namespace ariac
