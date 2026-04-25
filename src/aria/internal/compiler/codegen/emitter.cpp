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

        EmitImpl();
    }

    void Emitter::EmitImpl() {
        AddBasicTypes();
        AddUserDefinedTypes();
        EmitDeclarations();
        EmitStartEnd();

        m_Context->Ops = m_OpCodes;
        m_Context->ReflectionData = m_ReflectionData;
    }

    void Emitter::AddBasicTypes() {
        #define ADD_TYPE(vmType, primType) do { m_OpCodes.TypeTable.emplace_back(VMTypeKind::vmType); m_BasicTypes[PrimitiveType::primType] = static_cast<u16>(i); i++; } while (0)

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
        ADD_TYPE(String, String);
        ADD_TYPE(Ptr, Ptr);

        #undef ADD_TYPE
    }

    void Emitter::AddUserDefinedTypes() {
        m_StructIndex = static_cast<u16>(m_BasicTypes.size());

        for (Module* mod : m_Context->Modules) {
            if (mod->Units.size() == 0) { continue; }
            m_ActiveNamespace = mod->Name;

            for (CompilationUnit* unit : mod->Units) {
                for (Decl* s : unit->Structs) {
                    EmitDecl(s);
                }
            }
        }
    }

    void Emitter::EmitDeclarations() {
        for (Module* mod : m_Context->Modules) {
            if (mod->Units.size() == 0) { continue; }
            m_ActiveNamespace = mod->Name;

            for (CompilationUnit* unit : mod->Units) {
                for (Decl* f : unit->Funcs) {
                    EmitDecl(f);
                }
            }

            // Global variable initializion requires special functions
            std::string startSig = fmt::format("_start${}()", mod->Name);
            std::string endSig = fmt::format("_end${}()", mod->Name);

            ADD_STR(startSig);
            ADD_STR("_entry$");
            PUSH_OP(OP_FUNCTION);
            PUSH_OP(STR_IDX(-2));
            PUSH_OP(OP_LABEL);
            PUSH_OP(STR_IDX(-1));
            PushStackFrame(startSig);

            for (CompilationUnit* unit : mod->Units) {
                for (Decl* g : unit->Globals) {
                    EmitDecl(g);
                }
            }

            MergePendingOpCodes();
            PopStackFrame();
            PUSH_OP(OP_RET);
            PUSH_OP(OP_ENDFUNCTION);

            // Global variable destruction
            ADD_STR(endSig);
            ADD_STR("_entry$");
            PUSH_OP(OP_FUNCTION);
            PUSH_OP(STR_IDX(-2));
            PUSH_OP(OP_LABEL);
            PUSH_OP(STR_IDX(-1));
            PushStackFrame(endSig);

            EmitDestructors(m_GlobalScope.DeclaredSymbols);
            MergePendingOpCodes();

            PopStackFrame();
            PUSH_OP(OP_RET);
            PUSH_OP(OP_ENDFUNCTION);

            mod->Ops = m_OpCodes;
            mod->ReflectionData = m_ReflectionData;
        }
    }

    void Emitter::EmitStartEnd() {
        ADD_STR("_start$()");
        ADD_STR("_entry$");
        PUSH_OP(OP_FUNCTION);
        PUSH_OP(STR_IDX(-2));
        PUSH_OP(OP_LABEL);
        PUSH_OP(STR_IDX(-1));
        PushStackFrame("_start$()");

        for (Module* mod : m_Context->Modules) {
            if (mod->Units.size() == 0) { continue; }
            ADD_STR(fmt::format("_start${}()", mod->Name));
            PUSH_OP(OP_CALL);
            PUSH_OP(STR_IDX(-1));
        }

        PopStackFrame();
        PUSH_OP(OP_RET);
        PUSH_OP(OP_ENDFUNCTION);

        ADD_STR("_end$()");
        ADD_STR("_entry$");
        PUSH_OP(OP_FUNCTION);
        PUSH_OP(STR_IDX(-2));
        PUSH_OP(OP_LABEL);
        PUSH_OP(STR_IDX(-1));
        PushStackFrame("_end$()");

        for (Module* mod : m_Context->Modules) {
            if (mod->Units.size() == 0) { continue; }
            ADD_STR(fmt::format("_end${}()", mod->Name));
            PUSH_OP(OP_CALL);
            PUSH_OP(STR_IDX(-1));
        }

        PopStackFrame();
        PUSH_OP(OP_RET);
        PUSH_OP(OP_ENDFUNCTION);
    }

    void Emitter::EmitBooleanConstantExpr(Expr* expr, ExprValueKind valueKind) {
        BooleanConstantExpr bc = expr->BooleanConstant;
        ADD_CONSTANT(static_cast<u64>(bc.Value));
        PUSH_PENDING_OP(OP_LD_CONST);
        PUSH_PENDING_OP(TypeInfoToVMTypeIdx(expr->Type));
        PUSH_PENDING_OP(CONST_IDX(-1));
    }

    void Emitter::EmitCharacterConstantExpr(Expr* expr, ExprValueKind valueKind) {
        CharacterConstantExpr cc = expr->CharacterConstant;
        ADD_CONSTANT(static_cast<u64>(cc.Value));
        PUSH_PENDING_OP(OP_LD_CONST);
        PUSH_PENDING_OP(TypeInfoToVMTypeIdx(expr->Type));
        PUSH_PENDING_OP(CONST_IDX(-1));
    }

    void Emitter::EmitIntegerConstantExpr(Expr* expr,ExprValueKind valueKind) {
        IntegerConstantExpr ic = expr->IntegerConstant;
        ADD_CONSTANT(static_cast<u64>(ic.Value));
        PUSH_PENDING_OP(OP_LD_CONST);
        PUSH_PENDING_OP(TypeInfoToVMTypeIdx(expr->Type));
        PUSH_PENDING_OP(CONST_IDX(-1));
    }

    void Emitter::EmitFloatingConstantExpr(Expr* expr, ExprValueKind valueKind) {
        FloatingConstantExpr fc = expr->FloatingConstant;
        PUSH_PENDING_OP(OP_LD_CONST);
        PUSH_PENDING_OP(TypeInfoToVMTypeIdx(expr->Type));

        switch (expr->Type->Type) {
            case PrimitiveType::Float: ADD_CONSTANT(static_cast<float>(fc.Value)); break;
            case PrimitiveType::Double: ADD_CONSTANT(static_cast<double>(fc.Value)); break;

            default: ARIA_UNREACHABLE();
        }
        
        PUSH_PENDING_OP(CONST_IDX(-1));
    }

    void Emitter::EmitStringConstantExpr(Expr* expr, ExprValueKind valueKind) {
        StringConstantExpr sc = expr->StringConstant;
        ADD_STR(fmt::format("{}", sc.Value));
        PUSH_PENDING_OP(OP_LD_STR);
        PUSH_PENDING_OP(STR_IDX(-1));
    }

    // List of op codes this function can emit:
    // 
    // lvalue: LdPtrLocal, LdPtrArg, LdPtrGlobal, LdFunc
    // lvalue + ref: LdLocal, LdArg, LdGlobal
    // 
    // rvalue: LdLocal, LdArg, LdGlobal
    // rvalue + ref: (LdLocal, LdArg, LdGlobal) + deref
    void Emitter::EmitDeclRefExpr(Expr* expr, ExprValueKind valueKind) {
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
                    if (expr->Type->IsReference()) {
                        PUSH_PENDING_OP(OP_LD_GLOBAL);
                    } else {
                        PUSH_PENDING_OP(OP_LD_PTR_GLOBAL);
                    }

                    PUSH_PENDING_OP(STR_IDX(-1));
                } else if (valueKind == ExprValueKind::RValue) {
                    PUSH_PENDING_OP(OP_LD_GLOBAL);
                    PUSH_PENDING_OP(STR_IDX(-1));
        
                    if (expr->Type->IsReference()) {
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
                        if (expr->Type->IsReference()) {
                            PUSH_PENDING_OP(OP_LD_LOCAL);
                        } else {
                            PUSH_PENDING_OP(OP_LD_PTR_LOCAL);
                        }

                        PUSH_PENDING_OP(static_cast<OpCode>(std::get<size_t>(decl.Data)));
                    } else if (valueKind == ExprValueKind::RValue) {
                        PUSH_PENDING_OP(OP_LD_LOCAL);
                        PUSH_PENDING_OP(static_cast<OpCode>(std::get<size_t>(decl.Data)));
        
                        if (expr->Type->IsReference()) {
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
                if (expr->Type->IsReference()) {
                    PUSH_PENDING_OP(OP_LD_LOCAL);
                } else {
                    PUSH_PENDING_OP(OP_LD_PTR_LOCAL);
                }

                PUSH_PENDING_OP(static_cast<OpCode>(m_ActiveStackFrame.Parameters.at(ident)));
            } else if (valueKind == ExprValueKind::RValue) {
                PUSH_PENDING_OP(OP_LD_LOCAL);
                PUSH_PENDING_OP(static_cast<OpCode>(m_ActiveStackFrame.Parameters.at(ident)));
        
                if (expr->Type->IsReference()) {
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

    void Emitter::EmitMemberExpr(Expr* expr, ExprValueKind valueKind) {
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
        //             EmitExpr(mem->GetParent(), ExprValueKind::LValue);
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

    void Emitter::EmitSelfExpr(Expr* expr, ExprValueKind valueKind) {
        ARIA_TODO("Emitter::EmitSelfExpr()");
    }

    void Emitter::EmitTemporaryExpr(Expr* expr, ExprValueKind valueKind) {
        TemporaryExpr& temp = expr->Temporary;
        
        EmitExpr(temp.Expression, valueKind);
        
        // Create a new temporary
        PUSH_PENDING_OP(OP_DECL_LOCAL);
        PUSH_PENDING_OP(static_cast<OpCode>(m_ActiveStackFrame.LocalCount));
        PUSH_PENDING_OP(OP_LD_LOCAL);
        PUSH_PENDING_OP(static_cast<OpCode>(m_ActiveStackFrame.LocalCount));
        
        Declaration d;
        d.Type = temp.Expression->Type;
        d.Data = m_ActiveStackFrame.LocalCount;
        d.Destructor = temp.Destructor;
        m_ActiveStackFrame.LocalCount++;
        
        m_Temporaries.push_back(d);
    }

    void Emitter::EmitCopyExpr(Expr* expr, ExprValueKind valueKind) {
        CopyExpr copy = expr->Copy;
        
        if (copy.Constructor->Kind == DeclKind::BuiltinCopyConstructor) {
            switch (copy.Constructor->BuiltinCopyConstructor.Kind) {
                case BuiltinKind::String: ADD_STR("__aria_copy_str()"); break;
                default: ARIA_UNREACHABLE();
            }
        }

        OpCode idx = STR_IDX(-1);

        EmitExpr(copy.Expression, ExprValueKind::LValue);
        PUSH_PENDING_OP(OP_CALL);
        PUSH_PENDING_OP(idx);
    }

    void Emitter::EmitCallExpr(Expr* expr, ExprValueKind valueKind) {
        CallExpr call = expr->Call;
        
        ARIA_ASSERT(call.Callee->Kind == ExprKind::DeclRef, "Callee of call expression must be a decl-ref");

        for (auto it = call.Arguments.rbegin(); it != call.Arguments.rend(); it--) {
            EmitExpr(*it, (*it)->ValueKind);
        }

        std::string ident = (call.Callee->DeclRef.ReferencedDecl->Flags & DECL_FLAG_NOMANGLE) ? 
            fmt::format("{}()", call.Callee->DeclRef.Identifier) : 
            fmt::format("{}::{}", (call.Callee->DeclRef.NameSpecifier) ? 
                call.Callee->DeclRef.NameSpecifier->Scope.Identifier : m_ActiveNamespace, MangleFunction(&call.Callee->DeclRef.ReferencedDecl->Function));

        ADD_STR(ident);
        PUSH_PENDING_OP(OP_CALL);
        PUSH_PENDING_OP(STR_IDX(-1));
    }

    void Emitter::EmitConstructExpr(Expr* expr, ExprValueKind valueKind) {
        ARIA_TODO("Emitter::EmitConstructExpr()");
        // ConstructExpr& ct = expr->Construct;
        // 
        // PUSH_OP(OpCodeKind::Alloca, { TypeInfoToVMTypeIdx(expr->Type) });
        // PUSH_OP(OpCodeKind::DeclareLocal, { m_ActiveStackFrame.LocalCount });
        // ADD_STR(fmt::format("{}::<ctor>()", TypeInfoToString(expr->Type)));
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

    void Emitter::EmitMethodCallExpr(Expr* expr, ExprValueKind valueKind) {
        ARIA_TODO("Emitter::EmitMethodCallExpr()");
        // MethodCallExpr* call = GetNode<MethodCallExpr>(expr);
        // 
        // EmitExpr(call->GetCallee(), call->GetCallee()->GetValueKind());
        // 
        // // Push self
        // EmitExpr(call->GetCallee()->GetParent(), ExprValueKind::LValue);
        // PUSH_PENDING_OP(OpCodeKind::DeclareArg, static_cast<size_t>(0));
        // 
        // for (size_t i = 0; i < call->GetArguments().Size; i++) {
        //     EmitExpr(call->GetArguments().Items[i], call->GetArguments().Items[i]->GetValueKind());
        //     PUSH_PENDING_OP(OpCodeKind::DeclareArg, i + 1);
        // }
        // 
        // size_t retCount = 0;
        // TypeInfo* retType = call->GetResolvedType();
        // if (retType->Type != PrimitiveType::Void) {
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
        //     if (retType->IsReference()) {
        //         retType->Reference = false;
        //         PUSH_PENDING_OP(OpCodeKind::Deref, TypeInfoToVMType(retType));
        //         retType->Reference = true;
        //     }
        // }
    }

    void Emitter::EmitFormatExpr(Expr* expr, ExprValueKind valueKind) {
        FormatExpr format = expr->Format;
        StringView fStr = format.Args.Items[0]->Temporary.Expression->StringConstant.Value;
        
        PUSH_OP(OP_ALLOCA);
        PUSH_OP(TypeInfoToVMTypeIdx(expr->Type));
        PUSH_OP(OP_DECL_LOCAL);
        PUSH_OP(static_cast<OpCode>(m_ActiveStackFrame.LocalCount));
        size_t idx = m_ActiveStackFrame.LocalCount++;
        
        std::string start;
        size_t firstFormat = 0;
        
        for (size_t i = 0; i < fStr.Size(); i++) {
            if (fStr.At(i) == '{') { firstFormat = i; break; }
            start += fStr.At(i);
        }
        
        ADD_STR(start);
        PUSH_PENDING_OP(OP_LD_STR);
        PUSH_PENDING_OP(STR_IDX(-1));
        PUSH_PENDING_OP(OP_ST_LOCAL);
        PUSH_PENDING_OP(static_cast<OpCode>(idx));
        start.clear();

        size_t currentArg = 0;

        for (size_t i = firstFormat; i < fStr.Size(); i++) {
            if (fStr.At(i) == '{') {
                if (!start.empty()) {
                    ADD_STR(start);
                    PUSH_PENDING_OP(OP_LD_STR);
                    PUSH_PENDING_OP(STR_IDX(-1));
                    
                    PUSH_PENDING_OP(OP_LD_PTR_LOCAL);
                    PUSH_PENDING_OP(static_cast<OpCode>(idx));
                    
                    ADD_STR("__aria_append_str()");
                    PUSH_PENDING_OP(OP_CALL);
                    PUSH_PENDING_OP(STR_IDX(-1));

                    start.clear();
                }

                i++;

                EmitExpr(format.Args.Items[currentArg + 1], format.Args.Items[currentArg + 1]->ValueKind);
                PUSH_PENDING_OP(OP_LD_PTR_LOCAL);
                PUSH_PENDING_OP(static_cast<OpCode>(idx));
                
                ARIA_ASSERT(format.Args.Items[currentArg + 1]->Type->IsString(), "Not supported yet");
                ADD_STR("__aria_append_str()");
                PUSH_PENDING_OP(OP_CALL);
                PUSH_PENDING_OP(STR_IDX(-1));

                currentArg++;
            } else {
                start += fStr.At(i);
            }
        }
        
        PUSH_PENDING_OP(OP_LD_LOCAL);
        PUSH_PENDING_OP(static_cast<OpCode>(idx));
    }

    void Emitter::EmitParenExpr(Expr* expr, ExprValueKind valueKind) {
        ParenExpr paren = expr->Paren;
        EmitExpr(paren.Expression, expr->ValueKind);
    }

    void Emitter::EmitImplicitCastExpr(Expr* expr, ExprValueKind valueKind) {
        ImplicitCastExpr cast = expr->ImplicitCast;
        
        if (cast.Kind == CastKind::LValueToRValue) {
            return EmitExpr(cast.Expression, ExprValueKind::RValue);
        } else {
            EmitExpr(cast.Expression, cast.Expression->ValueKind);

            switch (cast.Kind) {
                case CastKind::Integral: {
                    PUSH_PENDING_OP(OP_CONV_ITOI);
                    PUSH_PENDING_OP(TypeInfoToVMTypeIdx(expr->Type));
                    break;
                }

                case CastKind::Floating: {
                    PUSH_PENDING_OP(OP_CONV_FTOF);
                    PUSH_PENDING_OP(TypeInfoToVMTypeIdx(expr->Type));
                    break;
                }

                case CastKind::IntegralToFloating: {
                    PUSH_PENDING_OP(OP_CONV_ITOF);
                    PUSH_PENDING_OP(TypeInfoToVMTypeIdx(expr->Type));
                    break;
                }

                case CastKind::FloatingToIntegral: {
                    PUSH_PENDING_OP(OP_CONV_FTOI);
                    PUSH_PENDING_OP(TypeInfoToVMTypeIdx(expr->Type));
                    break;
                }

                default: ARIA_UNREACHABLE(); break;
            }

            return;
        }
        
        ARIA_UNREACHABLE();
    }

    void Emitter::EmitCastExpr(Expr* expr, ExprValueKind valueKind) {
        CastExpr cast = expr->Cast;
        EmitExpr(cast.Expression, cast.Expression->ValueKind);
    }

    void Emitter::EmitUnaryOperatorExpr(Expr* expr, ExprValueKind valueKind) {
        UnaryOperatorExpr unop = expr->UnaryOperator;
        
        switch (unop.Operator) {
            case UnaryOperatorKind::Negate: { 
                EmitExpr(unop.Expression, unop.Expression->ValueKind);
                if (expr->Type->IsIntegral()) {
                    ARIA_ASSERT(expr->Type->IsSigned(), "Type must be signed");
                    PUSH_PENDING_OP(OP_NEGI);
                } else if (expr->Type->IsFloatingPoint()) {
                    PUSH_PENDING_OP(OP_NEGF);
                } else {
                    ARIA_UNREACHABLE();
                }

                break;
            }
            default: ARIA_UNREACHABLE();
        }
    }

    void Emitter::EmitBinaryOperatorExpr(Expr* expr, ExprValueKind valueKind) {
        BinaryOperatorExpr binop = expr->BinaryOperator;
        
        #define BINOP2(_enum, op) case BinaryOperatorKind::_enum: { \
            EmitExpr(binop.LHS, binop.LHS->ValueKind); \
            EmitExpr(binop.RHS, binop.RHS->ValueKind); \
            \
            if (binop.LHS->Type->IsIntegral()) { \
                if (binop.LHS->Type->IsSigned()) { PUSH_PENDING_OP(OP_##op##I); } \
                if (binop.LHS->Type->IsUnsigned()) { PUSH_PENDING_OP(OP_##op##U); } \
            } else if (binop.LHS->Type->IsFloatingPoint()) { \
                PUSH_PENDING_OP(OP_##op##F); \
            } \
            break; \
        }

        #define BINOP(_enum, op) BINOP2(_enum, op)

        #define BINOP_INT2(_enum, op) case BinaryOperatorKind::_enum: { \
            EmitExpr(binop.LHS, binop.LHS->ValueKind); \
            EmitExpr(binop.RHS, binop.RHS->ValueKind); \
            \
            if (binop.LHS->Type->IsIntegral()) { \
                if (binop.LHS->Type->IsSigned()) { PUSH_PENDING_OP(OP_##op##I); } \
                if (binop.LHS->Type->IsUnsigned()) { PUSH_PENDING_OP(OP_##op##U); } \
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
        
                EmitExpr(binop.LHS, binop.LHS->ValueKind);
                PUSH_PENDING_OP(OP_JF);
                PUSH_PENDING_OP(idx);
                EmitExpr(binop.RHS, binop.RHS->ValueKind);
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
        
                EmitExpr(binop.LHS, binop.LHS->ValueKind);
                PUSH_PENDING_OP(OP_JT);
                PUSH_PENDING_OP(idx);
                EmitExpr(binop.RHS, binop.RHS->ValueKind);
                PUSH_PENDING_OP(OP_LOGOR);
                PUSH_PENDING_OP(OP_JMP);
                PUSH_PENDING_OP(idx);
        
                PUSH_PENDING_OP(OP_LABEL);
                PUSH_PENDING_OP(idx);
        
                break;
            }
        
            case BinaryOperatorKind::Eq: {
                EmitExpr(binop.LHS, binop.LHS->ValueKind);
                EmitExpr(binop.RHS, binop.RHS->ValueKind);
        
                PUSH_PENDING_OP(OP_ST_ADDR);
                EmitExpr(binop.LHS, valueKind);
                break;
            }

            default: ARIA_UNREACHABLE();
        }

        #undef BINOP_INT
        #undef BINOP_INT2
        #undef BINOP
        #undef BINOP2
    }

    void Emitter::EmitCompoundAssignExpr(Expr* expr, ExprValueKind valueKind) {
        CompoundAssignExpr compAss = expr->CompoundAssign;
        
        #define BINOP2(_enum, op) case BinaryOperatorKind::Compound##_enum: { \
            EmitExpr(compAss.LHS, ExprValueKind::RValue); \
            EmitExpr(compAss.RHS, compAss.RHS->ValueKind); \
            \
            if (compAss.LHS->Type->IsIntegral()) { \
                if (compAss.LHS->Type->IsSigned()) { PUSH_PENDING_OP(OP_##op##I); } \
                if (compAss.LHS->Type->IsUnsigned()) { PUSH_PENDING_OP(OP_##op##U); } \
            } else if (compAss.LHS->Type->IsFloatingPoint()) { \
                PUSH_PENDING_OP(OP_##op##F); \
            } \
            break; \
        }

        #define BINOP(_enum, op) BINOP2(_enum, op)

        #define BINOP_INT2(_enum, op) case BinaryOperatorKind::Compound##_enum: { \
            EmitExpr(compAss.LHS, ExprValueKind::RValue); \
            EmitExpr(compAss.RHS, compAss.RHS->ValueKind); \
            \
            if (compAss.LHS->Type->IsIntegral()) { \
                if (compAss.LHS->Type->IsSigned()) { PUSH_PENDING_OP(OP_##op##I); } \
                if (compAss.LHS->Type->IsUnsigned()) { PUSH_PENDING_OP(OP_##op##U); } \
            } \
            break; \
        }

        #define BINOP_INT(_enum, op) BINOP_INT2(_enum, op)

        // Load the destination
        EmitExpr(compAss.LHS, ExprValueKind::LValue);
        
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
        EmitExpr(compAss.LHS, valueKind);

        #undef BINOP_INT
        #undef BINOP_INT2
        #undef BINOP
        #undef BINOP2
    }

    void Emitter::EmitExpr(Expr* expr, ExprValueKind valueKind) {
        if (expr->Kind == ExprKind::BooleanConstant) {
            EmitBooleanConstantExpr(expr, valueKind);
        } else if (expr->Kind == ExprKind::CharacterConstant) {
            EmitCharacterConstantExpr(expr, valueKind);
        } else if (expr->Kind == ExprKind::IntegerConstant) {
            EmitIntegerConstantExpr(expr, valueKind);
        } else if (expr->Kind == ExprKind::FloatingConstant) {
            EmitFloatingConstantExpr(expr, valueKind);
        } else if (expr->Kind == ExprKind::StringConstant) {
            EmitStringConstantExpr(expr, valueKind);
        } else if (expr->Kind == ExprKind::DeclRef) {
            EmitDeclRefExpr(expr, valueKind);
        } else if (expr->Kind == ExprKind::Member) {
            EmitMemberExpr(expr, valueKind);
        } else if (expr->Kind == ExprKind::Temporary) {
            EmitTemporaryExpr(expr, valueKind);
        } else if (expr->Kind == ExprKind::Copy) {
            EmitCopyExpr(expr, valueKind);
        } else if (expr->Kind == ExprKind::Call) {
            EmitCallExpr(expr, valueKind);
        } else if (expr->Kind == ExprKind::Construct) {
            EmitConstructExpr(expr, valueKind);
        } else if (expr->Kind == ExprKind::MethodCall) {
            EmitMethodCallExpr(expr, valueKind);
        } else if (expr->Kind == ExprKind::Format) {
            EmitFormatExpr(expr, valueKind);
        } else if (expr->Kind == ExprKind::Paren) {
            EmitParenExpr(expr, valueKind);
        } else if (expr->Kind == ExprKind::Cast) {
            EmitCastExpr(expr, valueKind);
        } else if (expr->Kind == ExprKind::ImplicitCast) {
            EmitImplicitCastExpr(expr, valueKind);
        } else if (expr->Kind == ExprKind::UnaryOperator) {
            EmitUnaryOperatorExpr(expr, valueKind);
        } else if (expr->Kind == ExprKind::BinaryOperator) {
            EmitBinaryOperatorExpr(expr, valueKind);
        } else if (expr->Kind == ExprKind::CompoundAssign) {
            EmitCompoundAssignExpr(expr, valueKind);
        }

        if (expr->ResultDiscarded) {
            EmitDestructors(m_Temporaries);
            m_Temporaries.clear();

            if (!expr->Type->IsVoid()) {
                PUSH_PENDING_OP(OP_POP);
            }
        }
    }

    void Emitter::EmitTranslationUnitDecl(Decl* decl) {
        TranslationUnitDecl tu = decl->TranslationUnit;
        
        for (Stmt* stmt : tu.Stmts) {
            EmitStmt(stmt);
        }
    }

    void Emitter::EmitVarDecl(Decl* decl) {
        VarDecl varDecl = decl->Var;
        std::string ident = fmt::format("{}::{}", m_ActiveNamespace, varDecl.Identifier);

        PUSH_OP(OP_ALLOCA);
        PUSH_OP(TypeInfoToVMTypeIdx(varDecl.Type));

        Declaration d;
        d.Type = varDecl.Type;
        
        if (varDecl.Type->Type == PrimitiveType::String) {
            d.Destructor = Decl::Create(m_Context, SourceLocation(), SourceRange(), DeclKind::BuiltinDestructor, 0, BuiltinDestructorDecl(BuiltinKind::String));
        } else if (varDecl.Type->Type == PrimitiveType::Structure) {
            StructDeclaration& sDecl = std::get<StructDeclaration>(varDecl.Type->Data);
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
            EmitExpr(varDecl.Initializer, varDecl.Initializer->ValueKind);

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
    
    void Emitter::EmitParamDecl(Decl* decl) {
        ParamDecl param = decl->Param;
        PUSH_OP(OP_ALLOCA);
        PUSH_OP(TypeInfoToVMTypeIdx(param.Type));
        PUSH_OP(OP_DECL_LOCAL);
        PUSH_OP(static_cast<OpCode>(m_ActiveStackFrame.LocalCount));
        PUSH_OP(OP_ST_LOCAL);
        PUSH_OP(static_cast<OpCode>(m_ActiveStackFrame.LocalCount));

        m_ActiveStackFrame.Parameters[fmt::format("{}::{}", m_ActiveNamespace, param.Identifier)] = m_ActiveStackFrame.LocalCount++;
    }

    void Emitter::EmitFunctionDecl(Decl* decl) {
        FunctionDecl& fnDecl = decl->Function;
        std::string name = (decl->Flags & DECL_FLAG_NOMANGLE) ? fmt::format("{}()", fnDecl.Identifier) : fmt::format("{}::{}", m_ActiveNamespace, MangleFunction(&fnDecl));

        if (fnDecl.Body) {
            ADD_STR(name);
            ADD_STR("_entry$");
            PUSH_OP(OP_FUNCTION);
            PUSH_OP(STR_IDX(-2));
            PUSH_OP(OP_LABEL);
            PUSH_OP(STR_IDX(-1));
            PushStackFrame(name);
            
            size_t returnSlot = (std::get<FunctionDeclaration>(fnDecl.Type->Data).ReturnType->Type == PrimitiveType::Void) ? 0 : 1;
            
            for (Decl* p : fnDecl.Parameters) {
                EmitParamDecl(p);
            }
            
            EmitStmt(fnDecl.Body);
            MergePendingOpCodes();
        
            if (m_OpCodes.Program.back() != OP_RET || m_OpCodes.Program.back() != OP_RET_VAL) {
                PUSH_OP(OP_RET);
            }
        
            PopStackFrame();
            PUSH_OP(OP_ENDFUNCTION);
        
            CompilerReflectionDeclaration d;
            d.TypeIndex = TypeInfoToVMTypeIdx(std::get<FunctionDeclaration>(fnDecl.Type->Data).ReturnType);
            d.Kind = ReflectionKind::Function;
        
            m_ReflectionData.Declarations[name] = d;
        }
    }

    void Emitter::EmitStructDecl(Decl* decl) {
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
        //         str.Fields.push_back(TypeInfoToVMTypeIdx(field->Field.Type));
        //     } else if (field->Kind == DeclKind::Constructor) {
        //         ADD_STR(fmt::format("struct {}::<ctor>()", sDecl.Identifier));
        //         ADD_STR("_entry$");
        //         PUSH_OP(OpCodeKind::Function, { STR_IDX(-2) });
        //         PUSH_OP(OpCodeKind::Label, { STR_IDX(-1) });
        //         PUSH_OP(OpCodeKind::PushSF);
        // 
        //         EmitStmt(field->Constructor.Body);
        //         MergePendingOpCodes();
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
        //         EmitStmt(field->Destructor.Body);
        //         MergePendingOpCodes();
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

    void Emitter::EmitDecl(Decl* decl) {
        if (decl->Kind == DeclKind::TranslationUnit) {
            return EmitTranslationUnitDecl(decl);
        } else if (decl->Kind == DeclKind::Module) {
            return;
        } else if (decl->Kind == DeclKind::Var) {
            return EmitVarDecl(decl);
        } else if (decl->Kind == DeclKind::Param) {
            return EmitParamDecl(decl);
        } else if (decl->Kind == DeclKind::Function) {
            return EmitFunctionDecl(decl);
        } else if (decl->Kind == DeclKind::Struct) {
            return EmitStructDecl(decl);
        }

        ARIA_UNREACHABLE();
    }

    void Emitter::EmitBlockStmt(Stmt* stmt) {
        BlockStmt block = stmt->Block;
        
        for (Stmt* stmt : block.Stmts) {
            EmitStmt(stmt);
        }
    }

    void Emitter::EmitWhileStmt(Stmt* stmt) {
        WhileStmt wh = stmt->While;
        
        ADD_STR(fmt::format("loop.start_{}", m_LoopCounter));
        ADD_STR(fmt::format("loop.end_{}", m_LoopCounter));
        OpCode startIdx = STR_IDX(-2);
        OpCode endIdx = STR_IDX(-1);
        m_LoopCounter++;
        
        PUSH_PENDING_OP(OP_LABEL);
        PUSH_PENDING_OP(startIdx);
        EmitExpr(wh.Condition, wh.Condition->ValueKind);
        PUSH_PENDING_OP(OP_JF_POP);
        PUSH_PENDING_OP(endIdx);
        EmitStmt(wh.Body);
        
        PUSH_PENDING_OP(OP_JMP);
        PUSH_PENDING_OP(startIdx);
        PUSH_PENDING_OP(OP_LABEL);
        PUSH_PENDING_OP(endIdx);
    }

    void Emitter::EmitDoWhileStmt(Stmt* stmt) {
        DoWhileStmt wh = stmt->DoWhile;
        
        ADD_STR(fmt::format("loop.start_{}", m_LoopCounter));
        ADD_STR(fmt::format("loop.end_{}", m_LoopCounter));
        OpCode startIdx = STR_IDX(-2);
        OpCode endIdx = STR_IDX(-1);
        m_LoopCounter++;
        
        PUSH_PENDING_OP(OP_LABEL);
        PUSH_PENDING_OP(startIdx);
        EmitStmt(wh.Body);
        
        EmitExpr(wh.Condition, wh.Condition->ValueKind);
        PUSH_PENDING_OP(OP_JT_POP);
        PUSH_PENDING_OP(startIdx);
        PUSH_PENDING_OP(OP_JMP);
        PUSH_PENDING_OP(endIdx);
        
        PUSH_PENDING_OP(OP_LABEL);
        PUSH_PENDING_OP(endIdx);
    }

    void Emitter::EmitForStmt(Stmt* stmt) {
        ForStmt fs = stmt->For;
        
        ADD_STR(fmt::format("loop.start_{}", m_LoopCounter));
        ADD_STR(fmt::format("loop.end_{}", m_LoopCounter));
        OpCode startIdx = STR_IDX(-2);
        OpCode endIdx = STR_IDX(-1);
        m_LoopCounter++;
        
        PushScope();
        if (fs.Prologue) { EmitDecl(fs.Prologue); }
        
        PUSH_PENDING_OP(OP_LABEL);
        PUSH_PENDING_OP(startIdx);
        if (fs.Condition) {
            EmitExpr(fs.Condition, fs.Condition->ValueKind);
            PUSH_PENDING_OP(OP_JF_POP);
            PUSH_PENDING_OP(endIdx);
        }
        
        EmitBlockStmt(fs.Body);
            
        if (fs.Step) {
            EmitExpr(fs.Step, fs.Step->ValueKind);
        }
        
        PUSH_PENDING_OP(OP_JMP);
        PUSH_PENDING_OP(startIdx);
        PUSH_PENDING_OP(OP_LABEL);
        PUSH_PENDING_OP(endIdx);
        
        PopScope();
    }

    void Emitter::EmitIfStmt(Stmt* stmt) {
        IfStmt ifs = stmt->If;
        
        ADD_STR(fmt::format("if.body{}", m_LoopCounter));
        ADD_STR(fmt::format("if.end{}", m_LoopCounter));
        OpCode bodyIdx = STR_IDX(-2);
        OpCode endIdx = STR_IDX(-1);
        
        EmitExpr(ifs.Condition, ifs.Condition->ValueKind);
        PUSH_PENDING_OP(OP_JT_POP);
        PUSH_PENDING_OP(bodyIdx);
        PUSH_PENDING_OP(OP_JMP);
        PUSH_PENDING_OP(endIdx);
        
        PUSH_PENDING_OP(OP_LABEL);
        PUSH_PENDING_OP(bodyIdx);
        EmitStmt(ifs.Body);
        PUSH_PENDING_OP(OP_LABEL);
        PUSH_PENDING_OP(endIdx);
        
        m_IfCounter++;
    }

    void Emitter::EmitBreakStmt(Stmt* stmt) {
        ADD_STR(fmt::format("loop.end_{}", m_LoopCounter - 1));
        PUSH_PENDING_OP(OP_JMP);
        PUSH_PENDING_OP(STR_IDX(-1));
    }

    void Emitter::EmitContinueStmt(Stmt* stmt) {
        ADD_STR(fmt::format("loop.start_{}", m_LoopCounter - 1));
        PUSH_PENDING_OP(OP_JMP);
        PUSH_PENDING_OP(STR_IDX(-1));
    }

    void Emitter::EmitReturnStmt(Stmt* stmt) {
        ReturnStmt ret = stmt->Return;
        if (ret.Value) {
            EmitExpr(ret.Value, ret.Value->ValueKind);
            PUSH_PENDING_OP(OP_RET_VAL);
            return;
        }
        
        PUSH_PENDING_OP(OP_RET);
    }

    void Emitter::EmitStmt(Stmt* stmt) {
        if (stmt->Kind == StmtKind::Nop) { return; }
        else if (stmt->Kind == StmtKind::Import) { return; }
        else if (stmt->Kind == StmtKind::Block) {
            PushScope();
            EmitBlockStmt(stmt);
            PopScope();
            return;
        } else if (stmt->Kind == StmtKind::While) {
            return EmitWhileStmt(stmt);
        } else if (stmt->Kind == StmtKind::DoWhile) {
            return EmitDoWhileStmt(stmt);
        } else if (stmt->Kind == StmtKind::For) {
            return EmitForStmt(stmt);
        } else if (stmt->Kind == StmtKind::If) {
            return EmitIfStmt(stmt);
        } else if (stmt->Kind == StmtKind::Break) {
            return EmitBreakStmt(stmt);
        } else if (stmt->Kind == StmtKind::Continue) {
            return EmitContinueStmt(stmt);
        } else if (stmt->Kind == StmtKind::Return) {
            return EmitReturnStmt(stmt);
        } else if (stmt->Kind == StmtKind::Expr) {
            ARIA_ASSERT(stmt->ExprStmt->ResultDiscarded, "Result of expression-statement must be discarded");
            return EmitExpr(stmt->ExprStmt, stmt->ExprStmt->ValueKind);
        } else if (stmt->Kind == StmtKind::Decl) {
            return EmitDecl(stmt->DeclStmt);
        }

        ARIA_UNREACHABLE();
    }

    void Emitter::EmitDestructors(const std::vector<Declaration>& declarations) {
        for (auto it = declarations.rbegin(); it != declarations.rend(); it++) {
            auto& decl = *it;
        
            if (decl.Destructor) {
                if (decl.Destructor->Kind == DeclKind::BuiltinDestructor) {
                    switch (decl.Destructor->BuiltinDestructor.Kind) {
                        case BuiltinKind::String: ADD_STR("__aria_destruct_str()"); break;
                        default: ARIA_UNREACHABLE();
                    }
                } else if (decl.Destructor->Kind == DeclKind::Destructor) {
                    ADD_STR(fmt::format("{}::<dtor>()", TypeInfoToString(decl.Type)));
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

    void Emitter::PushStackFrame(const std::string& name) {
        m_ActiveStackFrame.Scopes.clear();
        m_ActiveStackFrame.Name = name;
    }

    void Emitter::PopStackFrame() {
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

    void Emitter::PushScope() {
        m_ActiveStackFrame.Scopes.emplace_back();
    }

    void Emitter::PopScope() {
        EmitDestructors(m_ActiveStackFrame.Scopes.back().DeclaredSymbols);
        m_ActiveStackFrame.Scopes.pop_back();
    }

    void Emitter::MergePendingOpCodes() {
        m_OpCodes.Program.reserve(m_OpCodes.Program.size() + m_PendingOpCodes.size());
        m_OpCodes.Program.insert(m_OpCodes.Program.end(), m_PendingOpCodes.begin(), m_PendingOpCodes.end());
        m_PendingOpCodes.clear();
    }

    OpCode Emitter::TypeInfoToVMTypeIdx(TypeInfo* t) {
        if (t->IsTrivial()) {
            return static_cast<OpCode>(m_BasicTypes[t->Type]);
        }

        if (t->IsStructure()) {
            return static_cast<OpCode>(m_Structs.at(std::get<StructDeclaration>(t->Data).SourceDecl).Index);
        }

        ARIA_UNREACHABLE();
    }

    std::string Emitter::MangleFunction(FunctionDecl* fn) {
        std::string ident = fmt::format("{}(", fn->Identifier);

        for (size_t i = 0; i < fn->Parameters.Size; i++) {
            ident += TypeInfoToString(fn->Parameters.Items[i]->Param.Type);

            if (i != fn->Parameters.Size - 1) {
                ident += ", ";
            }
        }

        ident += ")";

        return ident;
    }

} // namespace Aria::Internal
