#include "aria/internal/compiler/codegen/emitter.hpp"

#define ADD_STR(str) (m_OpCodes.StringTable.push_back(str))
#define ADD_CONSTANT(consta) (m_OpCodes.ConstantTable.push_back(consta))
#define STR_IDX(idx) (static_cast<OpCode>(m_OpCodes.StringTable.size() + idx))
#define CONST_IDX(idx) (static_cast<OpCode>(m_OpCodes.ConstantTable.size() + idx))
#define PUSH_OP(op) (m_OpCodes.Program.push_back(op))
#define PUSH_PENDING_OP(op) (m_PendingOpCodes.push_back(op))

namespace Aria::Internal {

    Emitter::Emitter(CompilationContext* ctx) {
        m_Context = ctx;

        emit_impl();
    }

    void Emitter::emit_impl() {
        add_basic_types();
        add_user_defined_types();
        emit_declarations();
        emit_start_end();

        m_Context->Ops = m_OpCodes;
        m_Context->ReflectionData = m_ReflectionData;
    }

    void Emitter::add_basic_types() {
        #define ADD_TYPE(vmType, primType) do { m_OpCodes.TypeTable.emplace_back(VMTypeKind::vmType); m_BasicTypes[TypeKind::primType] = static_cast<u16>(i); i++; } while (0)
        #define ADD_TYPE_STRUCT(vmType, primType, struc) do { m_OpCodes.TypeTable.emplace_back(VMTypeKind::vmType, struc); m_BasicTypes[TypeKind::primType] = static_cast<u16>(i); i++; } while (0)

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
        ADD_TYPE_STRUCT(Slice, Slice, VMStruct("<builtin_slice>", std::vector{static_cast<size_t>(m_BasicTypes[TypeKind::Ptr]), static_cast<size_t>(m_BasicTypes[TypeKind::ULong])}));

        #undef ADD_TYPE
    }

    void Emitter::add_user_defined_types() {
        m_StructIndex = static_cast<u16>(m_BasicTypes.size());

        for (Module* mod : m_Context->Modules) {
            if (mod->Units.size() == 0) { continue; }
            m_ActiveNamespace = mod->Name;

            for (CompilationUnit* unit : mod->Units) {
                for (Decl* s : unit->Structs) {
                    emit_decl(s);
                }
            }
        }
    }

    void Emitter::emit_declarations() {
        for (Module* mod : m_Context->Modules) {
            if (mod->Units.size() == 0) { continue; }
            m_ActiveNamespace = mod->Name;

            for (CompilationUnit* unit : mod->Units) {
                for (Decl* f : unit->Funcs) {
                    emit_decl(f);
                }
            }

            // Global variable initializion requires special functions
            std::string startSig = fmt::format("_start${}()",mod->Name);
            std::string endSig = fmt::format("_end${}()", mod->Name);

            ADD_STR(startSig);
            ADD_STR("_entry$");
            PUSH_OP(OP_FUNCTION);
            PUSH_OP(STR_IDX(-2));
            PUSH_OP(OP_LABEL);
            PUSH_OP(STR_IDX(-1));
            push_stack_frame(startSig);

            for (CompilationUnit* unit : mod->Units) {
                for (Decl* g : unit->Globals) {
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
            PUSH_OP(STR_IDX(-2));
            PUSH_OP(OP_LABEL);
            PUSH_OP(STR_IDX(-1));
            push_stack_frame(endSig);

            emit_destructors(m_GlobalScope.DeclaredSymbols);
            merge_pending_op_codes();

            pop_stack_frame();
            PUSH_OP(OP_RET);
            PUSH_OP(OP_ENDFUNCTION);

            mod->Ops = m_OpCodes;
            mod->ReflectionData = m_ReflectionData;
        }
    }

    void Emitter::emit_start_end() {
        ADD_STR("_start$()");
        ADD_STR("_entry$");
        PUSH_OP(OP_FUNCTION);
        PUSH_OP(STR_IDX(-2));
        PUSH_OP(OP_LABEL);
        PUSH_OP(STR_IDX(-1));
        push_stack_frame("_start$()");

        for (Module* mod : m_Context->Modules) {
            if (mod->Units.size() == 0) { continue; }
            ADD_STR(fmt::format("_start${}()", mod->Name));
            PUSH_OP(OP_CALL);
            PUSH_OP(STR_IDX(-1));
        }

        pop_stack_frame();
        PUSH_OP(OP_RET);
        PUSH_OP(OP_ENDFUNCTION);

        ADD_STR("_end$()");
        ADD_STR("_entry$");
        PUSH_OP(OP_FUNCTION);
        PUSH_OP(STR_IDX(-2));
        PUSH_OP(OP_LABEL);
        PUSH_OP(STR_IDX(-1));
        push_stack_frame("_end$()");

        for (Module* mod : m_Context->Modules) {
            if (mod->Units.size() == 0) { continue; }
            ADD_STR(fmt::format("_end${}()", mod->Name));
            PUSH_OP(OP_CALL);
            PUSH_OP(STR_IDX(-1));
        }

        pop_stack_frame();
        PUSH_OP(OP_RET);
        PUSH_OP(OP_ENDFUNCTION);
    }

    void Emitter::emit_BooleanConstant_expr(Expr* expr, ExprValueKind valueKind) {
        BooleanConstantExpr bc = expr->BooleanConstant;
        ADD_CONSTANT(static_cast<u64>(bc.Value));
        PUSH_PENDING_OP(OP_LD_CONST);
        PUSH_PENDING_OP(type_info_to_vm_type_idx(expr->Type));
        PUSH_PENDING_OP(CONST_IDX(-1));
    }

    void Emitter::emit_CharacterConstant_expr(Expr* expr, ExprValueKind valueKind) {
        CharacterConstantExpr cc = expr->CharacterConstant;
        ADD_CONSTANT(static_cast<u64>(cc.Value));
        PUSH_PENDING_OP(OP_LD_CONST);
        PUSH_PENDING_OP(type_info_to_vm_type_idx(expr->Type));
        PUSH_PENDING_OP(CONST_IDX(-1));
    }

    void Emitter::emit_IntegerConstant_expr(Expr* expr,ExprValueKind valueKind) {
        IntegerConstantExpr ic = expr->IntegerConstant;
        ADD_CONSTANT(static_cast<u64>(ic.Value));
        PUSH_PENDING_OP(OP_LD_CONST);
        PUSH_PENDING_OP(type_info_to_vm_type_idx(expr->Type));
        PUSH_PENDING_OP(CONST_IDX(-1));
    }

    void Emitter::emit_FloatingConstant_expr(Expr* expr, ExprValueKind valueKind) {
        FloatingConstantExpr fc = expr->FloatingConstant;
        PUSH_PENDING_OP(OP_LD_CONST);
        PUSH_PENDING_OP(type_info_to_vm_type_idx(expr->Type));

        switch (expr->Type->Kind) {
            case TypeKind::Float: ADD_CONSTANT(static_cast<float>(fc.Value)); break;
            case TypeKind::Double: ADD_CONSTANT(static_cast<double>(fc.Value)); break;

            default: ARIA_UNREACHABLE();
        }
        
        PUSH_PENDING_OP(CONST_IDX(-1));
    }

    void Emitter::emit_StringConstant_expr(Expr* expr, ExprValueKind valueKind) {
        StringConstantExpr sc = expr->StringConstant;
        ADD_STR(fmt::format("{}", sc.Value));
        PUSH_PENDING_OP(OP_LD_STR);
        PUSH_PENDING_OP(STR_IDX(-1));
    }

    void Emitter::emit_Null_expr(Expr* expr, ExprValueKind valueKind) {
        PUSH_PENDING_OP(OP_LD_NULL);
    }

    // List of op codes this function can emit:
    // 
    // lvalue: LdPtrLocal, LdPtrArg, LdPtrGlobal, LdFunc
    // lvalue + ref: LdLocal, LdArg, LdGlobal
    // 
    // rvalue: LdLocal, LdArg, LdGlobal
    // rvalue + ref: (LdLocal, LdArg, LdGlobal) + deref
    void Emitter::emit_DeclRef_expr(Expr* expr, ExprValueKind valueKind) {
        DeclRefExpr declRef = expr->DeclRef;
        std::string ident;

        if (declRef.NameSpecifier) {
            ident = fmt::format("{}::{}", declRef.NameSpecifier->Scope.Identifier, declRef.Identifier);
        } else {
            ident = fmt::format("{}::{}", m_ActiveNamespace, declRef.Identifier);
        }

        // VVV -Var- VVV //
        if (declRef.ReferencedDecl->Kind == DeclKind::Var) {
            // Check for global variables
            if (declRef.ReferencedDecl->Var.GlobalVar) {
                ADD_STR(ident);

                if (valueKind == ExprValueKind::LValue) {
                    if (expr->Type->is_reference()) {
                        PUSH_PENDING_OP(OP_LD_GLOBAL);
                    } else {
                        PUSH_PENDING_OP(OP_LD_PTR_GLOBAL);
                    }

                    PUSH_PENDING_OP(STR_IDX(-1));
                } else if (valueKind == ExprValueKind::RValue) {
                    PUSH_PENDING_OP(OP_LD_GLOBAL);
                    PUSH_PENDING_OP(STR_IDX(-1));
        
                    if (expr->Type->is_reference()) {
                        ARIA_TODO("Dereferencing references");
                    }
                }

                return;
            }

            for (size_t i = m_ActiveStackFrame.Scopes.size(); i > 0; i--) {
                Scope& s = m_ActiveStackFrame.Scopes[i - 1];
                if (s.DeclaredSymbolMap.contains(ident)) {
                    Declaration& decl = s.DeclaredSymbols[s.DeclaredSymbolMap.at(ident)];
                    
                    if (valueKind == ExprValueKind::LValue) {
                        if (expr->Type->is_reference()) {
                            PUSH_PENDING_OP(OP_LD_LOCAL);
                        } else {
                            PUSH_PENDING_OP(OP_LD_PTR_LOCAL);
                        }

                        PUSH_PENDING_OP(static_cast<OpCode>(std::get<size_t>(decl.Data)));
                    } else if (valueKind == ExprValueKind::RValue) {
                        PUSH_PENDING_OP(OP_LD_LOCAL);
                        PUSH_PENDING_OP(static_cast<OpCode>(std::get<size_t>(decl.Data)));
        
                        if (expr->Type->is_reference()) {
                            ARIA_TODO("Dereferencing references");
                        }
                    }
                }
            }

            return;
        }
        // ^^^ -Var- ^^^ //
        
        // VVV -Param- VVV //
        if (declRef.ReferencedDecl->Kind == DeclKind::Param) {
            if (valueKind == ExprValueKind::LValue) {
                if (expr->Type->is_reference()) {
                    PUSH_PENDING_OP(OP_LD_LOCAL);
                } else {
                    PUSH_PENDING_OP(OP_LD_PTR_LOCAL);
                }

                PUSH_PENDING_OP(static_cast<OpCode>(m_ActiveStackFrame.Parameters.at(ident)));
            } else if (valueKind == ExprValueKind::RValue) {
                PUSH_PENDING_OP(OP_LD_LOCAL);
                PUSH_PENDING_OP(static_cast<OpCode>(m_ActiveStackFrame.Parameters.at(ident)));
        
                if (expr->Type->is_reference()) {
                    ARIA_TODO("Dereferencing references");
                }
            }

            return;
        }
        // ^^^ -Param- ^^^ //
        
        // VVV -Function- VVV //
        if (declRef.ReferencedDecl->Kind == DeclKind::Function) {
            return;
        }
        // ^^^ -Function- ^^^ //

        ARIA_UNREACHABLE();
    }

    void Emitter::emit_Member_expr(Expr* expr, ExprValueKind valueKind) {
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
        //             if (valueKind == ExprValueKind::LValue) {
        //                 PUSH_PENDING_OP(OpCodeKind::LdPtrMember, OpCodeMember(sdecl.FieldIndices.at(fd->GetIdentifier()), {VMTypeKind::Ptr}, TypeInfoToVMType(mem->GetParentType())));
        //             } else if (valueKind == ExprValueKind::RValue) {
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

    void Emitter::emit_BuiltinMember_expr(Expr* expr, ExprValueKind valueKind) {
        MemberExpr mem = expr->Member;

        emit_expr(mem.Parent, mem.Parent->ValueKind);

        switch (mem.Parent->Type->Kind) {
            case TypeKind::Slice: {
                size_t idx = 0;

                if (mem.Member == "mem") { idx = 0; }
                else if (mem.Member == "len") { idx = 1; }

                if (valueKind == ExprValueKind::LValue) {
                    PUSH_PENDING_OP(OP_LD_PTR_FIELD);
                    PUSH_PENDING_OP(static_cast<OpCode>(idx));
                    PUSH_PENDING_OP(type_info_to_vm_type_idx(mem.Parent->Type));
                } else {
                    PUSH_PENDING_OP(OP_LD_FIELD);
                    PUSH_PENDING_OP(static_cast<OpCode>(idx));
                    PUSH_PENDING_OP(type_info_to_vm_type_idx(mem.Parent->Type));
                }

                break;
            }

            default: ARIA_UNREACHABLE();
        }
    }

    void Emitter::emit_Self_expr(Expr* expr, ExprValueKind valueKind) {
        ARIA_TODO("Emitter::EmitSelfExpr()");
    }

    void Emitter::emit_Temporary_expr(Expr* expr, ExprValueKind valueKind) {
        TemporaryExpr& temp = expr->Temporary;

        OpCode idx = static_cast<OpCode>(m_ActiveStackFrame.LocalCount);

        // Create a new temporary
        PUSH_OP(OP_ALLOCA);
        PUSH_OP(type_info_to_vm_type_idx(expr->Type));
        PUSH_OP(OP_DECL_LOCAL);
        PUSH_OP(idx);

        emit_expr(temp.Expression, valueKind);
        PUSH_PENDING_OP(OP_ST_LOCAL);
        PUSH_PENDING_OP(idx);
        
        Declaration d;
        d.Type = temp.Expression->Type;
        d.Data = m_ActiveStackFrame.LocalCount;
        d.Destructor = temp.Destructor;
        m_ActiveStackFrame.LocalCount++;
        
        m_Temporaries.push_back(d);

        PUSH_PENDING_OP(OP_LD_LOCAL);
        PUSH_PENDING_OP(idx);
    }

    void Emitter::emit_Copy_expr(Expr* expr, ExprValueKind valueKind) {
        CopyExpr copy = expr->Copy;
        
        if (copy.Constructor->Kind == DeclKind::BuiltinCopyConstructor) {
            switch (copy.Constructor->BuiltinCopyConstructor.Kind) {
                case BuiltinKind::String: ADD_STR("__aria_copy_str()"); break;
                default: ARIA_UNREACHABLE();
            }
        }

        OpCode idx = STR_IDX(-1);

        emit_expr(copy.Expression, ExprValueKind::LValue);
        PUSH_PENDING_OP(OP_CALL);
        PUSH_PENDING_OP(idx);
    }

    void Emitter::emit_Call_expr(Expr* expr, ExprValueKind valueKind) {
        CallExpr call = expr->Call;
        
        ARIA_ASSERT(call.Callee->Kind == ExprKind::DeclRef, "Callee of call expression must be a decl-ref");

        for (auto it = call.Arguments.rbegin(); it != call.Arguments.rend(); it--) {
            emit_expr(*it, (*it)->ValueKind);
        }

        std::string ident;
        for (auto& attr : call.Callee->DeclRef.ReferencedDecl->Function.Attributes) {
            if (attr.Kind == FunctionDecl::AttributeKind::Extern) { ident = fmt::format("{}", attr.Arg); }
            else if (attr.Kind == FunctionDecl::AttributeKind::NoMangle) { ident = fmt::format("{}()", call.Callee->DeclRef.Identifier); }
        }

        if (ident.empty()) {
            if (call.Callee->DeclRef.NameSpecifier) {
                ident = fmt::format("{}::{}", call.Callee->DeclRef.NameSpecifier->Scope.Identifier, mangle_function(&call.Callee->DeclRef.ReferencedDecl->Function));
            } else {
                ident = fmt::format("{}::{}", m_ActiveNamespace, mangle_function(&call.Callee->DeclRef.ReferencedDecl->Function));
            }
        }

        ADD_STR(ident);
        PUSH_PENDING_OP(OP_CALL);
        PUSH_PENDING_OP(STR_IDX(-1));
    }

    void Emitter::emit_Construct_expr(Expr* expr, ExprValueKind valueKind) {
        ARIA_TODO("Emitter::EmitConstructExpr()");
        // ConstructExpr& ct = expr->Construct;
        // 
        // PUSH_OP(OpCodeKind::Alloca, { type_info_to_vm_type_idx(expr->Type) });
        // PUSH_OP(OpCodeKind::DeclareLocal, { m_ActiveStackFrame.LocalCount });
        // ADD_STR(fmt::format("{}::<ctor>()", type_info_to_string(expr->Type)));
        // PUSH_PENDING_OP(OpCodeKind::LdFunc, { STR_IDX(-1) });
        // PUSH_PENDING_OP(OpCodeKind::LdPtrLocal, { m_ActiveStackFrame.LocalCount });
        // PUSH_PENDING_OP(OpCodeKind::DeclareArg, { static_cast<size_t>(0) });
        // PUSH_PENDING_OP(OpCodeKind::Call, { static_cast<size_t>(1), static_cast<size_t>(0) });
        // PUSH_PENDING_OP(OpCodeKind::LdLocal, { m_ActiveStackFrame.LocalCount });
        // 
        // Declaration d;
        // d.Type = expr->Type;
        // d.Data = m_ActiveStackFrame.LocalCount;
        // d.Destructor = nullptr;
        // m_ActiveStackFrame.LocalCount++;
        // 
        // m_ActiveStackFrame.Scopes.back().DeclaredSymbols.push_back(d);
    }

    void Emitter::emit_MethodCall_expr(Expr* expr, ExprValueKind valueKind) {
        ARIA_TODO("Emitter::EmitMethodCallExpr()");
        // MethodCallExpr* call = GetNode<MethodCallExpr>(expr);
        // 
        // emit_expr(call->GetCallee(), call->GetCallee()->GetValueKind());
        // 
        // // Push self
        // emit_expr(call->GetCallee()->GetParent(), ExprValueKind::LValue);
        // PUSH_PENDING_OP(OpCodeKind::DeclareArg, static_cast<size_t>(0));
        // 
        // for (size_t i = 0; i < call->GetArguments().Size; i++) {
        //     emit_expr(call->GetArguments().Items[i], call->GetArguments().Items[i]->GetValueKind());
        //     PUSH_PENDING_OP(OpCodeKind::DeclareArg, i + 1);
        // }
        // 
        // size_t retCount = 0;
        // TypeInfo* retType = call->GetResolvedType();
        // if (retType->Type != TypeKind::Void) {
        //     PUSH_PENDING_OP(OpCodeKind::Alloca, TypeInfoToVMType(retType));
        //     retCount = 1;
        // }
        // 
        // // We do size + 1 because self is an implicit parameter that isn't in the argument vector,
        // // However the VM certainly does need the actual amount of arguments
        // PUSH_PENDING_OP(OpCodeKind::Call, OpCodeCall(call->GetArguments().Size + 1, TypeInfoToVMType(retType)));
        // 
        // // The only special case is when returning a reference and getting it as an rvalue
        // if (valueKind == ExprValueKind::RValue) {
        //     if (retType->is_reference()) {
        //         retType->Reference = false;
        //         PUSH_PENDING_OP(OpCodeKind::Deref, TypeInfoToVMType(retType));
        //         retType->Reference = true;
        //     }
        // }
    }

    void Emitter::emit_ArraySubscript_expr(Expr* expr, ExprValueKind valueKind) {
        ArraySubscriptExpr subs = expr->ArraySubscript;

        switch (subs.Array->Type->Kind) {
            case TypeKind::Ptr: {
                emit_expr(subs.Array, subs.Array->ValueKind);
                emit_expr(subs.Index, subs.Index->ValueKind);
                PUSH_PENDING_OP(OP_OFFP);
                PUSH_PENDING_OP(type_info_to_vm_type_idx(subs.Array->Type->Base));

                if (valueKind == ExprValueKind::RValue) {
                    PUSH_PENDING_OP(OP_LD);
                    PUSH_PENDING_OP(type_info_to_vm_type_idx(subs.Array->Type->Base));
                }

                break;
            }

            case TypeKind::Slice: {
                emit_expr(subs.Array, subs.Array->ValueKind);
                PUSH_PENDING_OP(OP_LD_FIELD);
                PUSH_PENDING_OP(static_cast<OpCode>(0));
                PUSH_PENDING_OP(type_info_to_vm_type_idx(subs.Array->Type));

                emit_expr(subs.Index, subs.Index->ValueKind);
                PUSH_PENDING_OP(OP_OFFP);
                PUSH_PENDING_OP(type_info_to_vm_type_idx(subs.Array->Type->Base));

                if (valueKind == ExprValueKind::RValue) {
                    PUSH_PENDING_OP(OP_LD);
                    PUSH_PENDING_OP(type_info_to_vm_type_idx(subs.Array->Type->Base));
                }

                break;
            }

            default: ARIA_UNREACHABLE();
        }
    }

    void Emitter::emit_ToSlice_expr(Expr* expr, ExprValueKind valueKind) {
        ToSliceExpr tos = expr->ToSlice;

        PUSH_PENDING_OP(OP_ALLOCA);
        PUSH_PENDING_OP(type_info_to_vm_type_idx(expr->Type));

        switch (tos.Source->Type->Kind) {
            case TypeKind::Ptr: {
                PUSH_PENDING_OP(OP_LD_PTR); // Load the pointer to the slice
                PUSH_PENDING_OP(OP_DUP);
                emit_expr(tos.Source, tos.Source->ValueKind);
                PUSH_PENDING_OP(OP_ST_FIELD);
                PUSH_PENDING_OP(static_cast<OpCode>(0)); // .mem is the first field
                PUSH_PENDING_OP(type_info_to_vm_type_idx(expr->Type));
                emit_expr(tos.Len, tos.Len->ValueKind);
                PUSH_PENDING_OP(OP_ST_FIELD);
                PUSH_PENDING_OP(static_cast<OpCode>(1)); // .len is the second field
                PUSH_PENDING_OP(type_info_to_vm_type_idx(expr->Type));
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

    void Emitter::emit_New_expr(Expr* expr, ExprValueKind valueKind) {
        NewExpr n = expr->New;

        if (n.Array) {
            emit_expr(n.Initializer, n.Initializer->ValueKind);
            PUSH_PENDING_OP(OP_NEW_ARR);
            PUSH_PENDING_OP(type_info_to_vm_type_idx(expr->Type->Base));
        } else {
            PUSH_PENDING_OP(OP_NEW);
            PUSH_PENDING_OP(type_info_to_vm_type_idx(expr->Type->Base));

            if (n.Initializer) {
                PUSH_PENDING_OP(OP_DUP);
                emit_expr(n.Initializer, n.Initializer->ValueKind);
                PUSH_PENDING_OP(OP_ST_ADDR);
            }
        }
    }

    void Emitter::emit_Delete_expr(Expr* expr, ExprValueKind valueKind) {
        DeleteExpr n = expr->Delete;

        emit_expr(n.Expression, n.Expression->ValueKind);
        PUSH_PENDING_OP(OP_FREE);
    }

    void Emitter::emit_Format_expr(Expr* expr, ExprValueKind valueKind) {
        FormatExpr format = expr->Format;
        std::string_view fStr = format.Args.Items[0]->Temporary.Expression->StringConstant.Value;
        
        PUSH_OP(OP_ALLOCA);
        PUSH_OP(type_info_to_vm_type_idx(expr->Type));
        PUSH_OP(OP_DECL_LOCAL);
        PUSH_OP(static_cast<OpCode>(m_ActiveStackFrame.LocalCount));
        size_t idx = m_ActiveStackFrame.LocalCount++;
        
        for (auto& arg : format.ResolvedArgs) {
            emit_expr(arg.Arg, arg.Arg->ValueKind);
            ADD_STR(fmt::format("__aria_append_str<{}>()", type_info_to_string(arg.Arg->Type)));
            PUSH_PENDING_OP(OP_LD_PTR_LOCAL);
            PUSH_PENDING_OP(static_cast<OpCode>(idx));
            PUSH_PENDING_OP(OP_CALL);
            PUSH_PENDING_OP(STR_IDX(-1));
        }
        
        PUSH_PENDING_OP(OP_LD_LOCAL);
        PUSH_PENDING_OP(static_cast<OpCode>(idx));
    }

    void Emitter::emit_Paren_expr(Expr* expr, ExprValueKind valueKind) {
        ParenExpr paren = expr->Paren;
        emit_expr(paren.Expression, expr->ValueKind);
    }

    void Emitter::emit_ImplicitCast_expr(Expr* expr, ExprValueKind valueKind) {
        ImplicitCastExpr cast = expr->ImplicitCast;
        
        if (cast.Kind == CastKind::LValueToRValue) {
            return emit_expr(cast.Expression, ExprValueKind::RValue);
        } else {
            emit_expr(cast.Expression, cast.Expression->ValueKind);

            switch (cast.Kind) {
                case CastKind::Integral: {
                    PUSH_PENDING_OP(OP_CONV_ITOI);
                    PUSH_PENDING_OP(type_info_to_vm_type_idx(expr->Type));
                    break;
                }

                case CastKind::Floating: {
                    PUSH_PENDING_OP(OP_CONV_FTOF);
                    PUSH_PENDING_OP(type_info_to_vm_type_idx(expr->Type));
                    break;
                }

                case CastKind::IntegralToFloating: {
                    PUSH_PENDING_OP(OP_CONV_ITOF);
                    PUSH_PENDING_OP(type_info_to_vm_type_idx(expr->Type));
                    break;
                }

                case CastKind::FloatingToIntegral: {
                    PUSH_PENDING_OP(OP_CONV_FTOI);
                    PUSH_PENDING_OP(type_info_to_vm_type_idx(expr->Type));
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

    void Emitter::emit_Cast_expr(Expr* expr, ExprValueKind valueKind) {
        CastExpr cast = expr->Cast;
        emit_expr(cast.Expression, cast.Expression->ValueKind);
    }

    void Emitter::emit_UnaryOperator_expr(Expr* expr, ExprValueKind valueKind) {
        UnaryOperatorExpr unop = expr->UnaryOperator;
        
        switch (unop.Operator) {
            case UnaryOperatorKind::Negate: { 
                emit_expr(unop.Expression, unop.Expression->ValueKind);
                if (expr->Type->is_integral()) {
                    ARIA_ASSERT(expr->Type->is_signed(), "Type must be signed");
                    PUSH_PENDING_OP(OP_NEGI);
                } else if (expr->Type->is_floating_point()) {
                    PUSH_PENDING_OP(OP_NEGF);
                } else {
                    ARIA_UNREACHABLE();
                }

                break;
            }

            case UnaryOperatorKind::AddressOf: {
                emit_expr(unop.Expression, ExprValueKind::LValue);
                break;
            }

            case UnaryOperatorKind::Dereference: {
                emit_expr(unop.Expression, unop.Expression->ValueKind);

                if (valueKind == ExprValueKind::RValue) {
                    PUSH_PENDING_OP(OP_LD);
                    PUSH_PENDING_OP(type_info_to_vm_type_idx(expr->Type));
                }
                
                break;
            }

            default: ARIA_UNREACHABLE();
        }
    }

    void Emitter::emit_BinaryOperator_expr(Expr* expr, ExprValueKind valueKind) {
        BinaryOperatorExpr binop = expr->BinaryOperator;
        
        #define BINOP2(_enum, op) case BinaryOperatorKind::_enum: { \
            emit_expr(binop.LHS, binop.LHS->ValueKind); \
            emit_expr(binop.RHS, binop.RHS->ValueKind); \
            \
            if (binop.LHS->Type->is_integral()) { \
                if (binop.LHS->Type->is_signed()) { PUSH_PENDING_OP(OP_##op##I); } \
                if (binop.LHS->Type->is_unsigned()) { PUSH_PENDING_OP(OP_##op##U); } \
            } else if (binop.LHS->Type->is_floating_point()) { \
                PUSH_PENDING_OP(OP_##op##F); \
            } \
            break; \
        }

        #define BINOP(_enum, op) BINOP2(_enum, op)

        #define BINOP_INT2(_enum, op) case BinaryOperatorKind::_enum: { \
            emit_expr(binop.LHS, binop.LHS->ValueKind); \
            emit_expr(binop.RHS, binop.RHS->ValueKind); \
            \
            if (binop.LHS->Type->is_integral()) { \
                if (binop.LHS->Type->is_signed()) { PUSH_PENDING_OP(OP_##op##I); } \
                if (binop.LHS->Type->is_unsigned()) { PUSH_PENDING_OP(OP_##op##U); } \
            } \
            break; \
        }

        #define BINOP_INT(_enum, op) BINOP_INT2(_enum, op)
        
        switch (binop.Operator) {
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
                ADD_STR(fmt::format("logand.end_{}", m_AndCounter));
                OpCode idx = STR_IDX(-1);
                m_AndCounter++;
        
                emit_expr(binop.LHS, binop.LHS->ValueKind);
                PUSH_PENDING_OP(OP_JF);
                PUSH_PENDING_OP(idx);
                emit_expr(binop.RHS, binop.RHS->ValueKind);
                PUSH_PENDING_OP(OP_LOGAND);
                PUSH_PENDING_OP(OP_JMP);
                PUSH_PENDING_OP(idx);
        
                PUSH_PENDING_OP(OP_LABEL);
                PUSH_PENDING_OP(idx);
        
                break;
            }
        
            case BinaryOperatorKind::LogOr: {
                ADD_STR(fmt::format("logor.end_{}", m_OrCounter));
                OpCode idx = STR_IDX(-1);
                m_OrCounter++;
        
                emit_expr(binop.LHS, binop.LHS->ValueKind);
                PUSH_PENDING_OP(OP_JT);
                PUSH_PENDING_OP(idx);
                emit_expr(binop.RHS, binop.RHS->ValueKind);
                PUSH_PENDING_OP(OP_LOGOR);
                PUSH_PENDING_OP(OP_JMP);
                PUSH_PENDING_OP(idx);
        
                PUSH_PENDING_OP(OP_LABEL);
                PUSH_PENDING_OP(idx);
        
                break;
            }
        
            case BinaryOperatorKind::Eq: {
                emit_expr(binop.LHS, binop.LHS->ValueKind);
                emit_expr(binop.RHS, binop.RHS->ValueKind);
        
                PUSH_PENDING_OP(OP_ST_ADDR);
                emit_expr(binop.LHS, valueKind);
                break;
            }

            default: ARIA_UNREACHABLE();
        }

        #undef BINOP_INT
        #undef BINOP_INT2
        #undef BINOP
        #undef BINOP2
    }

    void Emitter::emit_CompoundAssign_expr(Expr* expr, ExprValueKind valueKind) {
        CompoundAssignExpr compAss = expr->CompoundAssign;
        
        #define BINOP2(_enum, op) case BinaryOperatorKind::Compound##_enum: { \
            emit_expr(compAss.LHS, ExprValueKind::RValue); \
            emit_expr(compAss.RHS, compAss.RHS->ValueKind); \
            \
            if (compAss.LHS->Type->is_integral()) { \
                if (compAss.LHS->Type->is_signed()) { PUSH_PENDING_OP(OP_##op##I); } \
                if (compAss.LHS->Type->is_unsigned()) { PUSH_PENDING_OP(OP_##op##U); } \
            } else if (compAss.LHS->Type->is_floating_point()) { \
                PUSH_PENDING_OP(OP_##op##F); \
            } \
            break; \
        }

        #define BINOP(_enum, op) BINOP2(_enum, op)

        #define BINOP_INT2(_enum, op) case BinaryOperatorKind::Compound##_enum: { \
            emit_expr(compAss.LHS, ExprValueKind::RValue); \
            emit_expr(compAss.RHS, compAss.RHS->ValueKind); \
            \
            if (compAss.LHS->Type->is_integral()) { \
                if (compAss.LHS->Type->is_signed()) { PUSH_PENDING_OP(OP_##op##I); } \
                if (compAss.LHS->Type->is_unsigned()) { PUSH_PENDING_OP(OP_##op##U); } \
            } \
            break; \
        }

        #define BINOP_INT(_enum, op) BINOP_INT2(_enum, op)

        // Load the destination
        emit_expr(compAss.LHS, ExprValueKind::LValue);
        
        switch (compAss.Operator) {
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
        emit_expr(compAss.LHS, valueKind);

        #undef BINOP_INT
        #undef BINOP_INT2
        #undef BINOP
        #undef BINOP2
    }

    void Emitter::emit_expr(Expr* expr, ExprValueKind valueKind) {
        #define EXPR_CASE(kind) emit_##kind##_expr(expr, valueKind)
        #include "aria/internal/compiler/ast/expr_switch.hpp"
        #undef EXPR_CASE

        if (expr->ResultDiscarded) {
            emit_destructors(m_Temporaries);
            m_Temporaries.clear();

            if (!expr->Type->is_void()) {
                PUSH_PENDING_OP(OP_POP);
            }
        }
    }

    void Emitter::emit_TranslationUnit_decl(Decl* decl) {
        TranslationUnitDecl tu = decl->TranslationUnit;
        
        for (Stmt* stmt : tu.Stmts) {
            emit_stmt(stmt);
        }
    }

    void Emitter::emit_Module_decl(Decl* decl) {}

    void Emitter::emit_Var_decl(Decl* decl) {
        VarDecl varDecl = decl->Var;
        std::string ident = fmt::format("{}::{}", m_ActiveNamespace, varDecl.Identifier);

        PUSH_OP(OP_ALLOCA);
        PUSH_OP(type_info_to_vm_type_idx(varDecl.Type));

        Declaration d;
        d.Type = varDecl.Type;
        
        if (varDecl.Type->is_string()) {
            d.Destructor = Decl::Create(m_Context, SourceLocation(), SourceRange(), DeclKind::BuiltinDestructor, BuiltinDestructorDecl(BuiltinKind::String));
        } else if (varDecl.Type->is_structure()) {
            StructDeclaration& sDecl = varDecl.Type->Struct;
            Decl* dtor = nullptr;

            for (auto& field : sDecl.SourceDecl->Struct.Fields) {
                if (field->Kind == DeclKind::Destructor) {
                    dtor = field;
                }
            }

            if (!sDecl.SourceDecl->Struct.Definition.TrivialDtor) { d.Destructor = dtor; };
        }

        // We want to allocate the variables up front (at the start of the stack frame)
        // This is why we use m_OpCodes instead of m_PendingOpCodes here
        if (varDecl.GlobalVar) {
            ADD_STR(ident);
            PUSH_OP(OP_DECL_GLOBAL);
            PUSH_OP(STR_IDX(-1));

            d.Data = ident;
            m_GlobalScope.DeclaredSymbols.push_back(d);
        } else {
            PUSH_OP(OP_DECL_LOCAL);
            PUSH_OP(static_cast<OpCode>(m_ActiveStackFrame.LocalCount));
            d.Data = m_ActiveStackFrame.LocalCount;
            m_ActiveStackFrame.LocalCount++;
        
            m_ActiveStackFrame.Scopes.back().DeclaredSymbols.push_back(d);
            m_ActiveStackFrame.Scopes.back().DeclaredSymbolMap[ident] = m_ActiveStackFrame.Scopes.back().DeclaredSymbols.size() - 1;
        }
        
        // For initializers we need to just store the value in the already declared variable
        if (varDecl.Initializer) {
            emit_expr(varDecl.Initializer, varDecl.Initializer->ValueKind);

            if (varDecl.GlobalVar) {
                ADD_STR(ident);
                PUSH_PENDING_OP(OP_ST_GLOBAL);
                PUSH_PENDING_OP(STR_IDX(-1));
            } else {
                PUSH_PENDING_OP(OP_ST_LOCAL);
                PUSH_PENDING_OP(static_cast<OpCode>(m_ActiveStackFrame.LocalCount - 1));
            }
        }
    }
    
    void Emitter::emit_Param_decl(Decl* decl) {
        ParamDecl param = decl->Param;
        PUSH_OP(OP_ALLOCA);
        PUSH_OP(type_info_to_vm_type_idx(param.Type));
        PUSH_OP(OP_DECL_LOCAL);
        PUSH_OP(static_cast<OpCode>(m_ActiveStackFrame.LocalCount));
        PUSH_OP(OP_ST_LOCAL);
        PUSH_OP(static_cast<OpCode>(m_ActiveStackFrame.LocalCount));

        m_ActiveStackFrame.Parameters[fmt::format("{}::{}", m_ActiveNamespace, param.Identifier)] = m_ActiveStackFrame.LocalCount++;
    }

    void Emitter::emit_Function_decl(Decl* decl) {
        FunctionDecl& fnDecl = decl->Function;

        std::string sig;
        for (auto& attr : fnDecl.Attributes) {
            if (attr.Kind == FunctionDecl::AttributeKind::Extern) { return; }
            if (attr.Kind == FunctionDecl::AttributeKind::NoMangle) { sig = fmt::format("{}()", fnDecl.Identifier); break; }
        }

        if (sig.empty()) {
            sig = fmt::format("{}::{}", m_ActiveNamespace, mangle_function(&fnDecl));
        }

        if (fnDecl.Body) {
            ADD_STR(sig);
            ADD_STR("_entry$");
            PUSH_OP(OP_FUNCTION);
            PUSH_OP(STR_IDX(-2));
            PUSH_OP(OP_LABEL);
            PUSH_OP(STR_IDX(-1));
            push_stack_frame(sig);
            
            size_t returnSlot = (fnDecl.Type->Function.ReturnType->Kind == TypeKind::Void) ? 0 : 1;
            
            for (Decl* p : fnDecl.Parameters) {
                emit_Param_decl(p);
            }
            
            emit_stmt(fnDecl.Body);
            merge_pending_op_codes();
        
            if (m_OpCodes.Program.back() != OP_RET || m_OpCodes.Program.back() != OP_RET_VAL) {
                PUSH_OP(OP_RET);
            }
        
            pop_stack_frame();
            PUSH_OP(OP_ENDFUNCTION);
        
            CompilerReflectionDeclaration d;
            d.TypeIndex = type_info_to_vm_type_idx(fnDecl.Type->Function.ReturnType);
            d.Kind = ReflectionKind::Function;
        
            m_ReflectionData.Declarations[sig] = d;
        }
    }

    void Emitter::emit_OverloadedFunction_decl(Decl* decl) {}

    void Emitter::emit_Struct_decl(Decl* decl) {
        ARIA_TODO("Emitter::EmitStructDecl()");

        // StructDecl& sDecl = decl->Struct;
        // std::string ident = fmt::format("{}::{}", m_ActiveNamespace, sDecl.Identifier);
        // 
        // RuntimeStructDeclaration sd;
        // sd.Index = m_StructIndex++;
        // 
        // for (Decl* field : sDecl.Fields) {
        //     if (field->Kind == DeclKind::Field) {
        //         sd.FieldIndices[ident] = sd.FieldIndices.size();
        //     }
        // }
        // 
        // VMStruct str;
        // str.Name = ident;
        // str.Fields.reserve(decl->Struct.Fields.Size);
        // 
        // for (Decl* field : decl->Struct.Fields) {
        //     if (field->Kind == DeclKind::Field) {
        //         str.Fields.push_back(type_info_to_vm_type_idx(field->Field.Type));
        //     } else if (field->Kind == DeclKind::Constructor) {
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
        //     } else if (field->Kind == DeclKind::Destructor) {
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

    void Emitter::emit_Field_decl(Decl* decl) { ARIA_UNREACHABLE(); }
    void Emitter::emit_Constructor_decl(Decl* decl) { ARIA_UNREACHABLE(); }
    void Emitter::emit_Destructor_decl(Decl* decl) { ARIA_UNREACHABLE(); }
    void Emitter::emit_Method_decl(Decl* decl) { ARIA_UNREACHABLE(); }
    void Emitter::emit_BuiltinCopyConstructor_decl(Decl* decl) { ARIA_UNREACHABLE(); }
    void Emitter::emit_BuiltinDestructor_decl(Decl* decl) { ARIA_UNREACHABLE(); }

    void Emitter::emit_decl(Decl* decl) {
        #define DECL_CASE(kind) emit_##kind##_decl(decl)
        #include "aria/internal/compiler/ast/decl_switch.hpp"
        #undef DECL_CASE
    }

    void Emitter::emit_Nop_stmt(Stmt* stmt) {}
    void Emitter::emit_Import_stmt(Stmt* stmt) {}

    void Emitter::emit_Block_stmt(Stmt* stmt) {
        BlockStmt block = stmt->Block;
        push_scope();
        
        for (Stmt* stmt : block.Stmts) {
            emit_stmt(stmt);
        }

        pop_scope();
    }

    void Emitter::emit_While_stmt(Stmt* stmt) {
        WhileStmt wh = stmt->While;
        
        ADD_STR(fmt::format("loop.start_{}", m_LoopCounter));
        ADD_STR(fmt::format("loop.end_{}", m_LoopCounter));
        OpCode startIdx = STR_IDX(-2);
        OpCode endIdx = STR_IDX(-1);
        m_LoopCounter++;
        
        PUSH_PENDING_OP(OP_LABEL);
        PUSH_PENDING_OP(startIdx);
        emit_expr(wh.Condition, wh.Condition->ValueKind);
        PUSH_PENDING_OP(OP_JF_POP);
        PUSH_PENDING_OP(endIdx);
        emit_stmt(wh.Body);
        
        PUSH_PENDING_OP(OP_JMP);
        PUSH_PENDING_OP(startIdx);
        PUSH_PENDING_OP(OP_LABEL);
        PUSH_PENDING_OP(endIdx);
    }

    void Emitter::emit_DoWhile_stmt(Stmt* stmt) {
        DoWhileStmt wh = stmt->DoWhile;
        
        ADD_STR(fmt::format("loop.start_{}", m_LoopCounter));
        ADD_STR(fmt::format("loop.end_{}", m_LoopCounter));
        OpCode startIdx = STR_IDX(-2);
        OpCode endIdx = STR_IDX(-1);
        m_LoopCounter++;
        
        PUSH_PENDING_OP(OP_LABEL);
        PUSH_PENDING_OP(startIdx);
        emit_stmt(wh.Body);
        
        emit_expr(wh.Condition, wh.Condition->ValueKind);
        PUSH_PENDING_OP(OP_JT_POP);
        PUSH_PENDING_OP(startIdx);
        PUSH_PENDING_OP(OP_JMP);
        PUSH_PENDING_OP(endIdx);
        
        PUSH_PENDING_OP(OP_LABEL);
        PUSH_PENDING_OP(endIdx);
    }

    void Emitter::emit_For_stmt(Stmt* stmt) {
        ForStmt fs = stmt->For;
        
        ADD_STR(fmt::format("loop.start_{}", m_LoopCounter));
        ADD_STR(fmt::format("loop.end_{}", m_LoopCounter));
        OpCode startIdx = STR_IDX(-2);
        OpCode endIdx = STR_IDX(-1);
        m_LoopCounter++;
        
        push_scope();
        if (fs.Prologue) { emit_decl(fs.Prologue); }
        
        PUSH_PENDING_OP(OP_LABEL);
        PUSH_PENDING_OP(startIdx);
        if (fs.Condition) {
            emit_expr(fs.Condition, fs.Condition->ValueKind);
            PUSH_PENDING_OP(OP_JF_POP);
            PUSH_PENDING_OP(endIdx);
        }
        
        emit_Block_stmt(fs.Body);
            
        if (fs.Step) {
            emit_expr(fs.Step, fs.Step->ValueKind);
        }
        
        PUSH_PENDING_OP(OP_JMP);
        PUSH_PENDING_OP(startIdx);
        PUSH_PENDING_OP(OP_LABEL);
        PUSH_PENDING_OP(endIdx);
        
        pop_scope();
    }

    void Emitter::emit_If_stmt(Stmt* stmt) {
        IfStmt ifs = stmt->If;
        
        ADD_STR(fmt::format("if.body{}", m_LoopCounter));
        ADD_STR(fmt::format("if.end{}", m_LoopCounter));
        OpCode bodyIdx = STR_IDX(-2);
        OpCode endIdx = STR_IDX(-1);
        
        emit_expr(ifs.Condition, ifs.Condition->ValueKind);
        PUSH_PENDING_OP(OP_JT_POP);
        PUSH_PENDING_OP(bodyIdx);
        PUSH_PENDING_OP(OP_JMP);
        PUSH_PENDING_OP(endIdx);
        
        PUSH_PENDING_OP(OP_LABEL);
        PUSH_PENDING_OP(bodyIdx);
        emit_stmt(ifs.Body);
        PUSH_PENDING_OP(OP_LABEL);
        PUSH_PENDING_OP(endIdx);
        
        m_IfCounter++;
    }

    void Emitter::emit_Break_stmt(Stmt* stmt) {
        ADD_STR(fmt::format("loop.end_{}", m_LoopCounter - 1));
        PUSH_PENDING_OP(OP_JMP);
        PUSH_PENDING_OP(STR_IDX(-1));
    }

    void Emitter::emit_Continue_stmt(Stmt* stmt) {
        ADD_STR(fmt::format("loop.start_{}", m_LoopCounter - 1));
        PUSH_PENDING_OP(OP_JMP);
        PUSH_PENDING_OP(STR_IDX(-1));
    }

    void Emitter::emit_Return_stmt(Stmt* stmt) {
        ReturnStmt ret = stmt->Return;
        if (ret.Value) {
            emit_expr(ret.Value, ret.Value->ValueKind);
            PUSH_PENDING_OP(OP_RET_VAL);
            return;
        }
        
        PUSH_PENDING_OP(OP_RET);
    }

    void Emitter::emit_Expr_stmt(Stmt* stmt) {
        emit_expr(stmt->ExprStmt, stmt->ExprStmt->ValueKind);
    }

    void Emitter::emit_Decl_stmt(Stmt* stmt) {
        emit_decl(stmt->DeclStmt);
    }

    void Emitter::emit_stmt(Stmt* stmt) {
        #define STMT_CASE(kind) emit_##kind##_stmt(stmt)
        #include "aria/internal/compiler/ast/stmt_switch.hpp"
        #undef STMT_CASE
    }

    void Emitter::emit_destructors(const std::vector<Declaration>& declarations) {
        for (auto it = declarations.rbegin(); it != declarations.rend(); it++) {
            auto& decl = *it;
        
            if (decl.Destructor) {
                if (decl.Destructor->Kind == DeclKind::BuiltinDestructor) {
                    switch (decl.Destructor->BuiltinDestructor.Kind) {
                        case BuiltinKind::String: ADD_STR("__aria_destruct_str()"); break;
                        default: ARIA_UNREACHABLE();
                    }
                } else if (decl.Destructor->Kind == DeclKind::Destructor) {
                    ADD_STR(fmt::format("{}::<dtor>()", type_info_to_string(decl.Type)));
                }

                OpCode idx = STR_IDX(-1);

                if (std::holds_alternative<size_t>(decl.Data)) {
                    PUSH_PENDING_OP(OP_LD_PTR_LOCAL);
                    PUSH_PENDING_OP(static_cast<OpCode>(std::get<size_t>(decl.Data)));
                } else if (std::holds_alternative<std::string>(decl.Data)) {
                    ADD_STR(std::get<std::string>(decl.Data));
                    PUSH_PENDING_OP(OP_LD_PTR_GLOBAL);
                    PUSH_PENDING_OP(STR_IDX(-1));
                }
        
                PUSH_PENDING_OP(OP_CALL);
                PUSH_PENDING_OP(idx);
            }
        }
    }

    void Emitter::push_stack_frame(const std::string& name) {
        m_ActiveStackFrame.Scopes.clear();
        m_ActiveStackFrame.Name = name;
    }

    void Emitter::pop_stack_frame() {
        m_ActiveStackFrame.Scopes.clear();
        m_ActiveStackFrame.Name.clear();
        m_ActiveStackFrame.Parameters.clear();
        m_ActiveStackFrame.LocalCount = 0;

        // Reset counters
        m_AndCounter = 0;
        m_OrCounter = 0;
        m_LoopCounter = 0;
        m_IfCounter = 0;
    }

    void Emitter::push_scope() {
        m_ActiveStackFrame.Scopes.emplace_back();
    }

    void Emitter::pop_scope() {
        emit_destructors(m_ActiveStackFrame.Scopes.back().DeclaredSymbols);
        m_ActiveStackFrame.Scopes.pop_back();
    }

    void Emitter::merge_pending_op_codes() {
        m_OpCodes.Program.reserve(m_OpCodes.Program.size() + m_PendingOpCodes.size());
        m_OpCodes.Program.insert(m_OpCodes.Program.end(), m_PendingOpCodes.begin(), m_PendingOpCodes.end());
        m_PendingOpCodes.clear();
    }

    OpCode Emitter::type_info_to_vm_type_idx(TypeInfo* t) {
        if (t->is_primitive()) {
            return static_cast<OpCode>(m_BasicTypes[t->Kind]);
        }

        if (t->is_structure()) {
            return static_cast<OpCode>(m_Structs.at(t->Struct.SourceDecl).Index);
        }

        ARIA_UNREACHABLE();
    }

    std::string Emitter::mangle_function(FunctionDecl* fn) {
        std::string ident = fmt::format("{}(", fn->Identifier);

        for (size_t i = 0; i < fn->Parameters.Size; i++) {
            ident += type_info_to_string(fn->Parameters.Items[i]->Param.Type);

            if (i != fn->Parameters.Size - 1) {
                ident += ", ";
            }
        }

        ident += ")";

        return ident;
    }

} // namespace Aria::Internal
