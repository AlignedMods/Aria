#include "ariac/codegen/codegen.hpp"

namespace ariac {

    llvm::Value* Codegen::gen_boolean_literal_expr(Expr* expr) {
        BooleanLiteralExpr& bl = expr->boolean_literal;
        set_debug_loc(expr->loc);
        return llvm::ConstantInt::get(*m_active_module_context.context, llvm::APInt(8, static_cast<u64>(bl.value), false));
    }

    llvm::Value* Codegen::gen_character_literal_expr(Expr* expr) {
        CharacterLiteralExpr& cl = expr->character_literal;
        set_debug_loc(expr->loc);
        return llvm::ConstantInt::get(*m_active_module_context.context, llvm::APInt(8, cl.value, expr->type->is_signed()));
    }

    llvm::Value* Codegen::gen_integer_literal_expr(Expr* expr) {
        IntegerLiteralExpr& il = expr->integer_literal;
        set_debug_loc(expr->loc);
        return llvm::ConstantInt::get(*m_active_module_context.context, llvm::APInt(static_cast<unsigned>(expr->type->get_bit_size()), il.value));
    }

    llvm::Value* Codegen::gen_floating_literal_expr(Expr* expr) {
        FloatingLiteralExpr& fl = expr->floating_literal;
        set_debug_loc(expr->loc);
        if (expr->type->kind == TypeKind::Float) {
            return llvm::ConstantFP::get(*m_active_module_context.context, llvm::APFloat(static_cast<float>(fl.value)));
        } else {
            return llvm::ConstantFP::get(*m_active_module_context.context, llvm::APFloat(fl.value));
        }
    }

    llvm::Value* Codegen::gen_string_literal_expr(Expr* expr) {
        StringLiteralExpr& sl = expr->string_literal;
        set_debug_loc(expr->loc);
       
        if (expr->type->is_string()) {
            llvm::GlobalVariable* str = m_active_module_context.builder->CreateGlobalString(sl.value, "str", 0, nullptr);
            llvm::Constant* vals[2] = { str, m_active_module_context.builder->getInt64(sl.value.length()) };
            return llvm::ConstantStruct::get(llvm::dyn_cast<llvm::StructType>(type_info_to_llvm_type(expr->type)), llvm::ArrayRef(vals));
        } else {
            return m_active_module_context.builder->CreateGlobalString(sl.value, "cstr");
        }
    }

    llvm::Value* Codegen::gen_array_filler_expr(Expr* expr) {
        ARIA_TODO("array filler");
    }

    llvm::Value* Codegen::gen_null_expr(Expr* expr) {
        set_debug_loc(expr->loc);
        return llvm::Constant::getNullValue(type_info_to_llvm_type(expr->type));
    }

    llvm::Value* Codegen::gen_decl_ref_expr(Expr* expr) {
        DeclRefExpr& dr = expr->decl_ref;
        set_debug_loc(expr->loc);
            
        if (dr.referenced_decl->kind == DeclKind::Function) {
            if (!m_active_module_context.functions.contains(dr.referenced_decl)) {
                gen_function_prototype(dr.referenced_decl);
            }

            return m_active_module_context.functions.at(dr.referenced_decl);
        }

        if (dr.referenced_decl->kind == DeclKind::Var && dr.referenced_decl->var.const_var) {
            return gen_expr(dr.referenced_decl->var.initializer);
        }

        ARIA_ASSERT(m_active_module_context.named_values.contains(dr.referenced_decl), "Invalid DeclRef expression");
        llvm::Value* val = m_active_module_context.named_values.at(dr.referenced_decl);
        ARIA_ASSERT(val, "Invalid DeclRef expression");

        if (dr.referenced_decl->kind == DeclKind::Param && get_param_abi_type_info(expr->type).kind == ABIParamKind::Pointer) {
            return m_active_module_context.builder->CreateLoad(type_info_to_llvm_type(TypeInfo::get_void_ptr()), val);
        }

        return val;
    }

    llvm::Value* Codegen::gen_typeinfo_expr(Expr* expr) {
        TypeInfoExpr& ti = expr->type_info;
        return get_typeid(ti.type);
    }

    llvm::Value* Codegen::gen_member_expr(Expr* expr) {
        MemberExpr& mem = expr->member;
        set_debug_loc(expr->loc);

        switch (mem.referenced_member->kind) {
            case DeclKind::Field: {
                llvm::Value* val = gen_expr(mem.parent);
                TypeInfo* type = mem.parent->type;
                size_t idx = 0;
                TinyVector<Decl*> fields;

                if (mem.implicit_deref) {
                    type = mem.parent->type->pointer.base;
                    if (mem.parent->value_kind == ExprValueKind::LValue) {
                        val = m_active_module_context.builder->CreateLoad(type_info_to_llvm_type(TypeInfo::get_void_ptr()), val);
                    }
                }

                switch (type->kind) {
                    case TypeKind::Struct: {
                        fields = type->struct_.source_decl->struct_.fields;
                        break;
                    }

                    case TypeKind::StructSpecilization: {
                        fields = type->struct_specilization.resolved_decl->struct_specilization.source->struct_.fields;
                        break;
                    }

                    default: ARIA_UNREACHABLE("Invalid type kind");
                }

                for (Decl* field : fields) {
                    if (field == mem.referenced_member) { break; }
                    idx++;
                }

                ARIA_ASSERT(val, "Invalid value for MemberExpr");
                llvm::Value* gep = m_active_module_context.builder->CreateStructGEP(type_info_to_llvm_type(type), val, static_cast<unsigned>(idx), "ptradd");

                return gep;
            }

            case DeclKind::Method: {
                if (!m_active_module_context.functions.contains(mem.referenced_member)) {
                    gen_method_prototype(mem.referenced_member);
                }

                return m_active_module_context.functions.at(mem.referenced_member);
            }

            case DeclKind::Destructor: {
                if (!m_active_module_context.functions.contains(mem.referenced_member)) {
                    gen_destructor_prototype(mem.referenced_member);
                }

                return m_active_module_context.functions.at(mem.referenced_member);
            }

            case DeclKind::EnumConstant: {
                return m_active_module_context.builder->getIntN(expr->type->enum_.source_decl->enum_.backing_type->get_bit_size(), 
                    mem.referenced_member->enum_constant.resolved_value);
            }

            default: ARIA_UNREACHABLE("Invalid decl kind");
        }
    }

    llvm::Value* Codegen::gen_builtin_member_expr(Expr* expr) {
        MemberExpr& mem = expr->member;
        set_debug_loc(expr->loc);
        
        llvm::Value* val = gen_expr(mem.parent);
        TypeInfo* type = TypeInfo::get_flattened(mem.parent->type);

        if (mem.implicit_deref) {
            type = mem.parent->type->pointer.base;
        }

        switch (type->kind) {
            case TypeKind::Typeid: {
                val = m_active_module_context.builder->CreateLoad(llvm::PointerType::get(*m_active_module_context.context, 0), val);
                llvm::Type* ty = llvm::StructType::getTypeByName(*m_active_module_context.context, "$builtin_typeid");

                if (mem.member == "name") {
                    return m_active_module_context.builder->CreateStructGEP(ty, val, 0, "ptradd");
                } else if (mem.member == "kind") {
                    return m_active_module_context.builder->CreateStructGEP(ty, val, 1, "ptradd");
                } else if (mem.member == "size") {
                    return m_active_module_context.builder->CreateStructGEP(ty, val, 2, "ptradd");
                } else if (mem.member == "len") {
                    return m_active_module_context.builder->CreateStructGEP(ty, val, 3, "ptradd");
                } else if (mem.member == "inner") {
                    return m_active_module_context.builder->CreateStructGEP(ty, val, 4, "ptradd");
                }

                ARIA_UNREACHABLE("Should never be reached");
                break;
            }

            case TypeKind::Any: {
                if (mem.member == "type") {
                    return m_active_module_context.builder->CreateStructGEP(type_info_to_llvm_type(type), val, 0, "ptradd");
                } else if (mem.member == "value") {
                    return m_active_module_context.builder->CreateStructGEP(type_info_to_llvm_type(type), val, 1, "ptradd");
                }

                ARIA_UNREACHABLE("Should never be reached");
                break;
            }

            case TypeKind::Array: {
                if (mem.member == "mem") {
                    return val;
                } else if (mem.member == "len") {
                    return llvm::ConstantInt::get(type_info_to_llvm_type(expr->type), mem.parent->type->array.size);
                } 

                ARIA_UNREACHABLE("Should never be reached");
                break;
            }

            case TypeKind::String:
            case TypeKind::Slice: {
                if (mem.member == "mem") {
                    return m_active_module_context.builder->CreateStructGEP(type_info_to_llvm_type(type), val, 0, "ptradd");
                } else if (mem.member == "len") {
                    return m_active_module_context.builder->CreateStructGEP(type_info_to_llvm_type(type), val, 1, "ptradd");
                } 

                ARIA_UNREACHABLE("Should never be reached");
                break;
            }

            default: ARIA_UNREACHABLE("Invalid type kind");
        }
    }

    llvm::Value* Codegen::gen_type_member_expr(Expr* expr) {
        TypeMemberExpr& mem = expr->type_member;

        if (mem.member == "name") {
            return get_string(mem.type->to_string());
        } else if (mem.member == "size") {
            return get_sz(mem.type->get_size());
        }

        switch (mem.type->kind) {
            case TypeKind::Enum: {
                ARIA_ASSERT(mem.referenced_member && mem.referenced_member->kind == DeclKind::EnumConstant, "Invalid reference");
                return get_int(mem.referenced_member->enum_constant.resolved_value, mem.type->enum_.source_decl->enum_.backing_type);
            }

            default: ARIA_UNREACHABLE("Should never be reached");
        }
    }

    llvm::Value* Codegen::gen_self_expr(Expr* expr) {
        set_debug_loc(expr->loc);
        return m_self_value;
    }

    llvm::Value* Codegen::gen_call_expr(Expr* expr) {
        CallExpr& call = expr->call;
        set_debug_loc(expr->loc);
        
        std::vector<llvm::Value*> args;

        for (size_t i = 0; i < call.arguments.size; i++) {
            // Check if we are handlng a named variadic parameter
            if (call.callee->type->function.variadic == VariadicKind::Named && i == call.callee->type->function.param_types.size - 1) {
                break;
            }

            Expr* arg = call.arguments.items[i];
            gen_call_param(&args, gen_expr(arg), arg->type);
        }

        if (call.callee->type->function.variadic == VariadicKind::Named) {
            std::vector<llvm::Value*> vals;
            std::vector<TypeInfo*> types;

            for (size_t i = call.callee->type->function.param_types.size - 1; i < call.arguments.size; i++) {
                vals.push_back(gen_expr(call.arguments.items[i]));
                types.push_back(call.arguments.items[i]->type);
            }

            gen_call_variadic(&args, vals, types);
        }

        llvm::Value* callee = gen_expr(call.callee);
        ARIA_ASSERT(callee, "Invalid function callee");

        TypeInfo* callee_type = TypeInfo::get_flattened(call.callee->type);
        llvm::Value* c = gen_call_raw(args, callee, callee_type->is_pointer() ? callee_type->pointer.base : callee_type);
        return c;
    }

    llvm::Value* Codegen::gen_builtin_call_expr(Expr* expr) {
        BuiltinCallExpr& b = expr->builtin_call;
        set_debug_loc(expr->loc);

        switch (b.kind) {
            case BuiltinCallKind::Sizeof: {
                return m_active_module_context.builder->getInt(llvm::APInt((unsigned)expr->type->get_bit_size(), b.arguments.items[0]->type->get_size()));
            }

            case BuiltinCallKind::Memcpy: {
                llvm::Value* dst = gen_expr(b.arguments.items[0]);
                llvm::Value* src = gen_expr(b.arguments.items[1]);
                llvm::Value* len = gen_expr(b.arguments.items[2]);
                
                return m_active_module_context.builder->CreateMemCpy(dst, llvm::MaybeAlign(), src, llvm::MaybeAlign(), len);
            }

            case BuiltinCallKind::Memset: {
                llvm::Value* ptr = gen_expr(b.arguments.items[0]);
                llvm::Value* val = gen_expr(b.arguments.items[1]);
                llvm::Value* len = gen_expr(b.arguments.items[2]);
                
                return m_active_module_context.builder->CreateMemSet(ptr, val, len, llvm::MaybeAlign());
            }

            default: ARIA_UNREACHABLE("Invalid builtin call kind");
        }
    }

    llvm::Value* Codegen::gen_construct_expr(Expr* expr) {
        ConstructExpr& ct = expr->construct;
        set_debug_loc(expr->loc);

        llvm::Type* type = type_info_to_llvm_type(expr->type);

        switch (expr->type->kind) {
            case TypeKind::Bool:
            case TypeKind::Char:
            case TypeKind::IChar:
            case TypeKind::Short:
            case TypeKind::UShort:
            case TypeKind::Int:
            case TypeKind::UInt:
            case TypeKind::Long:
            case TypeKind::ULong:
            case TypeKind::Float:
            case TypeKind::Double:
            case TypeKind::Pointer: {
                if (ct.arguments.size == 0) {
                    return llvm::Constant::getNullValue(type);
                }

                if (ct.arguments.size == 1) {
                    return gen_expr(ct.arguments.items[0]);
                }

                ARIA_UNREACHABLE("Invalid amount of args");
            }

            case TypeKind::Typeid: {
                if (ct.arguments.size == 0) {
                    return llvm::Constant::getNullValue(type);
                }

                if (ct.arguments.size == 1) {
                    return gen_expr(ct.arguments.items[0]);
                }

                ARIA_UNREACHABLE("Invalid amount of args");
            }

            case TypeKind::Array: {
                if (ct.is_const) {
                    std::vector<llvm::Constant*> fields;
                    for (Expr* arg : ct.arguments) {
                        fields.push_back(llvm::dyn_cast<llvm::Constant>(gen_expr(arg)));
                    }

                    // Pad the rest of the array constant with null values
                    for (size_t i = ct.arguments.size; i < expr->type->array.size; i++) {
                        fields.push_back(llvm::Constant::getNullValue(type->getArrayElementType()));
                    }

                    return llvm::ConstantArray::get(llvm::dyn_cast<llvm::ArrayType>(type), fields);
                } else {
                    llvm::Value* temp = alloca_at_entry(m_active_module_context.function, "construct", type);
                    llvm::Value* zero = llvm::Constant::getNullValue(type);
                    m_active_module_context.builder->CreateStore(zero, temp);

                    for (size_t i = 0; i < ct.arguments.size; i++) {
                        llvm::Value* arg = gen_expr(ct.arguments.items[i]);
                        llvm::Value* field = m_active_module_context.builder->CreateGEP(type, temp, { get_i64(0), get_i64(i) }, "ptradd");

                        m_active_module_context.builder->CreateStore(arg, field);
                    }

                    return m_active_module_context.builder->CreateLoad(type, temp);
                }
            }

            case TypeKind::Any:
            case TypeKind::Struct:
            case TypeKind::StructSpecilization: {
                if (ct.is_const) {
                    std::vector<llvm::Constant*> fields;
                    for (Expr* arg : ct.arguments) {
                        fields.push_back(llvm::dyn_cast<llvm::Constant>(gen_expr(arg)));
                    }
                    return llvm::ConstantStruct::get(llvm::dyn_cast<llvm::StructType>(type), fields);
                } else {
                    llvm::Value* temp = alloca_at_entry(m_active_module_context.function, "construct", type);
                    llvm::Value* zero = llvm::Constant::getNullValue(type);
                    m_active_module_context.builder->CreateStore(zero, temp);

                    for (size_t i = 0; i < ct.arguments.size; i++) {
                        llvm::Value* arg = gen_expr(ct.arguments.items[i]);
                        llvm::Value* field = m_active_module_context.builder->CreateStructGEP(type, temp, static_cast<unsigned>(i), "ptradd");

                        m_active_module_context.builder->CreateStore(arg, field);
                    }

                    return m_active_module_context.builder->CreateLoad(type, temp);
                }
            }

            default: ARIA_UNREACHABLE("Invalid type kind");
        }
    }

    llvm::Value* Codegen::gen_array_literal_expr(Expr* expr) {
        ArrayLiteralExpr& lit = expr->array_literal;
        set_debug_loc(expr->loc);

        llvm::Type* type = type_info_to_llvm_type(expr->type);

        if (lit.is_const) {
            std::vector<llvm::Constant*> fields;
            for (Expr* arg : lit.arguments) {
                fields.push_back(llvm::dyn_cast<llvm::Constant>(gen_expr(arg)));
            }
            return llvm::ConstantArray::get(llvm::dyn_cast<llvm::ArrayType>(type), fields);
        } else {
            llvm::Value* temp = alloca_at_entry(m_active_module_context.function, "arrayliteral", expr->type);
            llvm::Value* zero = llvm::Constant::getNullValue(type);
            m_active_module_context.builder->CreateStore(zero, temp);

            for (size_t i = 0; i < lit.arguments.size; i++) {
                llvm::Value* arg = gen_expr(lit.arguments.items[i]);

                llvm::Value* zero_val = m_active_module_context.builder->getInt64(0);
                llvm::Value* i_val = m_active_module_context.builder->getInt64(i);

                llvm::Value* field = m_active_module_context.builder->CreateGEP(type, temp, { zero_val, i_val }, "ptradd");
                m_active_module_context.builder->CreateStore(arg, field);
            }

            return m_active_module_context.builder->CreateLoad(type, temp);
        }
    }

    llvm::Value* Codegen::gen_method_call_expr(Expr* expr) {
        CallExpr& mc = expr->call;
        set_debug_loc(expr->loc);

        std::vector<llvm::Value*> args;
        args.push_back(gen_expr(mc.callee->member.parent)); // push self

        for (size_t i = 0; i < mc.arguments.size; i++) {
            // Check if we are handlng a named variadic parameter
            if (mc.callee->type->function.variadic == VariadicKind::Named && i == mc.callee->type->function.param_types.size - 1) {
                break;
            }

            Expr* arg = mc.arguments.items[i];
            gen_call_param(&args, gen_expr(arg), arg->type);
        }

        if (mc.callee->type->function.variadic == VariadicKind::Named) {
            std::vector<llvm::Value*> vals;
            std::vector<TypeInfo*> types;

            for (size_t i = mc.callee->type->function.param_types.size - 1; i < mc.arguments.size; i++) {
                vals.push_back(gen_expr(mc.arguments.items[i]));
                types.push_back(mc.arguments.items[i]->type);
            }

            gen_call_variadic(&args, vals, types);
        }

        llvm::Value* callee = gen_expr(mc.callee);
        ARIA_ASSERT(callee, "Invalid function callee");

        return gen_call_raw(args, callee, mc.callee->type);
    }

    llvm::Value* Codegen::gen_array_subscript_expr(Expr* expr) {
        ArraySubscriptExpr& arr = expr->array_subscript;
        set_debug_loc(expr->loc);

        llvm::Value* index = gen_expr(arr.index);
        llvm::Value* array = gen_expr(arr.array);

        switch (arr.array->type->kind) {
            case TypeKind::Pointer:
                return m_active_module_context.builder->CreateGEP(type_info_to_llvm_type(arr.array->type->pointer.base), array, index, "ptradd");

            case TypeKind::Array: {
                llvm::Value* cond = m_active_module_context.builder->CreateICmpULT(index, get_sz(arr.array->type->array.size), "lt");
                call_assert(cond, expr->loc.line, "Array index out of bounds (len: %s, index: %s)",
                    { get_sz(arr.array->type->array.size), index }, { TypeInfo::get_sz(), TypeInfo::get_sz() });

                return m_active_module_context.builder->CreateGEP(type_info_to_llvm_type(arr.array->type), array, { m_active_module_context.builder->getInt64(0), index }, "ptradd");
            }

            case TypeKind::String: {
                llvm::Type* slice_type = type_info_to_llvm_type(arr.array->type);

                if (!array->getType()->isPointerTy()) {
                    llvm::Value* tempaddr = alloca_at_entry(m_active_module_context.function, "tempaddr", array->getType());
                    m_active_module_context.builder->CreateStore(array, tempaddr);
                    array = tempaddr;
                }

                llvm::Value* len = m_active_module_context.builder->CreateStructGEP(slice_type, array, 1, "ptradd");
                len = m_active_module_context.builder->CreateLoad(m_active_module_context.builder->getIntNTy((unsigned)TypeInfo::get_sz()->get_bit_size()), len);
                llvm::Value* cond = m_active_module_context.builder->CreateICmpULT(index, len, "lt");
                call_assert(cond, expr->loc.line, "String index out of bounds (len: %s, index: %s)",
                    { len, index }, { TypeInfo::get_sz(), TypeInfo::get_sz() });

                llvm::Value* mem = m_active_module_context.builder->CreateStructGEP(slice_type, array, 0);
                llvm::Value* loaded = m_active_module_context.builder->CreateLoad(llvm::PointerType::get(*m_active_module_context.context, 0), mem);
                return m_active_module_context.builder->CreateGEP(m_active_module_context.builder->getInt8Ty(), loaded, index, "ptradd");
            }

            case TypeKind::Slice: {
                llvm::Type* slice_type = type_info_to_llvm_type(arr.array->type);

                if (!array->getType()->isPointerTy()) {
                    llvm::Value* tempaddr = alloca_at_entry(m_active_module_context.function, "tempaddr", array->getType());
                    m_active_module_context.builder->CreateStore(array, tempaddr);
                    array = tempaddr;
                }

                llvm::Value* len = m_active_module_context.builder->CreateStructGEP(slice_type, array, 1, "ptradd");
                len = m_active_module_context.builder->CreateLoad(m_active_module_context.builder->getIntNTy((unsigned)TypeInfo::get_sz()->get_bit_size()), len);
                llvm::Value* cond = m_active_module_context.builder->CreateICmpULT(index, len, "lt");
                call_assert(cond, expr->loc.line, "Array index out of bounds (len: %s, index: %s)",
                    { len, index }, { TypeInfo::get_sz(), TypeInfo::get_sz() });

                llvm::Value* mem = m_active_module_context.builder->CreateStructGEP(slice_type, array, 0);
                llvm::Value* loaded = m_active_module_context.builder->CreateLoad(llvm::PointerType::get(*m_active_module_context.context, 0), mem);
                return m_active_module_context.builder->CreateGEP(type_info_to_llvm_type(arr.array->type->slice.base), loaded, index, "ptradd");
            }

            default: ARIA_UNREACHABLE("Invalid type kind");
        }
    }

    llvm::Value* Codegen::gen_to_slice_expr(Expr* expr) {
        ToSliceExpr& t = expr->to_slice;
        set_debug_loc(expr->loc);

        llvm::Type* type = type_info_to_llvm_type(expr->type);
        llvm::Value* slice = alloca_at_entry(m_active_module_context.function, "to_slice", type);

        llvm::Value* mem = m_active_module_context.builder->CreateStructGEP(type, slice, 0, "ptradd");
        llvm::Value* mem_val = gen_expr(t.source);
        m_active_module_context.builder->CreateStore(mem_val, mem);

        llvm::Value* len = m_active_module_context.builder->CreateStructGEP(type, slice, 1, "ptradd");
        llvm::Value* len_val = nullptr;
        
        if (t.len) {
            len_val = gen_expr(t.len);
        } else {
            ARIA_ASSERT(t.source->type->is_array(), "Type must be an array");
            len_val = m_active_module_context.builder->getInt(llvm::APInt((unsigned)TypeInfo::get_basic(TypeKind::Sz)->get_bit_size(), t.source->type->array.size));
        }

        m_active_module_context.builder->CreateStore(len_val, len);

        return m_active_module_context.builder->CreateLoad(type, slice);
    }

    llvm::Value* Codegen::gen_move_expr(Expr* expr) {
        MoveExpr& m = expr->move;

        llvm::Value* temp = alloca_at_entry(m_active_module_context.function, "move", expr->type);
        llvm::Value* val = gen_expr(m.expression);
        m_active_module_context.builder->CreateMemCpy(temp, llvm::MaybeAlign(), val, llvm::MaybeAlign(), get_sz(expr->type->get_size()));
        m_active_module_context.builder->CreateMemSet(val, get_int(0, TypeInfo::get_basic(TypeKind::Char)), get_sz(expr->type->get_size()), llvm::MaybeAlign());

        return m_active_module_context.builder->CreateLoad(type_info_to_llvm_type(expr->type), temp);
    }

    llvm::Value* Codegen::gen_temporary_expr(Expr* expr) {
        TemporaryExpr& t = expr->temporary;

        llvm::Type* ty = type_info_to_llvm_type(expr->type);
        llvm::Value* temp = alloca_at_entry(m_active_module_context.function, "temp.expr", ty);
        llvm::Value* val = gen_expr(t.expression);
        m_active_module_context.builder->CreateStore(val, temp);

        if (!m_active_module_context.functions.contains(t.dtor)) { gen_destructor_prototype(t.dtor); }
        llvm::Function* dtor = m_active_module_context.functions.at(t.dtor);

        m_active_module_context.temps.push_back({ temp, dtor });
        return m_active_module_context.builder->CreateLoad(ty, temp);
    }

    llvm::Value* Codegen::gen_materialize_temporary_expr(Expr* expr) {
        MaterializeTemporaryExpr& t = expr->materialize_temporary;

        llvm::Value* val = gen_expr(t.expression);
        llvm::Value* temp = alloca_at_entry(m_active_module_context.function, "tempaddr", t.expression->type);
        m_active_module_context.builder->CreateStore(val, temp);
        return temp;
    }

    llvm::Value* Codegen::gen_expr_with_cleanups(Expr* expr) {
        ExprWithCleanups& e = expr->expr_with_cleanups;

        llvm::Value* val = gen_expr(e.expression);

        for (auto& temp : m_active_module_context.temps) {
            m_active_module_context.builder->CreateCall(temp.dtor, temp.val);
        }
        m_active_module_context.temps.clear();

        return val;
    }

    llvm::Value* Codegen::gen_paren_expr(Expr* expr) {
        ParenExpr& p = expr->paren;
        set_debug_loc(expr->loc);
        return gen_expr(p.expression);
    }

    // TODO: Optimize certain cases to a 'select' instruction
    // This should only be done if both expression have no side effects
    // Expressions like calls do have side effects while integer literals do not
    llvm::Value* Codegen::gen_ternary_expr(Expr* expr) {
        TernaryExpr& t = expr->ternary;
        set_debug_loc(expr->loc);

        llvm::BasicBlock* true_block = create_block("ternary.true");
        llvm::BasicBlock* false_block = create_block("ternary.false");
        llvm::BasicBlock* end_block = create_block("ternary.end");

        llvm::Value* cond = gen_cond(t.condition);
        m_active_module_context.builder->CreateCondBr(cond, true_block, false_block);

        m_active_module_context.builder->SetInsertPoint(true_block);
        llvm::Value* first = gen_expr(t.first);
        m_active_module_context.builder->CreateBr(end_block);

        m_active_module_context.builder->SetInsertPoint(false_block);
        llvm::Value* second = gen_expr(t.second);
        m_active_module_context.builder->CreateBr(end_block);

        m_active_module_context.builder->SetInsertPoint(end_block);
        llvm::PHINode* phi = m_active_module_context.builder->CreatePHI(first->getType(), 2);
        phi->addIncoming(first, true_block);
        phi->addIncoming(second, false_block);

        return phi;
    }

    llvm::Value* Codegen::gen_implicit_cast_expr(Expr* expr) {
        ImplicitCastExpr& ic = expr->implicit_cast;
        set_debug_loc(expr->loc);

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

                ARIA_UNREACHABLE("Should never be reached");
                break;
            }

            case CastKind::Floating: {
                llvm::Value* val = gen_expr(ic.expression);

                if (ic.expression->type->get_bit_size() > expr->type->get_bit_size()) {
                    return m_active_module_context.builder->CreateFPTrunc(val, type_info_to_llvm_type(expr->type), "fptrunc");
                } else {
                    return m_active_module_context.builder->CreateFPExt(val, type_info_to_llvm_type(expr->type), "fpext");
                }

                ARIA_UNREACHABLE("Should never be reached");
                break;
            }

            case CastKind::IntegralToFloating: {
                llvm::Value* val = gen_expr(ic.expression);

                if (ic.expression->type->is_signed()) {
                    return m_active_module_context.builder->CreateSIToFP(val, type_info_to_llvm_type(expr->type), "sifp");
                } else {
                    return m_active_module_context.builder->CreateUIToFP(val, type_info_to_llvm_type(expr->type), "uifp");
                }

                ARIA_UNREACHABLE("Should never be reached");
                break;
            }

            case CastKind::FloatingToIntegral: {
                llvm::Value* val = gen_expr(ic.expression);

                if (expr->type->is_signed()) {
                    return m_active_module_context.builder->CreateFPToSI(val, type_info_to_llvm_type(expr->type), "fpsi");
                } else {
                    return m_active_module_context.builder->CreateFPToUI(val, type_info_to_llvm_type(expr->type), "fpui");
                }

                ARIA_UNREACHABLE("Should never be reached");
                break;
            }

            case CastKind::IntegerToPointer: {
                llvm::Value* val = gen_expr(ic.expression);
                return m_active_module_context.builder->CreateIntToPtr(val, llvm::PointerType::get(*m_active_module_context.context, 0), "inttoptr");
            }

            case CastKind::BitCast: {
                return gen_expr(ic.expression);
            }

            case CastKind::SliceToPointer: {
                llvm::Value* val = gen_expr(ic.expression);

                if (ic.expression->value_kind == ExprValueKind::RValue) {
                    llvm::Value* temp = alloca_at_entry(m_active_module_context.function, "tempaddr", val->getType());
                    m_active_module_context.builder->CreateStore(val, temp);
                    val = temp;
                }

                llvm::Value* gep = m_active_module_context.builder->CreateStructGEP(type_info_to_llvm_type(ic.expression->type), val, 0, "ptradd");
                return m_active_module_context.builder->CreateLoad(llvm::PointerType::get(*m_active_module_context.context, 0), gep);
            }

            case CastKind::PointerToAny: {
                llvm::Type* any_type = llvm::StructType::getTypeByName(*m_active_module_context.context, "$builtin_any");

                llvm::Value* typei = get_typeid(ic.expression->type->pointer.base);
                llvm::Value* value = gen_expr(ic.expression);

                llvm::Value* any_temp = alloca_at_entry(m_active_module_context.function, "ptrtoany", any_type);

                llvm::Value* any_typeid = m_active_module_context.builder->CreateStructGEP(any_type, any_temp, 0, "ptradd");
                m_active_module_context.builder->CreateStore(typei, any_typeid);

                llvm::Value* any_value = m_active_module_context.builder->CreateStructGEP(any_type, any_temp, 1, "ptradd");
                m_active_module_context.builder->CreateStore(value, any_value);

                return m_active_module_context.builder->CreateLoad(any_type, any_temp);
            }

            case CastKind::LValueToRValue: {
                if (ic.expression->kind == ExprKind::DeclRef) {
                    if (ic.expression->decl_ref.referenced_decl->kind == DeclKind::Var && ic.expression->decl_ref.referenced_decl->var.const_var) {
                        return gen_expr(ic.expression);
                    }
                }
                
                llvm::Value* val = gen_expr(ic.expression);
                val = m_active_module_context.builder->CreateLoad(type_info_to_llvm_type(expr->type), val);
                return val;
            }

            default: ARIA_UNREACHABLE("Invalid cast kind");
        }
    }

    llvm::Value* Codegen::gen_cast_expr(Expr* expr) {
        CastExpr& cast = expr->cast;
        set_debug_loc(expr->loc);
        return gen_expr(cast.expression);
    }

    llvm::Value* Codegen::gen_unary_operator_expr(Expr* expr) {
        UnaryOperatorExpr& un = expr->unary_operator;
        set_debug_loc(expr->loc);

        switch (un.op) {
            case UnaryOperatorKind::Not: {
                llvm::Value* val = gen_expr(un.expression);
                return m_active_module_context.builder->CreateNot(val, "not");
            }

            case UnaryOperatorKind::Negate: {
                llvm::Value* val = gen_expr(un.expression);

                if (un.expression->type->is_integral()) {
                    return m_active_module_context.builder->CreateNeg(val, "neg");
                } else if (un.expression->type->is_floating_point()) {
                    return m_active_module_context.builder->CreateFNeg(val, "fneg");
                }

                ARIA_UNREACHABLE("Should be unreachable");
                return nullptr;
            }

            case UnaryOperatorKind::AddressOf:
            case UnaryOperatorKind::RValueAddressOf: {
                return gen_expr(un.expression);
            }

            case UnaryOperatorKind::Dereference: {
                llvm::Value* val = gen_expr(un.expression);

                {
                    llvm::Value* cond = m_active_module_context.builder->CreateICmpNE(val, get_null(), "ne");
                    call_assert(cond, expr->loc.line, fmt::format("Dereference of null pointer ('{}' was null)",
                        std::string_view(context.active_comp_unit->source.c_str() + un.expression->loc.offset, un.expression->loc.len)));
                }

                {
                    llvm::Type* sz_type = type_info_to_llvm_type(TypeInfo::get_sz());
                    llvm::Value* alignment = get_i64(expr->type->get_alignment());
                    llvm::Value* ptrtoint = m_active_module_context.builder->CreatePtrToInt(val, sz_type);
                    llvm::Value* rem = m_active_module_context.builder->CreateURem(ptrtoint, alignment, "rem");
                    llvm::Value* cond = m_active_module_context.builder->CreateICmpEQ(rem, get_sz(0), "eq");
                    call_assert(cond, expr->loc.line, "Unaligned access ptr %% %s = %s", 
                        { alignment, rem }, { TypeInfo::get_sz(), TypeInfo::get_sz() });
                }
                
                return val;
            }

            case UnaryOperatorKind::PreIncrement: {
                llvm::Value* start_val = gen_expr(un.expression);
                llvm::Value* load = m_active_module_context.builder->CreateLoad(type_info_to_llvm_type(un.expression->type), start_val);

                if (un.expression->type->is_integral()) {
                    llvm::Value* inc = m_active_module_context.builder->CreateAdd(load, m_active_module_context.builder->getIntN((unsigned)un.expression->type->get_bit_size(), 1), "inc");
                    m_active_module_context.builder->CreateStore(inc, start_val);
                } else {
                    ARIA_UNREACHABLE("Invalid expression type");
                }

                return m_active_module_context.builder->CreateLoad(load->getType(), start_val);
            }

            case UnaryOperatorKind::PreDecrement: {
                llvm::Value* start_val = gen_expr(un.expression);
                llvm::Value* load = m_active_module_context.builder->CreateLoad(type_info_to_llvm_type(un.expression->type), start_val);

                if (un.expression->type->is_integral()) {
                    llvm::Value* dec = m_active_module_context.builder->CreateSub(load, m_active_module_context.builder->getIntN((unsigned)un.expression->type->get_bit_size(), 1), "dec");
                    m_active_module_context.builder->CreateStore(dec, start_val);
                } else {
                    ARIA_UNREACHABLE("Invalid expression type");
                }

                return m_active_module_context.builder->CreateLoad(load->getType(), start_val);
            }

            case UnaryOperatorKind::PostIncrement: {
                llvm::Value* start_val = gen_expr(un.expression);
                llvm::Value* load = m_active_module_context.builder->CreateLoad(type_info_to_llvm_type(un.expression->type), start_val);

                if (un.expression->type->is_integral()) {
                    llvm::Value* inc = m_active_module_context.builder->CreateAdd(load, m_active_module_context.builder->getIntN((unsigned)un.expression->type->get_bit_size(), 1), "inc");
                    m_active_module_context.builder->CreateStore(inc, start_val);
                } else {
                    ARIA_UNREACHABLE("Invalid expression type");
                }

                return load;
            }

            case UnaryOperatorKind::PostDecrement: {
                llvm::Value* start_val = gen_expr(un.expression);
                llvm::Value* load = m_active_module_context.builder->CreateLoad(type_info_to_llvm_type(un.expression->type), start_val);

                if (un.expression->type->is_integral()) {
                    llvm::Value* dec = m_active_module_context.builder->CreateSub(load, m_active_module_context.builder->getIntN((unsigned)un.expression->type->get_bit_size(), 1), "dec");
                    m_active_module_context.builder->CreateStore(dec, start_val);
                } else {
                    ARIA_UNREACHABLE("Invalid expression type");
                }

                return load;
            }

            default: ARIA_UNREACHABLE("Invalid unary operator");
        }
    }

    llvm::Value* Codegen::gen_binary_operator_expr(Expr* expr) {
        BinaryOperatorExpr& bin = expr->binary_operator;
        set_debug_loc(expr->loc);

        switch (bin.op) {
            case BinaryOperatorKind::Add: {
                llvm::Value* lhs = gen_expr(bin.lhs);
                llvm::Value* rhs = gen_expr(bin.rhs);

                if (expr->type->is_integral()) {
                    return m_active_module_context.builder->CreateAdd(lhs, rhs, "add");
                } else if (expr->type->is_floating_point()) {
                    return m_active_module_context.builder->CreateFAdd(lhs, rhs, "fadd");
                } else if (expr->type->is_pointer()) {
                    llvm::Type* base_type = expr->type->pointer.base->is_void() ? m_active_module_context.builder->getInt8Ty() : type_info_to_llvm_type(expr->type->pointer.base);
                    return m_active_module_context.builder->CreateGEP(base_type, lhs, rhs, "ptradd");
                }

                ARIA_UNREACHABLE("Should be unreachable");
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

                ARIA_UNREACHABLE("Should be unreachable");
                break;
            }

            case BinaryOperatorKind::Mul: {
                llvm::Value* lhs = gen_expr(bin.lhs);
                llvm::Value* rhs = gen_expr(bin.rhs);

                if (expr->type->is_integral()) {
                    return m_active_module_context.builder->CreateMul(lhs, rhs, "mul");
                } else if (expr->type->is_floating_point()) {
                    return m_active_module_context.builder->CreateFMul(lhs, rhs, "fmul");
                }

                ARIA_UNREACHABLE("Should be unreachable");
                break;
            }

            case BinaryOperatorKind::Div: {
                llvm::Value* lhs = gen_expr(bin.lhs);
                llvm::Value* rhs = gen_expr(bin.rhs);

                if (expr->type->is_integral()) {
                    llvm::Value* cond = m_active_module_context.builder->CreateICmpNE(rhs, get_int(0, bin.lhs->type));
                    call_assert(cond, expr->loc.line, "Division by zero",
                        { get_sz(6) }, { TypeInfo::get_sz() });

                    if (expr->type->is_signed()) {
                        return m_active_module_context.builder->CreateSDiv(lhs, rhs, "div");
                    } else {
                        return m_active_module_context.builder->CreateUDiv(lhs, rhs, "div");
                    }
                } else if (expr->type->is_floating_point()) {
                    return m_active_module_context.builder->CreateFDiv(lhs, rhs, "fdiv");
                }

                ARIA_UNREACHABLE("Should be unreachable");
                break;
            }

            case BinaryOperatorKind::Mod: {
                llvm::Value* lhs = gen_expr(bin.lhs);
                llvm::Value* rhs = gen_expr(bin.rhs);

                if (expr->type->is_integral()) {
                    if (expr->type->is_signed()) {
                        return m_active_module_context.builder->CreateSRem(lhs, rhs, "rem");
                    } else {
                        return m_active_module_context.builder->CreateURem(lhs, rhs, "rem");
                    }
                } else if (expr->type->is_floating_point()) {
                    return m_active_module_context.builder->CreateFRem(lhs, rhs, "frem");
                }

                ARIA_UNREACHABLE("Should be unreachable");
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
                } else if (bin.lhs->type->is_floating_point()) {
                    return m_active_module_context.builder->CreateFCmpULT(lhs, rhs, "flt");
                }

                ARIA_UNREACHABLE("Should be unreachable");
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
                } else if (bin.lhs->type->is_floating_point()) {
                    return m_active_module_context.builder->CreateFCmpUGT(lhs, rhs, "fgt");
                }

                ARIA_UNREACHABLE("Should be unreachable");
                break;
            }

            case BinaryOperatorKind::GreaterOrEq: {
                llvm::Value* lhs = gen_expr(bin.lhs);
                llvm::Value* rhs = gen_expr(bin.rhs);

                if (bin.lhs->type->is_integral()) {
                    if (bin.lhs->type->is_signed()) {
                        return m_active_module_context.builder->CreateICmpSGE(lhs, rhs, "ge");
                    } else {
                        return m_active_module_context.builder->CreateICmpUGE(lhs, rhs, "ge");
                    }
                }

                ARIA_UNREACHABLE("Should be unreachable");
                break;
            }

            case BinaryOperatorKind::BitAnd: {
                llvm::Value* lhs = gen_expr(bin.lhs);
                llvm::Value* rhs = gen_expr(bin.rhs);

                m_active_module_context.builder->CreateAnd(lhs, rhs, "and");
                break;
            }

            case BinaryOperatorKind::LogOr: {
                llvm::Value* lhs = gen_expr(bin.lhs);

                llvm::BasicBlock* prev_block = m_active_module_context.builder->GetInsertBlock();
                llvm::BasicBlock* phi_block = llvm::BasicBlock::Create(*m_active_module_context.context, "logor.phi", m_active_module_context.function);
                llvm::BasicBlock* rhs_block = llvm::BasicBlock::Create(*m_active_module_context.context, "logor.rhs", m_active_module_context.function);

                ARIA_ASSERT(lhs->getType()->isIntegerTy(1), "Not a boolean type");
                m_active_module_context.builder->CreateCondBr(lhs, phi_block, rhs_block);

                m_active_module_context.builder->SetInsertPoint(rhs_block);
                llvm::Value* rhs = gen_expr(bin.rhs);
                ARIA_ASSERT(rhs->getType()->isIntegerTy(1), "Not a boolean type");
                m_active_module_context.builder->CreateBr(phi_block);

                m_active_module_context.builder->SetInsertPoint(phi_block);
                
                llvm::PHINode* phi = m_active_module_context.builder->CreatePHI(llvm::Type::getInt1Ty(*m_active_module_context.context), 2);
                phi->addIncoming(lhs, prev_block);
                phi->addIncoming(rhs, rhs_block);

                return phi;
            }

            case BinaryOperatorKind::Eq: {
                llvm::Value* lhs = gen_expr(bin.lhs);
                llvm::Value* rhs = gen_expr(bin.rhs);

                return m_active_module_context.builder->CreateStore(rhs, lhs);
            }

            case BinaryOperatorKind::IsEq: {
                llvm::Value* lhs = gen_expr(bin.lhs);
                llvm::Value* rhs = gen_expr(bin.rhs);

                return m_active_module_context.builder->CreateICmpEQ(rhs, lhs);
            }

            case BinaryOperatorKind::IsNotEq: {
                llvm::Value* lhs = gen_expr(bin.lhs);
                llvm::Value* rhs = gen_expr(bin.rhs);

                return m_active_module_context.builder->CreateICmpNE(rhs, lhs);
            }

            default: ARIA_UNREACHABLE("Invalid binary operator"); return nullptr;
        }
    }

    llvm::Value* Codegen::gen_compound_assign_expr(Expr* expr) {
        CompoundAssignExpr& comp = expr->compound_assign;
        set_debug_loc(expr->loc);

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

                ARIA_UNREACHABLE("Should be unreachable");
                break;
            }

            case BinaryOperatorKind::CompoundSub: {
                if (expr->type->is_integral()) {
                    llvm::Value* sub = m_active_module_context.builder->CreateSub(lhs_val, rhs, "sub");
                    m_active_module_context.builder->CreateStore(sub, lhs);
                    return lhs;
                } else if (expr->type->is_floating_point()) {
                    llvm::Value* sub = m_active_module_context.builder->CreateFSub(lhs_val, rhs, "fsub");
                    m_active_module_context.builder->CreateStore(sub, lhs);
                    return lhs;
                }

                ARIA_UNREACHABLE("Should be unreachable");
                break;
            }

            case BinaryOperatorKind::CompoundMul: {
                if (expr->type->is_integral()) {
                    llvm::Value* mul = m_active_module_context.builder->CreateMul(lhs_val, rhs, "mul");
                    m_active_module_context.builder->CreateStore(mul, lhs);
                    return lhs;
                } else if (expr->type->is_floating_point()) {
                    llvm::Value* mul = m_active_module_context.builder->CreateFMul(lhs_val, rhs, "fmul");
                    m_active_module_context.builder->CreateStore(mul, lhs);
                    return lhs;
                }

                ARIA_UNREACHABLE("Should be unreachable");
                break;
            }

            case BinaryOperatorKind::CompoundDiv: {
                if (expr->type->is_integral()) {
                    llvm::Value* div = nullptr;
                    if (expr->type->is_signed()) {
                        div = m_active_module_context.builder->CreateSDiv(lhs_val, rhs, "div");
                    } else {
                        div = m_active_module_context.builder->CreateUDiv(lhs_val, rhs, "div");
                    }

                    m_active_module_context.builder->CreateStore(div, lhs);
                    return lhs;
                } else if (expr->type->is_floating_point()) {
                    llvm::Value* div = m_active_module_context.builder->CreateFDiv(lhs_val, rhs, "fdiv");
                    m_active_module_context.builder->CreateStore(div, lhs);
                    return lhs;
                }

                ARIA_UNREACHABLE("Should be unreachable");
                break;
            }

            case BinaryOperatorKind::CompoundShr: {
                llvm::Value* shr = nullptr;

                if (expr->type->is_signed()) {
                    shr = m_active_module_context.builder->CreateAShr(lhs_val, rhs, "shr");
                } else {
                    shr = m_active_module_context.builder->CreateLShr(lhs_val, rhs, "shr");
                }

                m_active_module_context.builder->CreateStore(shr, lhs);
                return lhs;
            }

            default: ARIA_UNREACHABLE("Invalid compound binary operator");
        }
    }

    llvm::Value* Codegen::gen_const_expr(Expr* expr) {
        ConstExpr& c = expr->const_;
        set_debug_loc(expr->loc);

        switch (c.kind) {
            case ConstExprKind::Boolean: {
                if (c.boolean) { return llvm::ConstantInt::getTrue(llvm::Type::getInt1Ty(*m_active_module_context.context)); }
                else { return llvm::ConstantInt::getFalse(llvm::Type::getInt1Ty(*m_active_module_context.context)); }
            }

            case ConstExprKind::Integer: { return llvm::ConstantInt::getIntegerValue(
                llvm::Type::getIntNTy(*m_active_module_context.context, static_cast<unsigned>(expr->type->get_bit_size())),
                llvm::APInt((unsigned)expr->type->get_bit_size(), c.integer));
            }

            case ConstExprKind::Floating: {
                if (expr->type->kind == TypeKind::Float) {
                    return llvm::ConstantFP::get(*m_active_module_context.context, llvm::APFloat(static_cast<float>(c.number)));
                } else {
                    return llvm::ConstantFP::get(*m_active_module_context.context, llvm::APFloat(c.number));
                }
            }

            case ConstExprKind::String: {
                return get_string(c.string);
            }

            case ConstExprKind::Struct: {
                std::vector<llvm::Constant*> vals;

                for (Expr* val : c.values) {
                    vals.push_back(llvm::dyn_cast<llvm::Constant>(gen_expr(val)));
                }

                return llvm::ConstantStruct::get(llvm::dyn_cast<llvm::StructType>(type_info_to_llvm_type(expr->type)), vals);
            }

            case ConstExprKind::Typeid: {
                return get_typeid(c.type);
            }

            default: ARIA_UNREACHABLE("Invalid const expr kind");
        }

        return nullptr;
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
            case ExprKind::TypeInfo: return gen_typeinfo_expr(expr);
            case ExprKind::Member: return gen_member_expr(expr);
            case ExprKind::BuiltinMember: return gen_builtin_member_expr(expr);
            case ExprKind::TypeMember: return gen_type_member_expr(expr);
            case ExprKind::Self: return gen_self_expr(expr);
            case ExprKind::Call: return gen_call_expr(expr);
            case ExprKind::BuiltinCall: return gen_builtin_call_expr(expr);
            case ExprKind::Construct: return gen_construct_expr(expr);
            case ExprKind::ArrayLiteral: return gen_array_literal_expr(expr);
            case ExprKind::MethodCall: return gen_method_call_expr(expr);
            case ExprKind::ArraySubscript: return gen_array_subscript_expr(expr);
            case ExprKind::ToSlice: return gen_to_slice_expr(expr);
            case ExprKind::Move: return gen_move_expr(expr);
            case ExprKind::Temporary: return gen_temporary_expr(expr);
            case ExprKind::MaterializeTemporary: return gen_materialize_temporary_expr(expr);
            case ExprKind::ExprWithCleanups: return gen_expr_with_cleanups(expr);
            case ExprKind::Paren: return gen_paren_expr(expr);
            case ExprKind::Ternary: return gen_ternary_expr(expr);
            case ExprKind::ImplicitCast: return gen_implicit_cast_expr(expr);
            case ExprKind::Cast: return gen_cast_expr(expr);
            case ExprKind::UnaryOperator: return gen_unary_operator_expr(expr);
            case ExprKind::BinaryOperator: return gen_binary_operator_expr(expr);
            case ExprKind::CompoundAssign: return gen_compound_assign_expr(expr);
            case ExprKind::Const: return gen_const_expr(expr);

            default: ARIA_UNREACHABLE("Invalid expr kind");
        }
    }

    llvm::Value* Codegen::gen_init_expr(Expr* expr, llvm::Value* dst) {
        llvm::Value* val = gen_expr(expr);
        set_debug_loc(expr->loc);

        if (expr->type->is_array() && expr->value_kind == ExprValueKind::LValue) {
            // Call memcpy since we copy arrays
            llvm::Value* size = m_active_module_context.builder->getInt64(expr->type->array.size);
            return m_active_module_context.builder->CreateMemCpy(dst, llvm::MaybeAlign::MaybeAlign(), val, llvm::MaybeAlign::MaybeAlign(), size);
        } else {
            return m_active_module_context.builder->CreateStore(val, dst);
        }
    }

    llvm::Value* Codegen::gen_cond(Expr* expr) {
        llvm::Value* val = gen_expr(expr);
        if (val->getType()->isIntegerTy(8)) {
            val = m_active_module_context.builder->CreateTrunc(val, m_active_module_context.builder->getInt1Ty(), "trunc");
        }

        return val;
    }

} // namespace ariac