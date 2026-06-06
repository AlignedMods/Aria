#include "ariac/codegen/codegen.hpp"

namespace ariac {

    llvm::Value* Codegen::gen_boolean_literal_expr(Expr* expr) {
        BooleanLiteralExpr& bl = expr->boolean_literal;
        if (bl.value) { return llvm::ConstantInt::getTrue(*m_active_module_context.context); }
        else { return llvm::ConstantInt::getFalse(*m_active_module_context.context); }

        ARIA_UNREACHABLE();
    }

    llvm::Value* Codegen::gen_character_literal_expr(Expr* expr) {
        CharacterLiteralExpr& cl = expr->character_literal;
        return llvm::ConstantInt::get(*m_active_module_context.context, llvm::APInt(8, cl.value, expr->type->is_signed()));
    }

    llvm::Value* Codegen::gen_integer_literal_expr(Expr* expr) {
        IntegerLiteralExpr& il = expr->integer_literal;
        return llvm::ConstantInt::get(*m_active_module_context.context, llvm::APInt(static_cast<unsigned>(expr->type->get_bit_size()), il.value));
    }

    llvm::Value* Codegen::gen_floating_literal_expr(Expr* expr) {
        FloatingLiteralExpr& fl = expr->floating_literal;
        if (expr->type->kind == TypeKind::Float) {
            return llvm::ConstantFP::get(*m_active_module_context.context, llvm::APFloat(static_cast<float>(fl.value)));
        } else {
            return llvm::ConstantFP::get(*m_active_module_context.context, llvm::APFloat(fl.value));
        }
    }

    llvm::Value* Codegen::gen_string_literal_expr(Expr* expr) {
        StringLiteralExpr& sl = expr->string_literal;
       
        if (expr->type->is_slice()) {
            llvm::Value* str = m_active_module_context.builder->CreateGlobalString(sl.value, "str", 0, nullptr, false);
            llvm::Constant* vals[2] = { llvm::dyn_cast<llvm::Constant>(str), m_active_module_context.builder->getInt64(sl.value.length()) };
            return llvm::ConstantStruct::get(llvm::dyn_cast<llvm::StructType>(type_info_to_llvm_type(expr->type)), llvm::ArrayRef(vals));
        } else {
            return m_active_module_context.builder->CreateGlobalString(sl.value, "cstr");
        }
    }

    llvm::Value* Codegen::gen_array_filler_expr(Expr* expr) {
        ARIA_TODO("array filler");
    }

    llvm::Value* Codegen::gen_null_expr(Expr* expr) {
        return llvm::Constant::getNullValue(type_info_to_llvm_type(expr->type));
    }

    llvm::Value* Codegen::gen_decl_ref_expr(Expr* expr) {
        DeclRefExpr& dr = expr->decl_ref;
            
        if (dr.referenced_decl->kind == DeclKind::Function) {
            if (!m_functions.contains(dr.referenced_decl)) {
                gen_function_prototype(dr.referenced_decl);
            }

            return m_functions.at(dr.referenced_decl);
        }

        ARIA_ASSERT(m_named_values.contains(dr.referenced_decl), "Invalid DeclRef expression");
        llvm::Value* val = m_named_values.at(dr.referenced_decl);

        if (dr.referenced_decl->kind == DeclKind::Param && get_abi_type_info(expr->type).pass_by_ptr || expr->type->is_reference()) {
            return m_active_module_context.builder->CreateLoad(type_info_to_llvm_type(&void_ptr_type), val);
        }

        return val;
    }

    llvm::Value* Codegen::gen_member_expr(Expr* expr) {
        MemberExpr& mem = expr->member;

        switch (mem.referenced_member->kind) {
            case DeclKind::Field: {
                llvm::Value* val = gen_expr(mem.parent);
                TypeInfo* type = mem.parent->type;
                size_t idx = 0;
                TinyVector<Decl*> fields;

                if (mem.implicit_deref) {
                    fields = mem.parent->type->base->struct_.source_decl->struct_.fields;
                    type = mem.parent->type->base;
                    val = m_active_module_context.builder->CreateLoad(type_info_to_llvm_type(&void_ptr_type), val);
                }

                switch (mem.parent->type->kind) {
                    case TypeKind::Ref: {
                        fields = mem.parent->type->base->struct_.source_decl->struct_.fields;
                        type = mem.parent->type->base;
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

                llvm::Value* gep = m_active_module_context.builder->CreateStructGEP(type_info_to_llvm_type(type), val, static_cast<unsigned>(idx));

                if (expr->type->is_reference()) {
                    return m_active_module_context.builder->CreateLoad(type_info_to_llvm_type(&void_ptr_type), gep);
                } else {
                    return gep;
                }
            }

            case DeclKind::Method: {
                if (!m_functions.contains(mem.referenced_member)) {
                    gen_method_prototype(mem.referenced_member);
                }

                return m_functions.at(mem.referenced_member);
            }

            default: ARIA_UNREACHABLE();
        }
    }

    llvm::Value* Codegen::gen_builtin_member_expr(Expr* expr) {
        MemberExpr& mem = expr->member;
        
        llvm::Value* val = gen_expr(mem.parent);
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

    llvm::Value* Codegen::gen_self_expr(Expr* expr) {
        return m_active_module_context.builder->CreateLoad(type_info_to_llvm_type(&void_ptr_type), m_self_value);
    }

    llvm::Value* Codegen::gen_call_expr(Expr* expr) {
        CallExpr& call = expr->call;
        
        std::vector<llvm::Value*> args;
        args.reserve(call.arguments.size);

        for (Expr* arg : call.arguments) {
            llvm::Value* val = gen_expr(arg);

            ABITypeInfo info = get_abi_type_info(arg->type);
            if (info.pass_direct) {
                args.push_back(val);
            } else if (info.pass_by_ptr) {
                llvm::Value* copy = alloca_at_entry(m_active_module_context.function, "", arg->type);
                m_active_module_context.builder->CreateStore(val, copy);
                args.push_back(copy);
            }
        }

        llvm::Value* callee = gen_expr(call.callee);
        return m_active_module_context.builder->CreateCall(llvm::FunctionCallee(llvm::dyn_cast<llvm::FunctionType>(type_info_to_llvm_type(call.callee->type)), callee), args, expr->type->is_void() ? "" : "call");
    }

    llvm::Value* Codegen::gen_method_call_expr(Expr* expr) {
        MethodCallExpr& mc = expr->method_call;

        std::vector<llvm::Value*> args;
        args.reserve(mc.arguments.size + 1);
        args.push_back(gen_expr(mc.callee->member.parent)); // self

        for (Expr* arg : mc.arguments) {
            llvm::Value* val = gen_expr(arg);

            ABITypeInfo info = get_abi_type_info(arg->type);
            if (info.pass_direct) {
                args.push_back(val);
            } else if (info.pass_by_ptr) {
                llvm::Value* copy = alloca_at_entry(m_active_module_context.function, "", arg->type);
                m_active_module_context.builder->CreateStore(val, copy);
                args.push_back(copy);
            }
        }

        llvm::Value* callee = gen_expr(mc.callee);
        return m_active_module_context.builder->CreateCall(llvm::FunctionCallee(llvm::dyn_cast<llvm::FunctionType>(type_info_to_llvm_type(mc.callee->type)), callee), args, expr->type->is_void() ? "" : "call");
    }

    llvm::Value* Codegen::gen_delete_expr(Expr* expr) {
        // DeleteExpr& del = expr->delete_;
        // 
        // llvm::Function* func = m_active_module_context.module->getFunction("free");
        // 
        // if (!func) {
        //     llvm::FunctionType* type = llvm::FunctionType::get(type_info_to_llvm_type(&void_type), { type_info_to_llvm_type(&void_ptr_type) }, false);
        //     func = llvm::Function::Create(type, llvm::GlobalValue::ExternalLinkage, "free", *m_active_module_context.module);
        // }
        // 
        // llvm::Value* ptr = gen_expr(del.expression);
        // return m_active_module_context.builder->CreateCall(llvm::FunctionCallee(func), ptr);
        return nullptr;
    }

    llvm::Value* Codegen::gen_sizeof_expr(Expr* expr) {
        SizeofExpr& sz = expr->sizeof_;

        if (sz.type) {
            return m_active_module_context.builder->getInt64(get_type_size(sz.type));
        } else {
            return m_active_module_context.builder->getInt64(get_type_size(sz.expression->type));
        }
    }

    llvm::Value* Codegen::gen_implicit_cast_expr(Expr* expr) {
        ImplicitCastExpr& ic = expr->implicit_cast;

        switch (ic.kind) {
            case CastKind::Integral: {
                llvm::Value* val = gen_expr(ic.expression);

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
                return gen_expr(ic.expression);
            }

            case CastKind::ArrayToSlice: {
                llvm::Value* slice = alloca_at_entry(m_active_module_context.function, "arrtoslice", expr->type);
                llvm::Value* arr = gen_expr(ic.expression);

                llvm::Value* mem = m_active_module_context.builder->CreateGEP(type_info_to_llvm_type(expr->type), slice, m_active_module_context.builder->getInt64(0), "ptradd", llvm::GEPNoWrapFlags::inBounds());
                llvm::Value* len = m_active_module_context.builder->CreateGEP(type_info_to_llvm_type(expr->type), slice, m_active_module_context.builder->getInt64(1), "ptradd", llvm::GEPNoWrapFlags::inBounds());

                m_active_module_context.builder->CreateStore(arr, mem);
                m_active_module_context.builder->CreateStore(m_active_module_context.builder->getInt64(ic.expression->type->array.size), len);

                return slice;
            }

            case CastKind::ArrayToPointer: {
                if (ic.expression->value_kind == ExprValueKind::LValue) {
                    llvm::Value* val = gen_expr(ic.expression);
                    llvm::Value* zero = m_active_module_context.builder->getInt64(0);
                    return m_active_module_context.builder->CreateGEP(type_info_to_llvm_type(ic.expression->type), val, { zero, zero }, "arraydecay", llvm::GEPNoWrapFlags::inBounds());
                } else {
                    return gen_expr(ic.expression);
                }
            }

            case CastKind::LValueToRValue: {
                if (ic.expression->kind == ExprKind::StringLiteral) {
                    return gen_expr(ic.expression);
                }
                
                llvm::Value* val = gen_expr(ic.expression);
                return m_active_module_context.builder->CreateLoad(type_info_to_llvm_type(expr->type), val);
            }

            default: ARIA_UNREACHABLE();
        }
    }

    llvm::Value* Codegen::gen_cast_expr(Expr* expr) {
        CastExpr& cast = expr->cast;
        return gen_expr(cast.expression);
    }

    llvm::Value* Codegen::gen_unary_operator_expr(Expr* expr) {
        UnaryOperatorExpr& un = expr->unary_operator;

        switch (un.op) {
            case UnaryOperatorKind::AddressOf: {
                return gen_expr(un.expression);
            }

            case UnaryOperatorKind::RValueAddressOf: {
                llvm::Value* val = gen_expr(un.expression);
                llvm::Value* temp = alloca_at_entry(m_active_module_context.function, "tempaddr", un.expression->type);
                m_active_module_context.builder->CreateStore(val, temp);

                return temp;
            }

            case UnaryOperatorKind::Dereference: {
                return gen_expr(un.expression);
            }

            default: ARIA_UNREACHABLE();
        }
    }

    llvm::Value* Codegen::gen_binary_operator_expr(Expr* expr) {
        BinaryOperatorExpr& bin = expr->binary_operator;

        switch (bin.op) {
            case BinaryOperatorKind::Add: {
                llvm::Value* lhs = gen_expr(bin.lhs);
                llvm::Value* rhs = gen_expr(bin.rhs);

                if (expr->type->is_integral()) {
                    return m_active_module_context.builder->CreateAdd(lhs, rhs, "add");
                } else if (expr->type->is_floating_point()) {
                    return m_active_module_context.builder->CreateFAdd(lhs, rhs, "fadd");
                }

                ARIA_UNREACHABLE();
                break;
            }

            case BinaryOperatorKind::Sub: {
                llvm::Value* lhs = gen_expr(bin.lhs);
                llvm::Value* rhs = gen_expr(bin.rhs);

                if (expr->type->is_integral()) {
                    return m_active_module_context.builder->CreateSub(lhs, rhs, "sub");
                } else if (expr->type->is_floating_point()) {
                    return m_active_module_context.builder->CreateFSub(lhs, rhs, "fsub");
                }

                ARIA_UNREACHABLE();
                break;
            }

            case BinaryOperatorKind::Less: {
                llvm::Value* lhs = gen_expr(bin.lhs);
                llvm::Value* rhs = gen_expr(bin.rhs);

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
                llvm::Value* lhs = gen_expr(bin.lhs);
                llvm::Value* rhs = gen_expr(bin.rhs);

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
                llvm::Value* lhs = gen_expr(bin.lhs);
                llvm::Value* rhs = gen_expr(bin.rhs);

                return m_active_module_context.builder->CreateStore(rhs, lhs);
            }

            default: ARIA_UNREACHABLE();
        }
    }

    llvm::Value* Codegen::gen_compound_assign_expr(Expr* expr) {
        CompoundAssignExpr& comp = expr->compound_assign;

        llvm::Value* lhs = gen_expr(comp.lhs);
        llvm::Value* lhs_val = m_active_module_context.builder->CreateLoad(type_info_to_llvm_type(comp.lhs->type), lhs);
        llvm::Value* rhs = gen_expr(comp.rhs);

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

            case BinaryOperatorKind::CompoundMul: {
                if (expr->type->is_integral()) {
                    llvm::Value* add = m_active_module_context.builder->CreateMul(lhs_val, rhs, "mul");
                    m_active_module_context.builder->CreateStore(add, lhs);
                    return lhs;
                } else if (expr->type->is_floating_point()) {
                    llvm::Value* add = m_active_module_context.builder->CreateFMul(lhs_val, rhs, "fmul");
                    m_active_module_context.builder->CreateStore(add, lhs);
                    return lhs;
                }

                ARIA_UNREACHABLE();
                break;
            }

            default: ARIA_UNREACHABLE();
        }
    }

    llvm::Value* Codegen::gen_expr(Expr* expr) {
        switch (expr->kind) {
            case ExprKind::BooleanLiteral: return gen_boolean_literal_expr(expr);
            case ExprKind::CharacterLiteral: return gen_character_literal_expr(expr);
            case ExprKind::IntegerLiteral: return gen_integer_literal_expr(expr);
            case ExprKind::FloatingLiteral: return gen_floating_literal_expr(expr);
            case ExprKind::StringLiteral: return gen_string_literal_expr(expr);
            case ExprKind::ArrayFiller: return gen_array_filler_expr(expr);
            case ExprKind::Null: return gen_null_expr(expr);
            case ExprKind::DeclRef: return gen_decl_ref_expr(expr);
            case ExprKind::Member: return gen_member_expr(expr);
            case ExprKind::BuiltinMember: return gen_builtin_member_expr(expr);
            case ExprKind::Self: return gen_self_expr(expr);
            case ExprKind::Call: return gen_call_expr(expr);
            case ExprKind::MethodCall: return gen_method_call_expr(expr);
            case ExprKind::Delete: return gen_delete_expr(expr);
            case ExprKind::Sizeof: return gen_sizeof_expr(expr);
            case ExprKind::ImplicitCast: return gen_implicit_cast_expr(expr);
            case ExprKind::Cast: return gen_cast_expr(expr);
            case ExprKind::UnaryOperator: return gen_unary_operator_expr(expr);
            case ExprKind::BinaryOperator: return gen_binary_operator_expr(expr);
            case ExprKind::CompoundAssign: return gen_compound_assign_expr(expr);

            default: ARIA_UNREACHABLE();
        }
    }

    llvm::Value* Codegen::gen_init_expr(Expr* expr, llvm::Value* dst) {
        llvm::Value* val = gen_expr(expr);

        if (expr->type->is_array()) {
            // Call memcpy since we copy arrays
            llvm::Value* size = m_active_module_context.builder->getInt64(expr->type->array.size);
            return m_active_module_context.builder->CreateMemCpy(dst, llvm::MaybeAlign::MaybeAlign(), val, llvm::MaybeAlign::MaybeAlign(), size);
        } else {
            return m_active_module_context.builder->CreateStore(val, dst);
        }
    }

} // namespace ariac