#include "ariac/emitter/emitter.hpp"

namespace Aria::Internal {

    Emitter::Emitter(CompilationContext* ctx) {
        m_context = ctx;
        emit_impl();
    }

    void Emitter::emit_impl() {
        for (Module* mod : m_context->modules) {
            delete m_active_module_context.context;
            m_active_module_context.context = new llvm::LLVMContext();

            delete m_active_module_context.module;
            m_active_module_context.module = new llvm::Module(llvm::StringRef(mod->name), *m_active_module_context.context);

            delete m_active_module_context.builder;
            m_active_module_context.builder = new llvm::IRBuilder<>(*m_active_module_context.context);

            for (CompilationUnit* unit : mod->units) {
                for (Decl* func : unit->funcs) {
                    emit_function_decl(func);
                }
            }

            if (m_context->flags.dump_ir) {
                m_active_module_context.module->print(llvm::outs(), nullptr);
            }
        }
    }

    llvm::Value* Emitter::emit_boolean_literal_expr(Expr* expr, ExprValueKind value_kind) {
        BooleanLiteralExpr& bl = expr->boolean_literal;
        if (bl.value) { return llvm::ConstantInt::getTrue(*m_active_module_context.context); }
        else { return llvm::ConstantInt::getFalse(*m_active_module_context.context); }

        ARIA_UNREACHABLE();
    }

    llvm::Value* Emitter::emit_character_literal_expr(Expr* expr, ExprValueKind value_kind) {
        CharacterLiteralExpr& cl = expr->character_literal;
        return llvm::ConstantInt::get(*m_active_module_context.context, llvm::APInt(1, cl.value, expr->type->is_signed()));
    }

    llvm::Value* Emitter::emit_integer_literal_expr(Expr* expr, ExprValueKind value_kind) {
        IntegerLiteralExpr& il = expr->integer_literal;
        return llvm::ConstantInt::get(*m_active_module_context.context, llvm::APInt(expr->type->get_bit_size(), il.value));
    }

    llvm::Value* Emitter::emit_floating_literal_expr(Expr* expr, ExprValueKind value_kind) {
        FloatingLiteralExpr& fl = expr->floating_literal;
        if (expr->type->kind == TypeKind::Float) {
            return llvm::ConstantFP::get(*m_active_module_context.context, llvm::APFloat(static_cast<float>(fl.value)));
        } else {
            return llvm::ConstantFP::get(*m_active_module_context.context, llvm::APFloat(fl.value));
        }
    }

    llvm::Value* Emitter::emit_string_literal_expr(Expr* expr, ExprValueKind value_kind) {
        ARIA_TODO("string literals");
    }

    llvm::Value* Emitter::emit_array_filler_expr(Expr* expr, ExprValueKind value_kind) {
        ARIA_TODO("array filler");
    }

    llvm::Value* Emitter::emit_null_expr(Expr* expr, ExprValueKind value_kind) {
        ARIA_TODO("null expr");
    }

    llvm::Value* Emitter::emit_decl_ref_expr(Expr* expr, ExprValueKind value_kind) {
        ARIA_TODO("decl ref");
    }

    llvm::Function* Emitter::emit_function_decl(Decl* decl) {
        FunctionDecl& fn = decl->function;
        std::string sig = mangle_function(decl);

        std::vector<llvm::Type*> params;
        for (Decl* p : fn.parameters) {
            params.push_back(type_info_to_llvm_type(p->param.type));
        }

        llvm::FunctionType* fn_ty = llvm::FunctionType::get(type_info_to_llvm_type(fn.type->function.return_type), params, false);
        llvm::Function* function = llvm::Function::Create(fn_ty, llvm::GlobalValue::LinkageTypes::ExternalLinkage, sig, m_active_module_context.module);

        size_t idx = 0;
        for (auto& arg : function->args()) { // assign names to the parameters
            arg.setName(fn.parameters.items[idx++]->param.identifier);
        }

        llvm::BasicBlock* bb = llvm::BasicBlock::Create(*m_active_module_context.context, "entry", function);
        m_active_module_context.builder->SetInsertPoint(bb);

        idx = 0;
        m_named_values.clear();
        for (auto& arg : function->args()) {
            m_named_values[fn.parameters.items[idx++]] = &arg;
        }

        m_active_module_context.builder->CreateRetVoid();
        llvm::verifyFunction(*function);

        return function;
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
        } else {
            ARIA_UNREACHABLE();
        }
    }

    std::string Emitter::mangle_function(Decl* fn) {
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

} // namespace Aria::Internal
