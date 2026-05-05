#include "ariac/emitter/emitter.hpp"

#define ADD_STR(str) (m_op_codes.string_table.push_back(str))
#define ADD_CONSTANT(consta) (m_op_codes.constant_table.push_back(consta))

#define STR_IDX(idx) (static_cast<u16>(m_op_codes.string_table.size() + idx))
#define CONST_IDX(idx) (static_cast<u16>(m_op_codes.constant_table.size() + idx))

#define PUSH_OP(op) do { \
    static_assert(std::is_same_v<decltype(op), OpCode>, "Invalid op"); \
    m_op_codes.program.push_back(op); \
} while (0)
#define PUSH_U16(i) do { \
    u16 r = i; \
    static_assert(sizeof(i) == 2, "Invalid size"); \
    m_op_codes.program.push_back(static_cast<OpCode>(r >> 8 & 0xff)); \
    m_op_codes.program.push_back(static_cast<OpCode>(r & 0xff)); \
} while(0) 

#define PUSH_PENDING_OP(op) do { \
    static_assert(std::is_same_v<decltype(op), OpCode>, "Invalid op"); \
    m_pending_op_codes.push_back(op); \
} while (0)
#define PUSH_PENDING_U16(i) do { \
    static_assert(sizeof(i) == 2, "Invalid size"); \
    m_pending_op_codes.push_back(static_cast<OpCode>(i >> 8 & 0xff)); \
    m_pending_op_codes.push_back(static_cast<OpCode>(i & 0xff)); \
} while(0) 

namespace Aria::Internal {

    Emitter::Emitter(CompilationContext* ctx) {
        m_context = ctx;

        emit_impl();
    }

    void Emitter::emit_impl() {
        add_basic_types();
        add_user_defined_types();
        emit_declarations();
        emit_start_end();

        m_context->ops = m_op_codes;
        m_context->reflection_data = m_reflection_data;
    }

    void Emitter::add_basic_types() {
        #define ADD_TYPE(vmType, primType) do { m_op_codes.type_table.emplace_back(VMTypeKind::vmType); m_basic_types[TypeKind::primType] = static_cast<u16>(i); i++; } while (0)
        #define ADD_TYPE_STRUCT(vmType, primType, struc) do { m_op_codes.type_table.emplace_back(VMTypeKind::vmType, struc); m_basic_types[TypeKind::primType] = static_cast<u16>(i); i++; } while (0)

        size_t i = 0;
        ADD_TYPE(Void, Void);
        ADD_TYPE(I1, Bool);
        ADD_TYPE(I8, Char);
        ADD_TYPE(U8, UChar);
        ADD_TYPE(I16, Short);
        ADD_TYPE(U16, UShort);
        ADD_TYPE(I32, Int);
        ADD_TYPE(U32, UInt);
        ADD_TYPE(I64, Long);
        ADD_TYPE(U64, ULong);
        ADD_TYPE(Float, Float);
        ADD_TYPE(Double, Double);
        ADD_TYPE(Ptr, Ptr);
        ADD_TYPE_STRUCT(Slice, Slice, VMStruct("<builtin_slice>", std::vector{static_cast<size_t>(m_basic_types[TypeKind::Ptr]), static_cast<size_t>(m_basic_types[TypeKind::ULong])}));

        #undef ADD_TYPE
    }

    void Emitter::add_user_defined_types() {
        m_struct_index = static_cast<u16>(m_basic_types.size());

        for (Module* mod : m_context->modules) {
            if (mod->units.size() == 0) { continue; }
            m_active_namespace = mod->name;

            for (CompilationUnit* unit : mod->units) {
                for (Decl* s : unit->structs) {
                    emit_decl(s);
                }
            }
        }
    }

    void Emitter::emit_declarations() {
        for (Module* mod : m_context->modules) {
            if (mod->units.size() == 0) { continue; }
            m_active_namespace = mod->name;

            for (CompilationUnit* unit : mod->units) {
                for (Decl* f : unit->funcs) {
                    emit_decl(f);
                }
            }

            // Global variable initializion requires special functions
            std::string startSig = fmt::format("_start${}()",mod->name);
            std::string endSig = fmt::format("_end${}()", mod->name);

            ADD_STR(startSig);
            ADD_STR("_entry$");
            PUSH_OP(OP_FUNCTION);
            PUSH_U16(STR_IDX(-2));
            PUSH_OP(OP_LABEL);
            PUSH_U16(STR_IDX(-1));
            push_stack_frame(startSig);

            for (CompilationUnit* unit : mod->units) {
                for (Decl* g : unit->globals) {
                    emit_decl(g);
                }
            }

            merge_pending_op_codes();
            pop_stack_frame();
            PUSH_OP(OP_RET);
            PUSH_OP(OP_ENDFUNCTION);

            // Global variable destruction
            ADD_STR(endSig);
            ADD_STR("_entry$");
            PUSH_OP(OP_FUNCTION);
            PUSH_U16(STR_IDX(-2));
            PUSH_OP(OP_LABEL);
            PUSH_U16(STR_IDX(-1));
            push_stack_frame(endSig);

            emit_destructors(m_global_scope.declared_symbols);
            merge_pending_op_codes();

            pop_stack_frame();
            PUSH_OP(OP_RET);
            PUSH_OP(OP_ENDFUNCTION);

            mod->ops = m_op_codes;
            mod->reflection_data = m_reflection_data;
        }
    }

    void Emitter::emit_start_end() {
        ADD_STR("_start$()");
        ADD_STR("_entry$");
        PUSH_OP(OP_FUNCTION);
        PUSH_U16(STR_IDX(-2));
        PUSH_OP(OP_LABEL);
        PUSH_U16(STR_IDX(-1));
        push_stack_frame("_start$()");

        for (Module* mod : m_context->modules) {
            if (mod->units.size() == 0) { continue; }
            ADD_STR(fmt::format("_start${}()", mod->name));
            PUSH_OP(OP_CALL);
            PUSH_U16(STR_IDX(-1));
        }

        pop_stack_frame();
        PUSH_OP(OP_RET);
        PUSH_OP(OP_ENDFUNCTION);

        ADD_STR("_end$()");
        ADD_STR("_entry$");
        PUSH_OP(OP_FUNCTION);
        PUSH_U16(STR_IDX(-2));
        PUSH_OP(OP_LABEL);
        PUSH_U16(STR_IDX(-1));
        push_stack_frame("_end$()");

        for (Module* mod : m_context->modules) {
            if (mod->units.size() == 0) { continue; }
            ADD_STR(fmt::format("_end${}()", mod->name));
            PUSH_OP(OP_CALL);
            PUSH_U16(STR_IDX(-1));
        }

        pop_stack_frame();
        PUSH_OP(OP_RET);
        PUSH_OP(OP_ENDFUNCTION);
    }

    void Emitter::emit_boolean_literal_expr(Expr* expr, ExprValueKind value_kind) {
        BooleanLiteralExpr bc = expr->boolean_literal;
        ADD_CONSTANT(static_cast<u64>(bc.value));
        PUSH_PENDING_OP(OP_LD_CONST);
        PUSH_PENDING_U16(type_info_to_vm_type_idx(expr->type));
        PUSH_PENDING_U16(CONST_IDX(-1));
    }

    void Emitter::emit_character_literal_expr(Expr* expr, ExprValueKind value_kind) {
        CharacterLiteralExpr cc = expr->character_literal;
        ADD_CONSTANT(static_cast<u64>(cc.value));
        PUSH_PENDING_OP(OP_LD_CONST);
        PUSH_PENDING_U16(type_info_to_vm_type_idx(expr->type));
        PUSH_PENDING_U16(CONST_IDX(-1));
    }

    void Emitter::emit_integer_literal_expr(Expr* expr, ExprValueKind value_kind) {
        IntegerLiteralExpr ic = expr->integer_literal;
        ADD_CONSTANT(static_cast<u64>(ic.value));
        PUSH_PENDING_OP(OP_LD_CONST);
        PUSH_PENDING_U16(type_info_to_vm_type_idx(expr->type));
        PUSH_PENDING_U16(CONST_IDX(-1));
    }

    void Emitter::emit_floating_literal_expr(Expr* expr, ExprValueKind value_kind) {
        FloatingLiteralExpr fc = expr->floating_literal;
        PUSH_PENDING_OP(OP_LD_CONST);
        PUSH_PENDING_U16(type_info_to_vm_type_idx(expr->type));

        switch (expr->type->kind) {
            case TypeKind::Float: ADD_CONSTANT(static_cast<float>(fc.value)); break;
            case TypeKind::Double: ADD_CONSTANT(static_cast<double>(fc.value)); break;

            default: ARIA_UNREACHABLE();
        }
        
        PUSH_PENDING_U16(CONST_IDX(-1));
    }

    void Emitter::emit_string_literal_expr(Expr* expr, ExprValueKind value_kind) {
        StringLiteralExpr sc = expr->string_literal;
        ADD_STR(fmt::format("{}", sc.value));
        PUSH_PENDING_OP(OP_LD_STR);
        PUSH_PENDING_U16(STR_IDX(-1));
    }

    void Emitter::emit_null_expr(Expr* expr, ExprValueKind value_kind) {
        PUSH_PENDING_OP(OP_LD_NULL);
    }

    void Emitter::emit_decl_ref_expr(Expr* expr, ExprValueKind value_kind) {
        DeclRefExpr declRef = expr->decl_ref;
        std::string ident;

        if (declRef.name_specifier) {
            ident = fmt::format("{}::{}", declRef.name_specifier->scope.identifier, declRef.identifier);
        } else {
            ident = fmt::format("{}::{}", m_active_namespace, declRef.identifier);
        }

        // VVV -Var- VVV //
        if (declRef.referenced_decl->kind == DeclKind::Var) {
            // Check for global variables
            if (declRef.referenced_decl->var.global_var) {
                ADD_STR(ident);

                if (value_kind == ExprValueKind::LValue) {
                    if (expr->type->is_reference()) {
                        PUSH_PENDING_OP(OP_LD_GLOBAL);
                    } else {
                        PUSH_PENDING_OP(OP_LD_PTR_GLOBAL);
                    }

                    PUSH_PENDING_U16(STR_IDX(-1));
                } else if (value_kind == ExprValueKind::RValue) {
                    PUSH_PENDING_OP(OP_LD_GLOBAL);
                    PUSH_PENDING_U16(STR_IDX(-1));
        
                    if (expr->type->is_reference()) {
                        ARIA_TODO("Dereferencing references");
                    }
                }

                return;
            }

            for (size_t i = m_active_stack_frame.scopes.size(); i > 0; i--) {
                Scope& s = m_active_stack_frame.scopes[i - 1];
                if (s.declared_symbol_map.contains(ident)) {
                    Declaration& decl = s.declared_symbols[s.declared_symbol_map.at(ident)];
                    
                    if (value_kind == ExprValueKind::LValue) {
                        if (expr->type->is_reference()) {
                            PUSH_PENDING_OP(OP_LD_LOCAL);
                        } else {
                            PUSH_PENDING_OP(OP_LD_PTR_LOCAL);
                        }

                        PUSH_PENDING_U16(static_cast<u16>(std::get<size_t>(decl.data)));
                    } else if (value_kind == ExprValueKind::RValue) {
                        PUSH_PENDING_OP(OP_LD_LOCAL);
                        PUSH_PENDING_U16(static_cast<u16>(std::get<size_t>(decl.data)));
        
                        if (expr->type->is_reference()) {
                            ARIA_TODO("Dereferencing references");
                        }
                    }
                }
            }

            return;
        }
        // ^^^ -Var- ^^^ //
        
        // VVV -Param- VVV //
        if (declRef.referenced_decl->kind == DeclKind::Param) {
            if (value_kind == ExprValueKind::LValue) {
                if (expr->type->is_reference()) {
                    PUSH_PENDING_OP(OP_LD_LOCAL);
                } else {
                    PUSH_PENDING_OP(OP_LD_PTR_LOCAL);
                }

                PUSH_PENDING_U16(static_cast<u16>(m_active_stack_frame.parameters.at(ident)));
            } else if (value_kind == ExprValueKind::RValue) {
                PUSH_PENDING_OP(OP_LD_LOCAL);
                PUSH_PENDING_U16(static_cast<u16>(m_active_stack_frame.parameters.at(ident)));
        
                if (expr->type->is_reference()) {
                    ARIA_TODO("Dereferencing references");
                }
            }

            return;
        }
        // ^^^ -Param- ^^^ //
        
        // VVV -Function- VVV //
        if (declRef.referenced_decl->kind == DeclKind::Function) {
            return;
        }
        // ^^^ -Function- ^^^ //

        ARIA_UNREACHABLE();
    }

    void Emitter::emit_member_expr(Expr* expr, ExprValueKind value_kind) {
        // MemberExpr* mem = GetNode<MemberExpr>(expr);
        // 
        // StructDeclaration& sd = std::get<StructDeclaration>(mem->GetParentType()->Data);
        // StructDecl* s = GetNode<StructDecl>(sd.SourceDecl);
        // 
        // for (Decl* field : s->GetFields()) {
        //     if (FieldDecl* fd = GetNode<FieldDecl>(field)) {
        //         if (fd->GetRawIdentifier() == mem->GetMember()) {
        //             RuntimeStructDeclaration& sdecl = m_Structs.at(s->GetIdentifier());
        // 
        //             emit_expr(mem->GetParent(), ExprValueKind::LValue);
        // 
        //             if (value_kind == ExprValueKind::LValue) {
        //                 PUSH_PENDING_OP(OpCodeKind::LdPtrMember, OpCodeMember(sdecl.FieldIndices.at(fd->GetIdentifier()), {VMTypeKind::Ptr}, TypeInfoToVMType(mem->GetParentType())));
        //             } else if (value_kind == ExprValueKind::RValue) {
        //                 PUSH_PENDING_OP(OpCodeKind::LdMember, OpCodeMember(sdecl.FieldIndices.at(fd->GetIdentifier()), TypeInfoToVMType(fd->GetResolvedType()), TypeInfoToVMType(mem->GetParentType())));
        //             }
        //         }
        //     } else if (MethodDecl* md = GetNode<MethodDecl>(field)) {
        //         if (md->GetRawIdentifier() == mem->GetMember()) {
        //             PUSH_PENDING_OP(OpCodeKind::LdFunc, fmt::format("{}::{}()", sd.Identifier, md->GetIdentifier()));
        //         }
        //     }  
        // }

        ARIA_ASSERT(false, "todo!");
    }

    void Emitter::emit_builtin_member_expr(Expr* expr, ExprValueKind value_kind) {
        MemberExpr mem = expr->member;

        emit_expr(mem.parent, mem.parent->value_kind);

        switch (mem.parent->type->kind) {
            case TypeKind::Slice: {
                size_t idx = 0;

                if (mem.member == "mem") { idx = 0; }
                else if (mem.member == "len") { idx = 1; }

                if (value_kind == ExprValueKind::LValue) {
                    PUSH_PENDING_OP(OP_LD_PTR_FIELD);
                    PUSH_PENDING_U16(static_cast<u16>(idx));
                    PUSH_PENDING_U16(type_info_to_vm_type_idx(mem.parent->type));
                } else {
                    PUSH_PENDING_OP(OP_LD_FIELD);
                    PUSH_PENDING_U16(static_cast<u16>(idx));
                    PUSH_PENDING_U16(type_info_to_vm_type_idx(mem.parent->type));
                }

                break;
            }

            default: ARIA_UNREACHABLE();
        }
    }

    void Emitter::emit_self_expr(Expr* expr, ExprValueKind value_kind) {
        ARIA_TODO("Emitter::emit_self_expr()");
    }

    void Emitter::emit_temporary_expr(Expr* expr, ExprValueKind value_kind) {
        TemporaryExpr& temp = expr->temporary;

        u16 idx = static_cast<u16>(m_active_stack_frame.local_count);

        // Create a new temporary
        PUSH_OP(OP_ALLOCA);
        PUSH_U16(type_info_to_vm_type_idx(expr->type));
        PUSH_OP(OP_DECL_LOCAL);
        PUSH_U16(idx);

        emit_expr(temp.expression, value_kind);
        PUSH_PENDING_OP(OP_ST_LOCAL);
        PUSH_PENDING_U16(idx);
        
        Declaration d;
        d.type = temp.expression->type;
        d.data = m_active_stack_frame.local_count;
        d.destructor = temp.destructor;
        m_active_stack_frame.local_count++;
        
        m_temporaries.push_back(d);

        PUSH_PENDING_OP(OP_LD_LOCAL);
        PUSH_PENDING_U16(idx);
    }

    void Emitter::emit_copy_expr(Expr* expr, ExprValueKind value_kind) {
        CopyExpr copy = expr->copy;
        
        if (copy.constructor->kind == DeclKind::BuiltinCopyConstructor) {
            switch (copy.constructor->built_in_copy_constructor.kind) {
                case BuiltinKind::String: ADD_STR("__aria_copy_str()"); break;
                default: ARIA_UNREACHABLE();
            }
        }

        u16 idx = STR_IDX(-1);

        emit_expr(copy.expression, ExprValueKind::LValue);
        PUSH_PENDING_OP(OP_CALL);
        PUSH_PENDING_U16(idx);
    }

    void Emitter::emit_call_expr(Expr* expr, ExprValueKind value_kind) {
        CallExpr call = expr->call;
        
        ARIA_ASSERT(call.callee->kind == ExprKind::DeclRef, "Callee of call expression must be a decl-ref");

        for (auto it = call.arguments.rbegin(); it != call.arguments.rend(); it--) {
            emit_expr(*it, (*it)->value_kind);
        }

        std::string ident;
        for (auto& attr : call.callee->decl_ref.referenced_decl->function.attributes) {
            if (attr.kind == FunctionDecl::AttributeKind::Extern) { ident = fmt::format("{}", attr.arg); }
            else if (attr.kind == FunctionDecl::AttributeKind::NoMangle) { ident = fmt::format("{}()", call.callee->decl_ref.identifier); }
        }

        if (ident.empty()) {
            if (call.callee->decl_ref.name_specifier) {
                ident = fmt::format("{}::{}", call.callee->decl_ref.name_specifier->scope.identifier, mangle_function(&call.callee->decl_ref.referenced_decl->function));
            } else {
                ident = fmt::format("{}::{}", m_active_namespace, mangle_function(&call.callee->decl_ref.referenced_decl->function));
            }
        }

        ADD_STR(ident);
        PUSH_PENDING_OP(OP_CALL);
        PUSH_PENDING_U16(STR_IDX(-1));
    }

    void Emitter::emit_construct_expr(Expr* expr, ExprValueKind value_kind) {
        ARIA_TODO("Emitter::emit_construct_expr()");
        // ConstructExpr& ct = expr->Construct;
        // 
        // PUSH_OP(OpCodeKind::Alloca, { type_info_to_vm_type_idx(expr->type) });
        // PUSH_OP(OpCodeKind::DeclareLocal, { m_ActiveStackFrame.LocalCount });
        // ADD_STR(fmt::format("{}::<ctor>()", type_info_to_string(expr->type)));
        // PUSH_PENDING_OP(OpCodeKind::LdFunc, { STR_IDX(-1) });
        // PUSH_PENDING_OP(OpCodeKind::LdPtrLocal, { m_ActiveStackFrame.LocalCount });
        // PUSH_PENDING_OP(OpCodeKind::DeclareArg, { static_cast<size_t>(0) });
        // PUSH_PENDING_OP(OpCodeKind::Call, { static_cast<size_t>(1), static_cast<size_t>(0) });
        // PUSH_PENDING_OP(OpCodeKind::LdLocal, { m_ActiveStackFrame.LocalCount });
        // 
        // Declaration d;
        // d.Type = expr->type;
        // d.Data = m_ActiveStackFrame.LocalCount;
        // d.Destructor = nullptr;
        // m_ActiveStackFrame.LocalCount++;
        // 
        // m_ActiveStackFrame.Scopes.back().DeclaredSymbols.push_back(d);
    }

    void Emitter::emit_array_subscript_expr(Expr* expr, ExprValueKind value_kind) {
        ArraySubscriptExpr subs = expr->array_subscript;

        switch (subs.array->type->kind) {
            case TypeKind::Ptr: {
                emit_expr(subs.array, subs.array->value_kind);
                emit_expr(subs.index, subs.index->value_kind);
                PUSH_PENDING_OP(OP_OFFP);
                PUSH_PENDING_U16(type_info_to_vm_type_idx(subs.array->type->base));

                if (value_kind == ExprValueKind::RValue) {
                    PUSH_PENDING_OP(OP_LD);
                    PUSH_PENDING_U16(type_info_to_vm_type_idx(subs.array->type->base));
                }

                break;
            }

            case TypeKind::Slice: {
                emit_expr(subs.array, subs.array->value_kind);
                PUSH_PENDING_OP(OP_LD_FIELD);
                PUSH_PENDING_U16(static_cast<u16>(0));
                PUSH_PENDING_U16(type_info_to_vm_type_idx(subs.array->type));

                emit_expr(subs.index, subs.index->value_kind);
                PUSH_PENDING_OP(OP_OFFP);
                PUSH_PENDING_U16(type_info_to_vm_type_idx(subs.array->type->base));

                if (value_kind == ExprValueKind::RValue) {
                    PUSH_PENDING_OP(OP_LD);
                    PUSH_PENDING_U16(type_info_to_vm_type_idx(subs.array->type->base));
                }

                break;
            }

            default: ARIA_UNREACHABLE();
        }
    }

    void Emitter::emit_to_slice_expr(Expr* expr, ExprValueKind value_kind) {
        ToSliceExpr tos = expr->to_slice;

        PUSH_PENDING_OP(OP_ALLOCA);
        PUSH_PENDING_U16(type_info_to_vm_type_idx(expr->type));

        switch (tos.source->type->kind) {
            case TypeKind::Ptr: {
                PUSH_PENDING_OP(OP_LD_PTR); // Load the pointer to the slice
                PUSH_PENDING_OP(OP_DUP);
                emit_expr(tos.source, tos.source->value_kind);
                PUSH_PENDING_OP(OP_ST_FIELD);
                PUSH_PENDING_U16(static_cast<u16>(0)); // .mem is the first field
                PUSH_PENDING_U16(type_info_to_vm_type_idx(expr->type));
                emit_expr(tos.len, tos.len->value_kind);
                PUSH_PENDING_OP(OP_ST_FIELD);
                PUSH_PENDING_U16(static_cast<u16>(1)); // .len is the second field
                PUSH_PENDING_U16(type_info_to_vm_type_idx(expr->type));
                break;
            }

            case TypeKind::Slice: {
                ARIA_TODO("slice to slice");
            }

            case TypeKind::Array: {
                ARIA_TODO("array to slice");
            }

            default: ARIA_UNREACHABLE();
        }
    }

    void Emitter::emit_new_expr(Expr* expr, ExprValueKind value_kind) {
        NewExpr n = expr->new_;

        if (n.array) {
            emit_expr(n.initializer, n.initializer->value_kind);
            PUSH_PENDING_OP(OP_NEW_ARR);
            PUSH_PENDING_U16(type_info_to_vm_type_idx(expr->type->base));
        } else {
            PUSH_PENDING_OP(OP_NEW);
            PUSH_PENDING_U16(type_info_to_vm_type_idx(expr->type->base));

            if (n.initializer) {
                PUSH_PENDING_OP(OP_DUP);
                emit_expr(n.initializer, n.initializer->value_kind);
                PUSH_PENDING_OP(OP_ST_ADDR);
            }
        }
    }

    void Emitter::emit_delete_expr(Expr* expr, ExprValueKind value_kind) {
        DeleteExpr n = expr->delete_;

        emit_expr(n.expression, n.expression->value_kind);
        PUSH_PENDING_OP(OP_FREE);
    }

    void Emitter::emit_format_expr(Expr* expr, ExprValueKind value_kind) {
        ARIA_TODO("Emitter::emit_format_expr()");
        // FormatExpr format = expr->Format;
        // std::string_view fStr = format.Args.Items[0]->Temporary.Expression->StringConstant.Value;
        // 
        // PUSH_OP(OP_ALLOCA);
        // PUSH_OP(type_info_to_vm_type_idx(expr->type));
        // PUSH_OP(OP_DECL_LOCAL);
        // PUSH_OP(static_cast<OpCode>(m_ActiveStackFrame.LocalCount));
        // size_t idx = m_ActiveStackFrame.LocalCount++;
        // 
        // for (auto& arg : format.ResolvedArgs) {
        //     emit_expr(arg.Arg, arg.Arg->ValueKind);
        //     ADD_STR(fmt::format("__aria_append_str<{}>()", type_info_to_string(arg.Arg->type)));
        //     PUSH_PENDING_OP(OP_LD_PTR_LOCAL);
        //     PUSH_PENDING_OP(static_cast<OpCode>(idx));
        //     PUSH_PENDING_OP(OP_CALL);
        //     PUSH_PENDING_OP(STR_IDX(-1));
        // }
        // 
        // PUSH_PENDING_OP(OP_LD_LOCAL);
        // PUSH_PENDING_OP(static_cast<OpCode>(idx));
    }

    void Emitter::emit_paren_expr(Expr* expr, ExprValueKind value_kind) {
        ParenExpr paren = expr->paren;
        emit_expr(paren.expression, expr->value_kind);
    }

    void Emitter::emit_implicit_cast_expr(Expr* expr, ExprValueKind value_kind) {
        ImplicitCastExpr cast = expr->implicit_cast;
        
        if (cast.kind == CastKind::LValueToRValue) {
            return emit_expr(cast.expression, ExprValueKind::RValue);
        } else {
            emit_expr(cast.expression, cast.expression->value_kind);

            switch (cast.kind) {
                case CastKind::Integral: {
                    PUSH_PENDING_OP(OP_CONV_ITOI);
                    PUSH_PENDING_U16(type_info_to_vm_type_idx(expr->type));
                    break;
                }

                case CastKind::Floating: {
                    PUSH_PENDING_OP(OP_CONV_FTOF);
                    PUSH_PENDING_U16(type_info_to_vm_type_idx(expr->type));
                    break;
                }

                case CastKind::IntegralToFloating: {
                    PUSH_PENDING_OP(OP_CONV_ITOF);
                    PUSH_PENDING_U16(type_info_to_vm_type_idx(expr->type));
                    break;
                }

                case CastKind::FloatingToIntegral: {
                    PUSH_PENDING_OP(OP_CONV_FTOI);
                    PUSH_PENDING_U16(type_info_to_vm_type_idx(expr->type));
                    break;
                }

                case CastKind::BitCast: {
                    break;
                }

                case CastKind::ArrayToSlice: {
                    break;
                }

                default: ARIA_UNREACHABLE(); break;
            }

            return;
        }
        
        ARIA_UNREACHABLE();
    }

    void Emitter::emit_cast_expr(Expr* expr, ExprValueKind value_kind) {
        CastExpr cast = expr->cast;
        emit_expr(cast.expression, cast.expression->value_kind);
    }

    void Emitter::emit_unary_operator_expr(Expr* expr, ExprValueKind value_kind) {
        UnaryOperatorExpr unop = expr->unary_operator;
        
        switch (unop.op) {
            case UnaryOperatorKind::Negate: { 
                emit_expr(unop.expression, unop.expression->value_kind);
                if (expr->type->is_integral()) {
                    ARIA_ASSERT(expr->type->is_signed(), "Type must be signed");
                    PUSH_PENDING_OP(OP_NEGI);
                } else if (expr->type->is_floating_point()) {
                    PUSH_PENDING_OP(OP_NEGF);
                } else {
                    ARIA_UNREACHABLE();
                }

                break;
            }

            case UnaryOperatorKind::AddressOf: {
                emit_expr(unop.expression, ExprValueKind::LValue);
                break;
            }

            case UnaryOperatorKind::Dereference: {
                emit_expr(unop.expression, unop.expression->value_kind);

                if (value_kind == ExprValueKind::RValue) {
                    PUSH_PENDING_OP(OP_LD);
                    PUSH_PENDING_U16(type_info_to_vm_type_idx(expr->type));
                }
                
                break;
            }

            default: ARIA_UNREACHABLE();
        }
    }

    void Emitter::emit_binary_operator_expr(Expr* expr, ExprValueKind value_kind) {
        BinaryOperatorExpr binop = expr->binary_operator;
        
        #define BINOP2(_enum, op) case BinaryOperatorKind::_enum: { \
            emit_expr(binop.lhs, binop.lhs->value_kind); \
            emit_expr(binop.rhs, binop.rhs->value_kind); \
            \
            if (binop.lhs->type->is_integral()) { \
                if (binop.lhs->type->is_signed()) { PUSH_PENDING_OP(OP_##op##I); } \
                if (binop.lhs->type->is_unsigned()) { PUSH_PENDING_OP(OP_##op##U); } \
            } else if (binop.lhs->type->is_floating_point()) { \
                PUSH_PENDING_OP(OP_##op##F); \
            } \
            break; \
        }

        #define BINOP(_enum, op) BINOP2(_enum, op)

        #define BINOP_INT2(_enum, op) case BinaryOperatorKind::_enum: { \
            emit_expr(binop.lhs, binop.lhs->value_kind); \
            emit_expr(binop.rhs, binop.rhs->value_kind); \
            \
            if (binop.lhs->type->is_integral()) { \
                if (binop.lhs->type->is_signed()) { PUSH_PENDING_OP(OP_##op##I); } \
                if (binop.lhs->type->is_unsigned()) { PUSH_PENDING_OP(OP_##op##U); } \
            } \
            break; \
        }

        #define BINOP_INT(_enum, op) BINOP_INT2(_enum, op)
        
        switch (binop.op) {
            BINOP(Add,         ADD)
            BINOP(Sub,         SUB)
            BINOP(Mul,         MUL)
            BINOP(Div,         DIV)
            BINOP(Mod,         MOD)

            BINOP_INT(BitAnd,      AND)
            BINOP_INT(BitOr,       OR)
            BINOP_INT(BitXor,      XOR)
            BINOP_INT(Shl,         SHL)
            BINOP_INT(Shr,         SHR)

            BINOP(Less,        LT)
            BINOP(LessOrEq,    LTE)
            BINOP(Greater,     GT)
            BINOP(GreaterOrEq, GTE)
            BINOP(IsEq,        CMP)
            BINOP(IsNotEq,     NCMP)
            
            case BinaryOperatorKind::LogAnd: {
                ADD_STR(fmt::format("logand.end_{}", m_and_counter));
                u16 idx = STR_IDX(-1);
                m_and_counter++;
        
                emit_expr(binop.lhs, binop.lhs->value_kind);
                PUSH_PENDING_OP(OP_JF);
                PUSH_PENDING_U16(idx);
                emit_expr(binop.rhs, binop.rhs->value_kind);
                PUSH_PENDING_OP(OP_LOGAND);
                PUSH_PENDING_OP(OP_JMP);
                PUSH_PENDING_U16(idx);
        
                PUSH_PENDING_OP(OP_LABEL);
                PUSH_PENDING_U16(idx);
        
                break;
            }
        
            case BinaryOperatorKind::LogOr: {
                ADD_STR(fmt::format("logor.end_{}", m_or_counter));
                u16 idx = STR_IDX(-1);
                m_or_counter++;
        
                emit_expr(binop.lhs, binop.lhs->value_kind);
                PUSH_PENDING_OP(OP_JT);
                PUSH_PENDING_U16(idx);
                emit_expr(binop.rhs, binop.rhs->value_kind);
                PUSH_PENDING_OP(OP_LOGOR);
                PUSH_PENDING_OP(OP_JMP);
                PUSH_PENDING_U16(idx);
        
                PUSH_PENDING_OP(OP_LABEL);
                PUSH_PENDING_U16(idx);
        
                break;
            }
        
            case BinaryOperatorKind::Eq: {
                emit_expr(binop.lhs, binop.lhs->value_kind);
                emit_expr(binop.rhs, binop.rhs->value_kind);
        
                PUSH_PENDING_OP(OP_ST_ADDR);
                emit_expr(binop.lhs, value_kind);
                break;
            }

            default: ARIA_UNREACHABLE();
        }

        #undef BINOP_INT
        #undef BINOP_INT2
        #undef BINOP
        #undef BINOP2
    }

    void Emitter::emit_compound_assign_expr(Expr* expr, ExprValueKind value_kind) {
        CompoundAssignExpr compAss = expr->compound_assign;
        
        #define BINOP2(_enum, op) case BinaryOperatorKind::Compound##_enum: { \
            emit_expr(compAss.lhs, ExprValueKind::RValue); \
            emit_expr(compAss.rhs, compAss.rhs->value_kind); \
            \
            if (compAss.lhs->type->is_integral()) { \
                if (compAss.lhs->type->is_signed()) { PUSH_PENDING_OP(OP_##op##I); } \
                if (compAss.lhs->type->is_unsigned()) { PUSH_PENDING_OP(OP_##op##U); } \
            } else if (compAss.lhs->type->is_floating_point()) { \
                PUSH_PENDING_OP(OP_##op##F); \
            } \
            break; \
        }

        #define BINOP(_enum, op) BINOP2(_enum, op)

        #define BINOP_INT2(_enum, op) case BinaryOperatorKind::Compound##_enum: { \
            emit_expr(compAss.lhs, ExprValueKind::RValue); \
            emit_expr(compAss.rhs, compAss.rhs->value_kind); \
            \
            if (compAss.lhs->type->is_integral()) { \
                if (compAss.lhs->type->is_signed()) { PUSH_PENDING_OP(OP_##op##I); } \
                if (compAss.lhs->type->is_unsigned()) { PUSH_PENDING_OP(OP_##op##U); } \
            } \
            break; \
        }

        #define BINOP_INT(_enum, op) BINOP_INT2(_enum, op)

        // Load the destination
        emit_expr(compAss.lhs, ExprValueKind::LValue);
        
        switch (compAss.op) {
            BINOP(Add, ADD)
            BINOP(Sub, SUB)
            BINOP(Mul, MUL)
            BINOP(Div, DIV)
            BINOP(Mod, MOD)
            BINOP_INT(BitAnd, AND)
            BINOP_INT(BitOr, OR)
            BINOP_INT(BitXor, XOR)
            BINOP_INT(Shl, SHL)
            BINOP_INT(Shr, SHR)
        
            default: ARIA_UNREACHABLE();
        }
        
        PUSH_PENDING_OP(OP_ST_ADDR);
        emit_expr(compAss.lhs, value_kind);

        #undef BINOP_INT
        #undef BINOP_INT2
        #undef BINOP
        #undef BINOP2
    }

    void Emitter::emit_expr(Expr* expr, ExprValueKind value_kind) {
        switch (expr->kind) {
            case ExprKind::BooleanLiteral: emit_boolean_literal_expr(expr, value_kind); break;
            case ExprKind::CharacterLiteral: emit_character_literal_expr(expr, value_kind); break;
            case ExprKind::IntegerLiteral: emit_integer_literal_expr(expr, value_kind); break;
            case ExprKind::FloatingLiteral: emit_floating_literal_expr(expr, value_kind); break;
            case ExprKind::StringLiteral: emit_string_literal_expr(expr, value_kind); break;
            case ExprKind::Null: emit_null_expr(expr, value_kind); break;
            case ExprKind::DeclRef: emit_decl_ref_expr(expr, value_kind); break;
            case ExprKind::Member: emit_member_expr(expr, value_kind); break;
            case ExprKind::Call: emit_call_expr(expr, value_kind); break;
            case ExprKind::Construct: emit_construct_expr(expr, value_kind); break;
            case ExprKind::ArraySubscript: emit_array_subscript_expr(expr, value_kind); break;
            case ExprKind::ToSlice: emit_to_slice_expr(expr, value_kind); break;
            case ExprKind::New: emit_new_expr(expr, value_kind); break;
            case ExprKind::Delete: emit_delete_expr(expr, value_kind); break;
            case ExprKind::Format: emit_format_expr(expr, value_kind); break;
            case ExprKind::Paren: emit_paren_expr(expr, value_kind); break;
            case ExprKind::ImplicitCast: emit_implicit_cast_expr(expr, value_kind); break;
            case ExprKind::Cast: emit_cast_expr(expr, value_kind); break;
            case ExprKind::UnaryOperator: emit_unary_operator_expr(expr, value_kind); break;
            case ExprKind::BinaryOperator: emit_binary_operator_expr(expr, value_kind); break;
            case ExprKind::CompoundAssign: emit_compound_assign_expr(expr, value_kind); break;

            default: ARIA_UNREACHABLE();
        }

        if (expr->result_discarded) {
            emit_destructors(m_temporaries);
            m_temporaries.clear();

            if (!expr->type->is_void()) {
                PUSH_PENDING_OP(OP_POP);
            }
        }
    }

    void Emitter::emit_translation_unit_decl(Decl* decl) {
        TranslationUnitDecl tu = decl->translation_unit;
        
        for (Stmt* stmt : tu.stmts) {
            emit_stmt(stmt);
        }
    }

    void Emitter::emit_var_decl(Decl* decl) {
        VarDecl varDecl = decl->var;
        std::string ident = fmt::format("{}::{}", m_active_namespace, varDecl.identifier);

        PUSH_OP(OP_ALLOCA);
        PUSH_U16(type_info_to_vm_type_idx(varDecl.type));

        Declaration d;
        d.type = varDecl.type;
        
        if (varDecl.type->is_string()) {
            d.destructor = Decl::Create(m_context, SourceLocation(), SourceRange(), DeclKind::BuiltinDestructor, BuiltinDestructorDecl(BuiltinKind::String));
        } else if (varDecl.type->is_structure()) {
            StructDeclaration& sDecl = varDecl.type->struct_;
            Decl* dtor = nullptr;

            for (auto& field : sDecl.source_decl->struct_.fields) {
                if (field->kind == DeclKind::Destructor) {
                    dtor = field;
                }
            }

            if (!sDecl.source_decl->struct_.definition.trivial_dtor) { d.destructor = dtor; };
        }

        // We want to allocate the variables up front (at the start of the stack frame)
        // This is why we use m_OpCodes instead of m_PendingOpCodes here
        if (varDecl.global_var) {
            ADD_STR(ident);
            PUSH_OP(OP_DECL_GLOBAL);
            PUSH_U16(STR_IDX(-1));

            d.data = ident;
            m_global_scope.declared_symbols.push_back(d);
        } else {
            PUSH_OP(OP_DECL_LOCAL);
            PUSH_PENDING_U16(static_cast<u16>(m_active_stack_frame.local_count));
            d.data = m_active_stack_frame.local_count;
            m_active_stack_frame.local_count++;
        
            m_active_stack_frame.scopes.back().declared_symbols.push_back(d);
            m_active_stack_frame.scopes.back().declared_symbol_map[ident] = m_active_stack_frame.scopes.back().declared_symbols.size() - 1;
        }
        
        // For initializers we need to just store the value in the already declared variable
        if (varDecl.initializer) {
            emit_expr(varDecl.initializer, varDecl.initializer->value_kind);

            if (varDecl.global_var) {
                ADD_STR(ident);
                PUSH_PENDING_OP(OP_ST_GLOBAL);
                PUSH_PENDING_U16(STR_IDX(-1));
            } else {
                PUSH_PENDING_OP(OP_ST_LOCAL);
                PUSH_PENDING_U16(static_cast<u16>(m_active_stack_frame.local_count - 1));
            }
        }
    }
    
    void Emitter::emit_param_decl(Decl* decl) {
        ParamDecl param = decl->param;
        PUSH_OP(OP_ALLOCA);
        PUSH_U16(type_info_to_vm_type_idx(param.type));
        PUSH_OP(OP_DECL_LOCAL);
        PUSH_U16(static_cast<u16>(m_active_stack_frame.local_count));
        PUSH_OP(OP_ST_LOCAL);
        PUSH_U16(static_cast<u16>(m_active_stack_frame.local_count));

        m_active_stack_frame.parameters[fmt::format("{}::{}", m_active_namespace, param.identifier)] = m_active_stack_frame.local_count++;
    }

    void Emitter::emit_function_decl(Decl* decl) {
        FunctionDecl& fnDecl = decl->function;

        std::string sig;
        for (auto& attr : fnDecl.attributes) {
            if (attr.kind == FunctionDecl::AttributeKind::Extern) { return; }
            if (attr.kind == FunctionDecl::AttributeKind::NoMangle) { sig = fmt::format("{}()", fnDecl.identifier); break; }
        }

        if (sig.empty()) {
            sig = fmt::format("{}::{}", m_active_namespace, mangle_function(&fnDecl));
        }

        if (fnDecl.body) {
            ADD_STR(sig);
            ADD_STR("_entry$");
            PUSH_OP(OP_FUNCTION);
            PUSH_U16(STR_IDX(-2));
            PUSH_OP(OP_LABEL);
            PUSH_U16(STR_IDX(-1));
            push_stack_frame(sig);
            
            size_t returnSlot = (fnDecl.type->function.return_type->kind == TypeKind::Void) ? 0 : 1;
            
            for (Decl* p : fnDecl.parameters) {
                emit_param_decl(p);
            }
            
            emit_stmt(fnDecl.body);
            merge_pending_op_codes();
        
            if (m_op_codes.program.back() != OP_RET || m_op_codes.program.back() != OP_RET_VAL) {
                PUSH_OP(OP_RET);
            }
        
            pop_stack_frame();
            PUSH_OP(OP_ENDFUNCTION);
        
            CompilerReflectionDeclaration d;
            d.type_index = type_info_to_vm_type_idx(fnDecl.type->function.return_type);
            d.kind = ReflectionKind::Function;
        
            m_reflection_data.declarations[sig] = d;
        }
    }

    void Emitter::emit_struct_decl(Decl* decl) {
        ARIA_TODO("Emitter::emit_struct_decl()");

        // StructDecl& sDecl = decl->Struct;
        // std::string ident = fmt::format("{}::{}", m_ActiveNamespace, sDecl.Identifier);
        // 
        // RuntimeStructDeclaration sd;
        // sd.Index = m_StructIndex++;
        // 
        // for (Decl* field : sDecl.Fields) {
        //     if (field->kind == DeclKind::Field) {
        //         sd.FieldIndices[ident] = sd.FieldIndices.size();
        //     }
        // }
        // 
        // VMStruct str;
        // str.Name = ident;
        // str.Fields.reserve(decl->Struct.Fields.Size);
        // 
        // for (Decl* field : decl->Struct.Fields) {
        //     if (field->kind == DeclKind::Field) {
        //         str.Fields.push_back(type_info_to_vm_type_idx(field->Field.Type));
        //     } else if (field->kind == DeclKind::Constructor) {
        //         ADD_STR(fmt::format("struct {}::<ctor>()", sDecl.Identifier));
        //         ADD_STR("_entry$");
        //         PUSH_OP(OpCodeKind::Function, { STR_IDX(-2) });
        //         PUSH_OP(OpCodeKind::Label, { STR_IDX(-1) });
        //         PUSH_OP(OpCodeKind::PushSF);
        // 
        //         emit_stmt(field->Constructor.Body);
        //         merge_pending_op_codes();
        // 
        //         PUSH_OP(OpCodeKind::PopSF);
        //         PUSH_OP(OpCodeKind::Ret);
        //     } else if (field->kind == DeclKind::Destructor) {
        //         ADD_STR(fmt::format("struct {}::<dtor>()", sDecl.Identifier));
        //         ADD_STR("_entry$");
        //         PUSH_OP(OpCodeKind::Function, { STR_IDX(-2) });
        //         PUSH_OP(OpCodeKind::Label, { STR_IDX(-1) });
        //         PUSH_OP(OpCodeKind::PushSF);
        // 
        //         emit_stmt(field->Destructor.Body);
        //         merge_pending_op_codes();
        // 
        //         PUSH_OP(OpCodeKind::PopSF);
        //         PUSH_OP(OpCodeKind::Ret);
        //     }
        // }
        // 
        // m_OpCodes.TypeTable.push_back(VMType(VMTypeKind::Struct, m_OpCodes.StructTable.size()));
        // m_OpCodes.StructTable.push_back(str);
        // m_Structs[decl] = sd;
    }

    void Emitter::emit_decl(Decl* decl) {
        switch (decl->kind) {
            case DeclKind::TranslationUnit: return emit_translation_unit_decl(decl);
            case DeclKind::Var: return emit_var_decl(decl);
            case DeclKind::Function: return emit_function_decl(decl);
            case DeclKind::Struct: return emit_struct_decl(decl);

            default: ARIA_UNREACHABLE();
        }
    }

    void Emitter::emit_block_stmt(Stmt* stmt) {
        BlockStmt block = stmt->block;

        for (Stmt* stmt : block.stmts) {
            emit_stmt(stmt);
        }
    }

    void Emitter::emit_while_stmt(Stmt* stmt) {
        WhileStmt wh = stmt->while_;
        
        ADD_STR(fmt::format("loop.start_{}", m_loop_counter));
        ADD_STR(fmt::format("loop.end_{}", m_loop_counter));
        u16 startIdx = STR_IDX(-2);
        u16 endIdx = STR_IDX(-1);
        m_loop_counter++;

        push_scope();
        
        PUSH_PENDING_OP(OP_LABEL);
        PUSH_PENDING_U16(startIdx);
        emit_expr(wh.condition, wh.condition->value_kind);
        PUSH_PENDING_OP(OP_JF_POP);
        PUSH_PENDING_U16(endIdx);
        emit_stmt(wh.body);
        
        PUSH_PENDING_OP(OP_JMP);
        PUSH_PENDING_U16(startIdx);
        PUSH_PENDING_OP(OP_LABEL);
        PUSH_PENDING_U16(endIdx);

        pop_scope();
    }

    void Emitter::emit_do_while_stmt(Stmt* stmt) {
        DoWhileStmt wh = stmt->do_while;
        
        ADD_STR(fmt::format("loop.start_{}", m_loop_counter));
        ADD_STR(fmt::format("loop.end_{}", m_loop_counter));
        u16 startIdx = STR_IDX(-2);
        u16 endIdx = STR_IDX(-1);
        m_loop_counter++;
        
        push_scope();

        PUSH_PENDING_OP(OP_LABEL);
        PUSH_PENDING_U16(startIdx);
        emit_stmt(wh.body);
        
        emit_expr(wh.condition, wh.condition->value_kind);
        PUSH_PENDING_OP(OP_JT_POP);
        PUSH_PENDING_U16(startIdx);
        PUSH_PENDING_OP(OP_JMP);
        PUSH_PENDING_U16(endIdx);
        
        PUSH_PENDING_OP(OP_LABEL);
        PUSH_PENDING_U16(endIdx);

        pop_scope();
    }

    void Emitter::emit_for_stmt(Stmt* stmt) {
        ForStmt fs = stmt->for_;
        
        ADD_STR(fmt::format("loop.start_{}", m_loop_counter));
        ADD_STR(fmt::format("loop.end_{}", m_loop_counter));
        u16 startIdx = STR_IDX(-2);
        u16 endIdx = STR_IDX(-1);
        m_loop_counter++;
        
        push_scope();
        if (fs.prologue) { emit_decl(fs.prologue); }
        
        PUSH_PENDING_OP(OP_LABEL);
        PUSH_PENDING_U16(startIdx);
        if (fs.condition) {
            emit_expr(fs.condition, fs.condition->value_kind);
            PUSH_PENDING_OP(OP_JF_POP);
            PUSH_PENDING_U16(endIdx);
        }
        
        emit_block_stmt(fs.body);
            
        if (fs.step) {
            emit_expr(fs.step, fs.step->value_kind);
        }
        
        PUSH_PENDING_OP(OP_JMP);
        PUSH_PENDING_U16(startIdx);
        PUSH_PENDING_OP(OP_LABEL);
        PUSH_PENDING_U16(endIdx);
        
        pop_scope();
    }

    void Emitter::emit_if_stmt(Stmt* stmt) {
        IfStmt ifs = stmt->if_;
        
        ADD_STR(fmt::format("if.body{}", m_if_counter));
        ADD_STR(fmt::format("if.end{}", m_if_counter));
        u16 bodyIdx = STR_IDX(-2);
        u16 endIdx = STR_IDX(-1);
        
        push_scope();

        emit_expr(ifs.condition, ifs.condition->value_kind);
        PUSH_PENDING_OP(OP_JT_POP);
        PUSH_PENDING_U16(bodyIdx);
        PUSH_PENDING_OP(OP_JMP);
        PUSH_PENDING_U16(endIdx);
        
        PUSH_PENDING_OP(OP_LABEL);
        PUSH_PENDING_U16(bodyIdx);
        emit_stmt(ifs.body);
        PUSH_PENDING_OP(OP_LABEL);
        PUSH_PENDING_U16(endIdx);

        pop_scope();
        
        m_if_counter++;
    }

    void Emitter::emit_break_stmt(Stmt* stmt) {
        ADD_STR(fmt::format("loop.end_{}", m_loop_counter - 1));
        PUSH_PENDING_OP(OP_JMP);
        PUSH_PENDING_U16(STR_IDX(-1));
    }

    void Emitter::emit_continue_stmt(Stmt* stmt) {
        ADD_STR(fmt::format("loop.start_{}", m_loop_counter - 1));
        PUSH_PENDING_OP(OP_JMP);
        PUSH_PENDING_U16(STR_IDX(-1));
    }

    void Emitter::emit_return_stmt(Stmt* stmt) {
        ReturnStmt ret = stmt->return_;
        if (ret.value) {
            emit_expr(ret.value, ret.value->value_kind);
            PUSH_PENDING_OP(OP_RET_VAL);
            return;
        }
        
        PUSH_PENDING_OP(OP_RET);
    }

    void Emitter::emit_expr_stmt(Stmt* stmt) {
        emit_expr(stmt->expr, stmt->expr->value_kind);
    }

    void Emitter::emit_decl_stmt(Stmt* stmt) {
        emit_decl(stmt->decl);
    }

    void Emitter::emit_stmt(Stmt* stmt) {
        switch (stmt->kind) {
            case StmtKind::Nop:
            case StmtKind::Import: return;
            case StmtKind::Block: push_scope(); emit_block_stmt(stmt); pop_scope(); return;
            case StmtKind::While: return emit_while_stmt(stmt);
            case StmtKind::DoWhile: return emit_do_while_stmt(stmt);
            case StmtKind::For: return emit_for_stmt(stmt);
            case StmtKind::If: return emit_if_stmt(stmt);
            case StmtKind::Break: return emit_break_stmt(stmt);
            case StmtKind::Continue: return emit_continue_stmt(stmt);
            case StmtKind::Return: return emit_return_stmt(stmt);
            case StmtKind::Expr: return emit_expr_stmt(stmt);
            case StmtKind::Decl: return emit_decl_stmt(stmt);

            default: ARIA_UNREACHABLE();
        }
    }

    void Emitter::emit_destructors(const std::vector<Declaration>& declarations) {
        for (auto it = declarations.rbegin(); it != declarations.rend(); it++) {
            auto& decl = *it;
        
            if (decl.destructor) {
                if (decl.destructor->kind == DeclKind::BuiltinDestructor) {
                    switch (decl.destructor->built_in_destructor.kind) {
                        case BuiltinKind::String: ADD_STR("__aria_destruct_str()"); break;
                        default: ARIA_UNREACHABLE();
                    }
                } else if (decl.destructor->kind == DeclKind::Destructor) {
                    ADD_STR(fmt::format("{}::<dtor>()", type_info_to_string(decl.type)));
                }

                u16 idx = STR_IDX(-1);

                if (std::holds_alternative<size_t>(decl.data)) {
                    PUSH_PENDING_OP(OP_LD_PTR_LOCAL);
                    PUSH_PENDING_U16(static_cast<u16>(std::get<size_t>(decl.data)));
                } else if (std::holds_alternative<std::string>(decl.data)) {
                    ADD_STR(std::get<std::string>(decl.data));
                    PUSH_PENDING_OP(OP_LD_PTR_GLOBAL);
                    PUSH_PENDING_U16(STR_IDX(-1));
                }
        
                PUSH_PENDING_OP(OP_CALL);
                PUSH_PENDING_U16(idx);
            }
        }
    }

    void Emitter::push_stack_frame(const std::string& name) {
        m_active_stack_frame.scopes.clear();
        m_active_stack_frame.name = name;
    }

    void Emitter::pop_stack_frame() {
        m_active_stack_frame.scopes.clear();
        m_active_stack_frame.name.clear();
        m_active_stack_frame.parameters.clear();
        m_active_stack_frame.local_count = 0;

        // Reset counters
        m_and_counter = 0;
        m_or_counter = 0;
        m_loop_counter = 0;
        m_if_counter = 0;
    }

    void Emitter::push_scope() {
        m_active_stack_frame.scopes.emplace_back();
    }

    void Emitter::pop_scope() {
        emit_destructors(m_active_stack_frame.scopes.back().declared_symbols);
        m_active_stack_frame.scopes.pop_back();
    }

    void Emitter::merge_pending_op_codes() {
        m_op_codes.program.reserve(m_op_codes.program.size() + m_pending_op_codes.size());
        m_op_codes.program.insert(m_op_codes.program.end(), m_pending_op_codes.begin(), m_pending_op_codes.end());
        m_pending_op_codes.clear();
    }

    u16 Emitter::type_info_to_vm_type_idx(TypeInfo* t) {
        if (t->is_primitive()) {
            return m_basic_types[t->kind];
        }

        if (t->is_structure()) {
            return m_structs.at(t->struct_.source_decl).index;
        }

        ARIA_UNREACHABLE();
    }

    std::string Emitter::mangle_function(FunctionDecl* fn) {
        std::string ident = fmt::format("{}(", fn->identifier);

        for (size_t i = 0; i < fn->parameters.size; i++) {
            ident += type_info_to_string(fn->parameters.items[i]->param.type);

            if (i != fn->parameters.size - 1) {
                ident += ", ";
            }
        }

        ident += ")";

        return ident;
    }

} // namespace Aria::Internal
