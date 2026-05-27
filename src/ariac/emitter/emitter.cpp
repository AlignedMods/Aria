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

        std::vector<std::string> object_files;

        for (Module* mod : m_context->modules) {
            delete m_active_module_context.context;
            m_active_module_context.context = new llvm::LLVMContext();

            delete m_active_module_context.module;
            m_active_module_context.module = new llvm::Module(llvm::StringRef(mod->name), *m_active_module_context.context);

            delete m_active_module_context.builder;
            m_active_module_context.builder = new llvm::IRBuilder<>(*m_active_module_context.context);

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

            std::string target_triple = llvm::sys::getDefaultTargetTriple();

            std::string error;
            const llvm::Target* target = llvm::TargetRegistry::lookupTarget(llvm::Triple(target_triple), error);

            if (!target) {
                fmt::print(stderr, "{}\n", error);
                continue;
            }

            const char* cpu = "generic";
            const char* features = "";

            llvm::TargetOptions opts;
            llvm::TargetMachine* machine = target->createTargetMachine(llvm::Triple(target_triple), cpu, features, opts, llvm::Reloc::PIC_);

            m_active_module_context.module->setDataLayout(machine->createDataLayout());
            m_active_module_context.module->setTargetTriple(llvm::Triple(target_triple));

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

    llvm::Value* Emitter::emit_boolean_literal_expr(Expr* expr) {
        BooleanLiteralExpr& bl = expr->boolean_literal;
        if (bl.value) { return llvm::ConstantInt::getTrue(*m_active_module_context.context); }
        else { return llvm::ConstantInt::getFalse(*m_active_module_context.context); }

        ARIA_UNREACHABLE();
    }

    llvm::Value* Emitter::emit_character_literal_expr(Expr* expr) {
        CharacterLiteralExpr& cl = expr->character_literal;
        return llvm::ConstantInt::get(*m_active_module_context.context, llvm::APInt(1, cl.value, expr->type->is_signed()));
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
        return m_active_module_context.builder->CreateGlobalString(sl.value, "str");
    }

    llvm::Value* Emitter::emit_array_filler_expr(Expr* expr) {
        ARIA_TODO("array filler");
    }

    llvm::Value* Emitter::emit_null_expr(Expr* expr) {
        return llvm::Constant::getNullValue(type_info_to_llvm_type(expr->type));
    }

    llvm::Value* Emitter::emit_decl_ref_expr(Expr* expr) {
        DeclRefExpr& dr = expr->decl_ref;

        if (m_functions.contains(dr.referenced_decl)) { return m_functions.at(dr.referenced_decl); }

        ARIA_ASSERT(m_named_values.contains(dr.referenced_decl), "Invalid DeclRef expression");
        return m_named_values.at(dr.referenced_decl);
    }

    llvm::Value* Emitter::emit_builtin_member_expr(Expr* expr) {
        MemberExpr& mem = expr->member;

        switch (mem.parent->type->kind) {
            case TypeKind::Slice: {
                llvm::Value* val = emit_expr(mem.parent);

                if (mem.member == "mem") {
                    return m_active_module_context.builder->CreateGEP(type_info_to_llvm_type(mem.parent->type), val, m_active_module_context.builder->getInt64(0), "ptradd", llvm::GEPNoWrapFlags::inBounds());
                } else if (mem.member == "len") {
                    return m_active_module_context.builder->CreateGEP(type_info_to_llvm_type(mem.parent->type), val, m_active_module_context.builder->getInt64(1), "ptradd", llvm::GEPNoWrapFlags::inBounds());
                } 

                ARIA_UNREACHABLE();
                break;
            }

            default: ARIA_UNREACHABLE();
        }
    }

    llvm::Value* Emitter::emit_call_expr(Expr* expr) {
        CallExpr& call = expr->call;
        
        std::vector<llvm::Value*> args;
        args.reserve(call.arguments.size);

        for (Expr* arg : call.arguments) {
            args.push_back(emit_expr(arg));
        }

        llvm::Value* val = emit_expr(call.callee);
        return m_active_module_context.builder->CreateCall(llvm::FunctionCallee(llvm::dyn_cast<llvm::FunctionType>(type_info_to_llvm_type(call.callee->type)), val), args, expr->type->is_void() ? "" : "call");
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
                llvm::Value* val = emit_expr(ic.expression);
                return m_active_module_context.builder->CreateLoad(type_info_to_llvm_type(expr->type), val);
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

            case BinaryOperatorKind::Eq: {
                llvm::Value* lhs = emit_expr(bin.lhs);
                llvm::Value* rhs = emit_expr(bin.rhs);

                return m_active_module_context.builder->CreateStore(rhs, lhs);
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
            case ExprKind::BuiltinMember: return emit_builtin_member_expr(expr);
            case ExprKind::Call: return emit_call_expr(expr);
            case ExprKind::ImplicitCast: return emit_implicit_cast_expr(expr);
            case ExprKind::BinaryOperator: return emit_binary_operator_expr(expr);

            default: ARIA_UNREACHABLE();
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
            llvm::Value* val = emit_expr(var.initializer);
            m_active_module_context.builder->CreateStore(val, m_named_values.at(decl));
        } else {
            m_active_module_context.builder->CreateStore(llvm::Constant::getNullValue(type), m_named_values.at(decl));
        }
    }

    void Emitter::emit_function_decl(Decl* decl) {
        FunctionDecl& fn = decl->function;
        std::string sig = mangle_function(decl);

        std::vector<llvm::Type*> params;
        for (Decl* p : fn.parameters) {
            params.push_back(type_info_to_llvm_type(p->param.type));
        }

        llvm::FunctionType* fn_ty = llvm::FunctionType::get(type_info_to_llvm_type(fn.type->function.return_type), params, false);
        llvm::Function* function = llvm::Function::Create(fn_ty, llvm::GlobalValue::LinkageTypes::ExternalLinkage, sig, m_active_module_context.module);
        m_active_module_context.function = function;
        m_functions[decl] = function;

        if (fn.body) {
            llvm::BasicBlock* bb = llvm::BasicBlock::Create(*m_active_module_context.context, "entry", function);
            m_active_module_context.builder->SetInsertPoint(bb);

            size_t idx = 0;
            for (auto& arg : function->args()) {
                arg.setName(fn.parameters.items[idx]->param.identifier);
                llvm::AllocaInst* a = alloca_at_entry(function, fmt::format("{}.addr", fn.parameters.items[idx]->param.identifier), fn.parameters.items[idx]->param.type);
                m_named_values[fn.parameters.items[idx++]] = a;

                m_active_module_context.builder->CreateStore(&arg, a);
            }

            emit_stmt(fn.body);
            llvm::verifyFunction(*function, &llvm::errs());
        }
    }
        
    void Emitter::emit_struct_decl(Decl* decl) {
        StructDecl& struc = decl->struct_;

        std::vector<llvm::Type*> fields;
        fields.reserve(struc.fields.size);
        std::string name = fmt::format("{}.{}", valid_module_name(decl->parent_module->name), struc.identifier);

        for (Decl* field : struc.fields) {
            fields.push_back(type_info_to_llvm_type(field->field.type));
        }

        llvm::StructType* type = llvm::StructType::create(*m_active_module_context.context, fields, name);
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
            llvm::Type* types[2] = { llvm::PointerType::get(*m_active_module_context.context, 0), llvm::Type::getInt64Ty(*m_active_module_context.context) };
            return llvm::StructType::get(*m_active_module_context.context, llvm::ArrayRef(types));
        } else if (t->kind == TypeKind::Ptr) {
            return llvm::PointerType::get(*m_active_module_context.context, 0);
        } else if (t->kind == TypeKind::Function) {
            std::vector<llvm::Type*> args;
            args.reserve(t->function.param_types.size);

            for (TypeInfo* type : t->function.param_types) {
                args.push_back(type_info_to_llvm_type(type));
            }

            return llvm::FunctionType::get(type_info_to_llvm_type(t->function.return_type), args, false);
        } else if (t->kind == TypeKind::Structure) {
            std::vector<llvm::Type*> types;
            types.reserve(t->struct_.source_decl->struct_.fields.size);
            for (Decl* field : t->struct_.source_decl->struct_.fields) { types.push_back(type_info_to_llvm_type(field->field.type)); }
            return llvm::StructType::get(*m_active_module_context.context, types);
        } else {
            ARIA_UNREACHABLE();
        }
    }

    std::string Emitter::mangle_function(Decl* fn) {
        if (fn->function.identifier == "main") { return "main"; }

        for (auto& attr : fn->function.attributes) {
            if (attr.kind == FunctionDecl::AttributeKind::Extern) {
                return fmt::format("{}", attr.arg);
            }
        }

        std::string sig = fmt::format("{}.{}_", valid_module_name(fn->parent_module->name), fn->function.identifier);

        for (size_t i = 0; i < fn->function.parameters.size; i++) {
            sig += type_info_to_string(fn->function.parameters.items[i]->param.type);

            if (i != fn->function.parameters.size - 1) {
                sig += "_";
            }
        }

        return sig;
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
        return TmpB.CreateAlloca(type_info_to_llvm_type(type), nullptr, name);
    }

} // namespace Aria::Internal
