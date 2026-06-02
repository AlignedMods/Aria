#include "ariac/emitter/emitter.hpp"

namespace Aria::Internal {

    Emitter::Emitter(CompilationContext* ctx) {
        m_context = ctx;
        emit_impl();
    }

    void Emitter::emit_impl() {
        llvm::InitializeAllTargetInfos();
        llvm::InitializeAllTargets();
        llvm::InitializeAllTargetMCs();
        llvm::InitializeAllAsmParsers();
        llvm::InitializeAllAsmPrinters();

        std::string target_triple = llvm::sys::getDefaultTargetTriple();
        m_triple = llvm::Triple(target_triple);

        ARIA_ASSERT(m_triple.isOSWindows() && m_triple.isX86_64(), "Unsupported platform");

        std::string error;
        const llvm::Target* target = llvm::TargetRegistry::lookupTarget(m_triple, error);

        if (!target) {
            fmt::print(stderr, "{}\n", error);
            return;
        }

        std::vector<std::string> object_files;

        for (Module* mod : m_context->modules) {
            delete m_active_module_context.context;
            m_active_module_context.context = new llvm::LLVMContext();

            delete m_active_module_context.module;
            m_active_module_context.module = new llvm::Module(llvm::StringRef(mod->name), *m_active_module_context.context);

            delete m_active_module_context.builder;
            m_active_module_context.builder = new llvm::IRBuilder<>(*m_active_module_context.context);

            emit_builtin_types();

            for (CompilationUnit* unit : mod->units) {
                for (Decl* struct_ : unit->structs) {
                    emit_struct_decl(struct_);
                }
            }

            for (CompilationUnit* unit : mod->units) {
                for (Decl* global : unit->globals) {
                    emit_var_decl(global);
                }
            }

            for (CompilationUnit* unit : mod->units) {
                for (Decl* func : unit->funcs) {
                    emit_function_decl(func);
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

    void Emitter::emit_builtin_types() {
        llvm::StructType::create({
            llvm::PointerType::get(*m_active_module_context.context, 0),
            llvm::Type::getInt64Ty(*m_active_module_context.context)
        }, "$builtin_slice");
    }

    llvm::Value* Emitter::emit_boolean_literal_expr(Expr* expr) {
        BooleanLiteralExpr& bl = expr->boolean_literal;
        if (bl.value) { return llvm::ConstantInt::getTrue(*m_active_module_context.context); }
        else { return llvm::ConstantInt::getFalse(*m_active_module_context.context); }

        ARIA_UNREACHABLE();
    }

    llvm::Value* Emitter::emit_character_literal_expr(Expr* expr) {
        CharacterLiteralExpr& cl = expr->character_literal;
        return llvm::ConstantInt::get(*m_active_module_context.context, llvm::APInt(8, cl.value, expr->type->is_signed()));
    }

    llvm::Value* Emitter::emit_integer_literal_expr(Expr* expr) {
        IntegerLiteralExpr& il = expr->integer_literal;
        return llvm::ConstantInt::get(*m_active_module_context.context, llvm::APInt(expr->type->get_bit_size(), il.value));
    }

    llvm::Value* Emitter::emit_floating_literal_expr(Expr* expr) {
        FloatingLiteralExpr& fl = expr->floating_literal;
        if (expr->type->kind == TypeKind::Float) {
            return llvm::ConstantFP::get(*m_active_module_context.context, llvm::APFloat(static_cast<float>(fl.value)));
        } else {
            return llvm::ConstantFP::get(*m_active_module_context.context, llvm::APFloat(fl.value));
        }
    }

    llvm::Value* Emitter::emit_string_literal_expr(Expr* expr) {
        StringLiteralExpr& sl = expr->string_literal;
       
        if (expr->type->is_slice()) {
            llvm::Value* str = m_active_module_context.builder->CreateGlobalString(sl.value, "str", 0, nullptr, false);
            llvm::Constant* vals[2] = { llvm::dyn_cast<llvm::Constant>(str), m_active_module_context.builder->getInt64(sl.value.length()) };
            return llvm::ConstantStruct::get(llvm::dyn_cast<llvm::StructType>(type_info_to_llvm_type(expr->type)), llvm::ArrayRef(vals));
        } else {
            return m_active_module_context.builder->CreateGlobalString(sl.value, "cstr");
        }
    }

    llvm::Value* Emitter::emit_array_filler_expr(Expr* expr) {
        ARIA_TODO("array filler");
    }

    llvm::Value* Emitter::emit_null_expr(Expr* expr) {
        return llvm::Constant::getNullValue(type_info_to_llvm_type(expr->type));
    }

    llvm::Value* Emitter::emit_decl_ref_expr(Expr* expr) {
        DeclRefExpr& dr = expr->decl_ref;
            
        if (dr.referenced_decl->kind == DeclKind::Function) {
            if (!m_functions.contains(dr.referenced_decl)) {
                emit_function_prototype(dr.referenced_decl);
            }

            return m_functions.at(dr.referenced_decl);
        }

        ARIA_ASSERT(m_named_values.contains(dr.referenced_decl), "Invalid DeclRef expression");
        llvm::Value* val = m_named_values.at(dr.referenced_decl);

        if (dr.referenced_decl->kind == DeclKind::Param && get_abi_type_info(expr->type).pass_by_ptr) {
            return m_active_module_context.builder->CreateLoad(type_info_to_llvm_type(&void_ptr_type), val);
        }

        return val;
    }

    llvm::Value* Emitter::emit_member_expr(Expr* expr) {
        MemberExpr& mem = expr->member;

        switch (mem.referenced_member->kind) {
            case DeclKind::Field: {
                llvm::Value* val = emit_expr(mem.parent);
                TypeInfo* type = mem.parent->type;
                size_t idx = 0;
                TinyVector<Decl*> fields;

                switch (mem.parent->type->kind) {
                    case TypeKind::Ref:
                    case TypeKind::Ptr: {
                        fields = mem.parent->type->base->struct_.source_decl->struct_.fields;
                        type = mem.parent->type->base;
                        val = m_active_module_context.builder->CreateLoad(type_info_to_llvm_type(&void_ptr_type), val);
                        break;
                    }

                    case TypeKind::Structure: {
                        fields = mem.parent->type->struct_.source_decl->struct_.fields;
                        break;
                    }

                    default: ARIA_UNREACHABLE();
                }

                for (Decl* field : fields) {
                    if (field == mem.referenced_member) { break; }
                    idx++;
                }

                return m_active_module_context.builder->CreateStructGEP(type_info_to_llvm_type(type), val, static_cast<unsigned>(idx));
            }

            case DeclKind::Method: {
                return m_functions.at(mem.referenced_member);
            }

            default: ARIA_UNREACHABLE();
        }
    }

    llvm::Value* Emitter::emit_builtin_member_expr(Expr* expr) {
        MemberExpr& mem = expr->member;
        
        llvm::Value* val = emit_expr(mem.parent);
        TypeInfo* type = mem.parent->type;

        if (mem.implicit_deref) {
            type = mem.parent->type->base;
            val = m_active_module_context.builder->CreateLoad(type_info_to_llvm_type(&void_ptr_type), val);
        }

        switch (type->kind) {
            case TypeKind::Slice: {
                if (mem.member == "mem") {
                    return m_active_module_context.builder->CreateStructGEP(type_info_to_llvm_type(type), val, 0);
                } else if (mem.member == "len") {
                    return m_active_module_context.builder->CreateStructGEP(type_info_to_llvm_type(type), val, 1);
                } 

                ARIA_UNREACHABLE();
                break;
            }

            default: ARIA_UNREACHABLE();
        }
    }

    llvm::Value* Emitter::emit_self_expr(Expr* expr) {
        return m_self_value;
    }

    llvm::Value* Emitter::emit_call_expr(Expr* expr) {
        CallExpr& call = expr->call;
        
        std::vector<llvm::Value*> args;
        args.reserve(call.arguments.size);

        for (Expr* arg : call.arguments) {
            llvm::Value* val = emit_expr(arg);

            ABITypeInfo info = get_abi_type_info(arg->type);
            if (info.pass_direct) {
                args.push_back(val);
            } else if (info.pass_by_ptr) {
                llvm::Value* copy = alloca_at_entry(m_active_module_context.function, "", arg->type);
                m_active_module_context.builder->CreateStore(val, copy);
                args.push_back(copy);
            }
        }

        llvm::Value* callee = emit_expr(call.callee);
        return m_active_module_context.builder->CreateCall(llvm::FunctionCallee(llvm::dyn_cast<llvm::FunctionType>(type_info_to_llvm_type(call.callee->type)), callee), args, expr->type->is_void() ? "" : "call");
    }

    llvm::Value* Emitter::emit_method_call_expr(Expr* expr) {
        MethodCallExpr& mc = expr->method_call;

        std::vector<llvm::Value*> args;
        args.reserve(mc.arguments.size + 1);
        args.push_back(emit_expr(mc.callee->member.parent)); // self

        for (Expr* arg : mc.arguments) {
            llvm::Value* val = emit_expr(arg);

            ABITypeInfo info = get_abi_type_info(arg->type);
            if (info.pass_direct) {
                args.push_back(val);
            } else if (info.pass_by_ptr) {
                llvm::Value* copy = alloca_at_entry(m_active_module_context.function, "", arg->type);
                m_active_module_context.builder->CreateStore(val, copy);
                args.push_back(copy);
            }
        }

        llvm::Value* callee = emit_expr(mc.callee);
        return m_active_module_context.builder->CreateCall(llvm::FunctionCallee(llvm::dyn_cast<llvm::FunctionType>(type_info_to_llvm_type(mc.callee->type)), callee), args, expr->type->is_void() ? "" : "call");
    }

    llvm::Value* Emitter::emit_implicit_cast_expr(Expr* expr) {
        ImplicitCastExpr& ic = expr->implicit_cast;

        switch (ic.kind) {
            case CastKind::Integral: {
                llvm::Value* val = emit_expr(ic.expression);

                if (ic.expression->type->get_bit_size() > expr->type->get_bit_size()) {
                    return m_active_module_context.builder->CreateTrunc(val, type_info_to_llvm_type(expr->type), "trunc");
                } else {
                    if (ic.expression->type->is_signed()) {
                        return m_active_module_context.builder->CreateSExt(val, type_info_to_llvm_type(expr->type), "sext");
                    } else {
                        return m_active_module_context.builder->CreateZExt(val, type_info_to_llvm_type(expr->type), "zext");
                    }
                }

                ARIA_UNREACHABLE();
                break;
            }

            case CastKind::BitCast: {
                return emit_expr(ic.expression);
            }

            case CastKind::ArrayToSlice: {
                llvm::Value* slice = alloca_at_entry(m_active_module_context.function, "arrtoslice", expr->type);
                llvm::Value* arr = emit_expr(ic.expression);

                llvm::Value* mem = m_active_module_context.builder->CreateGEP(type_info_to_llvm_type(expr->type), slice, m_active_module_context.builder->getInt64(0), "ptradd", llvm::GEPNoWrapFlags::inBounds());
                llvm::Value* len = m_active_module_context.builder->CreateGEP(type_info_to_llvm_type(expr->type), slice, m_active_module_context.builder->getInt64(1), "ptradd", llvm::GEPNoWrapFlags::inBounds());

                m_active_module_context.builder->CreateStore(arr, mem);
                m_active_module_context.builder->CreateStore(m_active_module_context.builder->getInt64(ic.expression->type->array.size), len);

                return slice;
            }

            case CastKind::ArrayToPointer: {
                if (ic.expression->value_kind == ExprValueKind::LValue) {
                    llvm::Value* val = emit_expr(ic.expression);
                    llvm::Value* zero = m_active_module_context.builder->getInt64(0);
                    return m_active_module_context.builder->CreateGEP(type_info_to_llvm_type(ic.expression->type), val, { zero, zero }, "arraydecay", llvm::GEPNoWrapFlags::inBounds());
                } else {
                    return emit_expr(ic.expression);
                }
            }

            case CastKind::LValueToRValue: {
                if (ic.expression->kind == ExprKind::StringLiteral) {
                    return emit_expr(ic.expression);
                }
                
                llvm::Value* val = emit_expr(ic.expression);
                return m_active_module_context.builder->CreateLoad(type_info_to_llvm_type(expr->type), val);
            }

            default: ARIA_UNREACHABLE();
        }
    }

    llvm::Value* Emitter::emit_cast_expr(Expr* expr) {
        CastExpr& cast = expr->cast;
        return emit_expr(cast.expression);
    }

    llvm::Value* Emitter::emit_unary_operator_expr(Expr* expr) {
        UnaryOperatorExpr& un = expr->unary_operator;

        switch (un.op) {
            case UnaryOperatorKind::AddressOf: {
                return emit_expr(un.expression);
            }

            case UnaryOperatorKind::Dereference: {
                return emit_expr(un.expression);
            }

            default: ARIA_UNREACHABLE();
        }
    }

    llvm::Value* Emitter::emit_binary_operator_expr(Expr* expr) {
        BinaryOperatorExpr& bin = expr->binary_operator;

        switch (bin.op) {
            case BinaryOperatorKind::Add: {
                llvm::Value* lhs = emit_expr(bin.lhs);
                llvm::Value* rhs = emit_expr(bin.rhs);

                if (expr->type->is_integral()) {
                    return m_active_module_context.builder->CreateAdd(lhs, rhs, "add");
                } else if (expr->type->is_floating_point()) {
                    return m_active_module_context.builder->CreateFAdd(lhs, rhs, "fadd");
                }

                ARIA_UNREACHABLE();
                break;
            }

            case BinaryOperatorKind::Sub: {
                llvm::Value* lhs = emit_expr(bin.lhs);
                llvm::Value* rhs = emit_expr(bin.rhs);

                if (expr->type->is_integral()) {
                    return m_active_module_context.builder->CreateSub(lhs, rhs, "sub");
                } else if (expr->type->is_floating_point()) {
                    return m_active_module_context.builder->CreateFSub(lhs, rhs, "fsub");
                }

                ARIA_UNREACHABLE();
                break;
            }

            case BinaryOperatorKind::Less: {
                llvm::Value* lhs = emit_expr(bin.lhs);
                llvm::Value* rhs = emit_expr(bin.rhs);

                if (bin.lhs->type->is_integral()) {
                    if (bin.lhs->type->is_signed()) {
                        return m_active_module_context.builder->CreateICmpSLT(lhs, rhs, "lt");
                    } else {    
                        return m_active_module_context.builder->CreateICmpULT(lhs, rhs, "lt");
                    }
                }

                ARIA_UNREACHABLE();
                break;
            }

            case BinaryOperatorKind::Greater: {
                llvm::Value* lhs = emit_expr(bin.lhs);
                llvm::Value* rhs = emit_expr(bin.rhs);

                if (bin.lhs->type->is_integral()) {
                    if (bin.lhs->type->is_signed()) {
                        return m_active_module_context.builder->CreateICmpSGT(lhs, rhs, "gt");
                    } else {
                        return m_active_module_context.builder->CreateICmpUGT(lhs, rhs, "gt");
                    }
                }

                ARIA_UNREACHABLE();
                break;
            }

            case BinaryOperatorKind::Eq: {
                llvm::Value* lhs = emit_expr(bin.lhs);
                llvm::Value* rhs = emit_expr(bin.rhs);

                return m_active_module_context.builder->CreateStore(rhs, lhs);
            }

            default: ARIA_UNREACHABLE();
        }
    }

    llvm::Value* Emitter::emit_compound_assign_expr(Expr* expr) {
        CompoundAssignExpr& comp = expr->compound_assign;

        llvm::Value* lhs = emit_expr(comp.lhs);
        llvm::Value* lhs_val = m_active_module_context.builder->CreateLoad(type_info_to_llvm_type(comp.lhs->type), lhs);
        llvm::Value* rhs = emit_expr(comp.rhs);

        switch (comp.op) {
            case BinaryOperatorKind::CompoundAdd: {
                if (expr->type->is_integral()) {
                    llvm::Value* add = m_active_module_context.builder->CreateAdd(lhs_val, rhs, "add");
                    m_active_module_context.builder->CreateStore(add, lhs);
                    return lhs;
                } else if (expr->type->is_floating_point()) {
                    llvm::Value* add = m_active_module_context.builder->CreateFAdd(lhs_val, rhs, "fadd");
                    m_active_module_context.builder->CreateStore(add, lhs);
                    return lhs;
                }

                ARIA_UNREACHABLE();
                break;
            }

            default: ARIA_UNREACHABLE();
        }
    }

    llvm::Value* Emitter::emit_expr(Expr* expr) {
        switch (expr->kind) {
            case ExprKind::BooleanLiteral: return emit_boolean_literal_expr(expr);
            case ExprKind::CharacterLiteral: return emit_character_literal_expr(expr);
            case ExprKind::IntegerLiteral: return emit_integer_literal_expr(expr);
            case ExprKind::FloatingLiteral: return emit_floating_literal_expr(expr);
            case ExprKind::StringLiteral: return emit_string_literal_expr(expr);
            case ExprKind::ArrayFiller: return emit_array_filler_expr(expr);
            case ExprKind::Null: return emit_null_expr(expr);
            case ExprKind::DeclRef: return emit_decl_ref_expr(expr);
            case ExprKind::Member: return emit_member_expr(expr);
            case ExprKind::BuiltinMember: return emit_builtin_member_expr(expr);
            case ExprKind::Self: return emit_self_expr(expr);
            case ExprKind::Call: return emit_call_expr(expr);
            case ExprKind::MethodCall: return emit_method_call_expr(expr);
            case ExprKind::ImplicitCast: return emit_implicit_cast_expr(expr);
            case ExprKind::Cast: return emit_cast_expr(expr);
            case ExprKind::UnaryOperator: return emit_unary_operator_expr(expr);
            case ExprKind::BinaryOperator: return emit_binary_operator_expr(expr);
            case ExprKind::CompoundAssign: return emit_compound_assign_expr(expr);

            default: ARIA_UNREACHABLE();
        }
    }

    llvm::Value* Emitter::emit_init_expr(Expr* expr, llvm::Value* dst) {
        llvm::Value* val = emit_expr(expr);

        if (expr->type->is_array()) {
            // Call memcpy since we copy arrays
            llvm::Value* size = m_active_module_context.builder->getInt64(expr->type->array.size);
            return m_active_module_context.builder->CreateMemCpy(dst, llvm::MaybeAlign::MaybeAlign(), val, llvm::MaybeAlign::MaybeAlign(), size);
        } else {
            return m_active_module_context.builder->CreateStore(val, dst);
        }
    }

    void Emitter::emit_var_decl(Decl* decl) {
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
            emit_init_expr(var.initializer, a);
        } else {
            m_active_module_context.builder->CreateStore(llvm::Constant::getNullValue(type), m_named_values.at(decl));
        }
    }

    void Emitter::emit_function_decl(Decl* decl) {
        FunctionDecl& fn = decl->function;

        if (!m_functions.contains(decl)) {
            emit_function_prototype(decl);
        }

        llvm::Function* function = m_functions.at(decl);
        m_active_module_context.function = function;

        if (fn.body) {
            llvm::BasicBlock* bb = llvm::BasicBlock::Create(*m_active_module_context.context, "entry", function);
            m_active_module_context.builder->SetInsertPoint(bb);

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

            emit_stmt(fn.body);
            llvm::verifyFunction(*function, &llvm::errs());
        }
    }

    void Emitter::emit_function_prototype(Decl* decl) {
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

    void Emitter::emit_method_prototype(Decl* decl) {
        MethodDecl& m = decl->method;
        std::string sig = mangle_method(&decl->method);

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
        
    void Emitter::emit_struct_decl(Decl* decl) {
        StructDecl& struc = decl->struct_;

        std::vector<llvm::Type*> fields;
        fields.reserve(struc.fields.size);
        std::string name = fmt::format("{}.{}", valid_module_name(decl->parent_module->name), struc.identifier);

        for (Decl* field : struc.fields) {
            fields.push_back(type_info_to_llvm_type(field->field.type));
        }

        llvm::StructType* type = llvm::StructType::create(fields, name);

        for (Decl* impl : struc.impls) {
            emit_impl_decl(impl);
        }
    }

    void Emitter::emit_impl_decl(Decl* decl) {
        ImplDecl& impl = decl->impl;

        for (Decl* field : impl.fields) {
            ARIA_ASSERT(field->kind == DeclKind::Method, "Invalid field");
            MethodDecl& m = field->method;

            if (!m_functions.contains(field)) {
                emit_method_prototype(field);
            }

            llvm::Function* function = m_functions.at(field);
            m_active_module_context.function = function;

            if (m.body) {
                llvm::BasicBlock* bb = llvm::BasicBlock::Create(*m_active_module_context.context, "entry", function);
                m_active_module_context.builder->SetInsertPoint(bb);

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

                emit_stmt(m.body);
                llvm::verifyFunction(*function, &llvm::errs());
            }

            m_functions[field];
        }
    }

    void Emitter::emit_decl(Decl* decl) {
        switch (decl->kind) {
            case DeclKind::Var: return emit_var_decl(decl);
            case DeclKind::Function: return emit_function_decl(decl);

            default: ARIA_UNREACHABLE();
        }
    }

    void Emitter::emit_block_stmt(Stmt* stmt) {
        BlockStmt& block = stmt->block;

        for (Stmt* stmt : block.stmts) {
            emit_stmt(stmt);
        }
    }

    void Emitter::emit_while_stmt(Stmt* stmt) {
        WhileStmt& wh = stmt->while_;
        
        llvm::BasicBlock* loop_cond = llvm::BasicBlock::Create(*m_active_module_context.context, "loop_cond", m_active_module_context.function);
        llvm::BasicBlock* loop_body = (wh.body->block.stmts.size == 0) ? nullptr : llvm::BasicBlock::Create(*m_active_module_context.context, "loop_body", m_active_module_context.function);
        llvm::BasicBlock* loop_end = llvm::BasicBlock::Create(*m_active_module_context.context, "loop_end", m_active_module_context.function);

        m_active_module_context.builder->CreateBr(loop_cond);

        m_active_module_context.builder->SetInsertPoint(loop_cond);
        llvm::Value* cond = emit_expr(wh.condition);
        m_active_module_context.builder->CreateCondBr(cond, loop_body ? loop_body : loop_cond, loop_end);

        if (loop_body) {
            m_active_module_context.builder->SetInsertPoint(loop_body);
            emit_block_stmt(wh.body);
            m_active_module_context.builder->CreateBr(loop_cond);
        }

        m_active_module_context.builder->SetInsertPoint(loop_end);
    }

    void Emitter::emit_return_stmt(Stmt* stmt) {
        ReturnStmt& ret = stmt->return_;

        if (ret.value) {
            llvm::Value* val = emit_expr(ret.value);
            m_active_module_context.builder->CreateRet(val);
        } else {
            m_active_module_context.builder->CreateRetVoid();
        }
    }

    void Emitter::emit_expr_stmt(Stmt* stmt) {
        Expr* expr = stmt->expr;
        emit_expr(expr);
    }

    void Emitter::emit_decl_stmt(Stmt* stmt)  {
        Decl* decl = stmt->decl;
        emit_decl(decl);
    }

    void Emitter::emit_stmt(Stmt* stmt) {
        switch (stmt->kind) {
            case StmtKind::Block: return emit_block_stmt(stmt);
            case StmtKind::While: return emit_while_stmt(stmt);
            case StmtKind::Return: return emit_return_stmt(stmt);
            case StmtKind::Expr: return emit_expr_stmt(stmt);
            case StmtKind::Decl: return emit_decl_stmt(stmt);
            default: ARIA_UNREACHABLE();
        }
    }

    llvm::Type* Emitter::type_info_to_llvm_type(TypeInfo* t) {
        if (t->is_void()) {
            return llvm::Type::getVoidTy(*m_active_module_context.context);
        } else if (t->is_integral()) {
            return llvm::Type::getIntNTy(*m_active_module_context.context, t->get_bit_size());
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
        } else {
            ARIA_UNREACHABLE();
        }
    }

    Emitter::ABITypeInfo Emitter::get_abi_type_info(TypeInfo* t) {
        ABITypeInfo info;
        info.type = t;
        
        switch (m_triple.getOS()) {
            case llvm::Triple::OSType::Win32: {
                switch (m_triple.getArch()) {
                    case llvm::Triple::ArchType::x86_64: {
                        if (t->is_slice()) {
                            info.pass_by_ptr = true;
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

    uint64_t Emitter::get_type_size(TypeInfo* t) {
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
                else if (m_triple.isX32()) { return 4; }
                else { ARIA_UNREACHABLE(); }

                return 0;
            }

            case TypeKind::Slice: {
                if (m_triple.isX86_64()) { return 16; }
                else if (m_triple.isX86_32()) { return 12; }
                else { ARIA_UNREACHABLE(); }

                return 0;
            }

            default: ARIA_UNREACHABLE();
        }
    }

    std::string Emitter::mangle_function(Decl* fn) {
        if (fn->function.identifier == "main") { return "main"; }

        for (auto& attr : fn->function.attributes) {
            if (attr.kind == FunctionDecl::AttributeKind::Extern) {
                return fmt::format("{}", attr.arg);
            }
        }

        std::string mod_name = valid_module_name(fn->parent_module->name);
        std::string sig = fmt::format("A_{}{}.{}{}", mod_name.length(), mod_name, fn->function.identifier.length(), fn->function.identifier);

        for (size_t i = 0; i < fn->function.parameters.size; i++) {
            sig += mangle_type(fn->function.parameters.items[i]->param.type);
        }

        return sig;
    }

    std::string Emitter::mangle_method(MethodDecl* md) {
        std::string mod_name = valid_module_name(md->parent->parent_module->name);
        std::string sig = fmt::format("A_{}{}.{}{}.{}{}", mod_name.length(), mod_name, md->parent->struct_.identifier.length(), md->parent->struct_.identifier, md->identifier.length(), md->identifier);

        for (size_t i = 0; i < md->parameters.size; i++) {
            sig += mangle_type(md->parameters.items[i]->param.type);
        }

        return sig;
    }

    std::string Emitter::mangle_type(TypeInfo* t) {
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

            default: ARIA_UNREACHABLE();
        }
    }

    std::string Emitter::valid_module_name(std::string_view name) {
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

    llvm::AllocaInst* Emitter::alloca_at_entry(llvm::Function* f, llvm::StringRef name, TypeInfo* type) {
        llvm::IRBuilder<> TmpB(&f->getEntryBlock(), f->getEntryBlock().begin());
        llvm::Type* t = type_info_to_llvm_type(type);
        return TmpB.CreateAlloca(t, nullptr, name);
    }

} // namespace Aria::Internal
