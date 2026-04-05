#include "aria/internal/compiler/codegen/emitter.hpp"

namespace Aria::Internal {

    Emitter::Emitter(CompilationContext* ctx) {
        m_Context = ctx;
        m_RootASTNode = ctx->GetRootASTNode();

        EmitImpl();
    }

    void Emitter::EmitImpl() {
        // VVV _start$() VVV //
        m_OpCodes.emplace_back(OpCodeKind::Function, "_start$()");
        m_OpCodes.emplace_back(OpCodeKind::Label,    "_entry$");
        m_OpCodes.emplace_back(OpCodeKind::PushSF);
        PushStackFrame("_start$()");

        EmitStmt(m_RootASTNode);

        m_OpCodes.emplace_back(OpCodeKind::Comment, "^^^ End of allocations VVV");

        MergePendingOpCodes();

        PopStackFrame();
        m_OpCodes.emplace_back(OpCodeKind::Ret);
        // ^^^ _start$() ^^^ //

        // VVV _end$() VVV //
        m_OpCodes.emplace_back(OpCodeKind::Function, "_end$()");
        m_OpCodes.emplace_back(OpCodeKind::Label,    "_entry$");
        m_OpCodes.emplace_back(OpCodeKind::PushSF);
        PushStackFrame("_end$()");

        EmitDestructors(m_GlobalScope.DeclaredSymbols);

        PopStackFrame();
        m_OpCodes.emplace_back(OpCodeKind::PopSF);
        m_OpCodes.emplace_back(OpCodeKind::Ret);
        // ^^^ _end$() ^^^ //

        EmitDeclarations();

        m_Context->SetOpCodes(m_OpCodes);
        m_Context->SetReflectionData(m_ReflectionData);
    }

    void Emitter::EmitBooleanConstantExpr(Expr* expr, ExprValueKind valueKind) {
        BooleanConstantExpr bc = expr->BooleanConstant;
        m_PendingOpCodes.emplace_back(OpCodeKind::Ldc, OpCodeLdc(bc.Value, TypeInfoToVMType(expr->Type)));
    }

    void Emitter::EmitCharacterConstantExpr(Expr* expr, ExprValueKind valueKind) {
        CharacterConstantExpr cc = expr->CharacterConstant;
        m_PendingOpCodes.emplace_back(OpCodeKind::Ldc, OpCodeLdc(cc.Value, TypeInfoToVMType(expr->Type)));
    }

    void Emitter::EmitIntegerConstantExpr(Expr* expr,ExprValueKind valueKind) {
        IntegerConstantExpr ic = expr->IntegerConstant;
        
        if (expr->Type->Type == PrimitiveType::Long) {
            m_PendingOpCodes.emplace_back(OpCodeKind::Ldc, OpCodeLdc(static_cast<i64>(ic.Value), TypeInfoToVMType(expr->Type)));
        } else if (expr->Type->Type == PrimitiveType::ULong) {
            m_PendingOpCodes.emplace_back(OpCodeKind::Ldc, OpCodeLdc(ic.Value, TypeInfoToVMType(expr->Type)));
        } else {
            ARIA_UNREACHABLE();
        }
    }

    void Emitter::EmitFloatingConstantExpr(Expr* expr, ExprValueKind valueKind) {
        FloatingConstantExpr fc = expr->FloatingConstant;
        m_PendingOpCodes.emplace_back(OpCodeKind::Ldc, OpCodeLdc(fc.Value, TypeInfoToVMType(expr->Type)));
    }

    void Emitter::EmitStringConstantExpr(Expr* expr, ExprValueKind valueKind) {
        StringConstantExpr sc = expr->StringConstant;
        m_PendingOpCodes.emplace_back(OpCodeKind::LdStr, fmt::format("{}", sc.Value));
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
        std::string ident = fmt::format("{}", declRef.Identifier);

        // VVV -LocalVar- VVV //
        if (declRef.Kind == DeclRefKind::LocalVar) {
            for (size_t i = m_ActiveStackFrame.Scopes.size(); i > 0; i--) {
                Scope& s = m_ActiveStackFrame.Scopes[i - 1];
                if (s.DeclaredSymbolMap.contains(ident)) {
                    Declaration& decl = s.DeclaredSymbols[s.DeclaredSymbolMap.at(ident)];
                    
                    if (valueKind == ExprValueKind::LValue) {
                        if (expr->Type->IsReference()) {
                            m_PendingOpCodes.emplace_back(OpCodeKind::LdLocal, std::get<size_t>(decl.Data));
                        } else {
                            m_PendingOpCodes.emplace_back(OpCodeKind::LdPtrLocal, std::get<size_t>(decl.Data));
                        }
                    } else if (valueKind == ExprValueKind::RValue) {
                        m_PendingOpCodes.emplace_back(OpCodeKind::LdLocal, std::get<size_t>(decl.Data));
        
                        if (expr->Type->IsReference()) {
                            expr->Type->Reference = false;
                            m_PendingOpCodes.emplace_back(OpCodeKind::Deref, TypeInfoToVMType(expr->Type));
                            expr->Type->Reference = true;
                        }
                    }
                }
            }

            return;
        }
        // ^^^ -LocalVar- ^^^ //
        
        // VVV -ParamVar- VVV //
        if (declRef.Kind == DeclRefKind::ParamVar) {
            if (valueKind == ExprValueKind::LValue) {
                if (expr->Type->IsReference()) {
                    m_PendingOpCodes.emplace_back(OpCodeKind::LdArg, m_ActiveStackFrame.Parameters.at(ident));
                } else {
                    m_PendingOpCodes.emplace_back(OpCodeKind::LdPtrArg, m_ActiveStackFrame.Parameters.at(ident));
                }
            } else if (valueKind == ExprValueKind::RValue) {
                m_PendingOpCodes.emplace_back(OpCodeKind::LdArg, m_ActiveStackFrame.Parameters.at(ident));
        
                if (expr->Type->IsReference()) {
                    expr->Type->Reference = false;
                    m_PendingOpCodes.emplace_back(OpCodeKind::Deref, TypeInfoToVMType(expr->Type));
                    expr->Type->Reference = true;
                }
            }

            return;
        }
        // ^^^ -ParamVar- ^^^ //
        
        // VVV -GlobalVar- VVV //
        if (declRef.Kind == DeclRefKind::GlobalVar) {
            if (valueKind == ExprValueKind::LValue) {
                if (expr->Type->IsReference()) {
                    m_PendingOpCodes.emplace_back(OpCodeKind::LdGlobal, ident);
                } else {
                    m_PendingOpCodes.emplace_back(OpCodeKind::LdPtrGlobal, ident);
                }
            } else if (valueKind == ExprValueKind::RValue) {
                m_PendingOpCodes.emplace_back(OpCodeKind::LdGlobal, ident);
        
                if (expr->Type->IsReference()) {
                    expr->Type->Reference = false;
                    m_PendingOpCodes.emplace_back(OpCodeKind::Deref, TypeInfoToVMType(expr->Type));
                    expr->Type->Reference = true;
                }
            }

            return;
        }
        // ^^^ -GlobalVar- ^^^ //
        
        // VVV -Function- VVV //
        if (declRef.Kind == DeclRefKind::Function) {
            ARIA_ASSERT(valueKind != ExprValueKind::RValue, "Cannot load function as rvalue");
            m_PendingOpCodes.emplace_back(OpCodeKind::LdFunc, fmt::format("{}()", ident));
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
        //                 m_PendingOpCodes.emplace_back(OpCodeKind::LdPtrMember, OpCodeMember(sdecl.FieldIndices.at(fd->GetIdentifier()), {VMTypeKind::Ptr}, TypeInfoToVMType(mem->GetParentType())));
        //             } else if (valueKind == ExprValueKind::RValue) {
        //                 m_PendingOpCodes.emplace_back(OpCodeKind::LdMember, OpCodeMember(sdecl.FieldIndices.at(fd->GetIdentifier()), TypeInfoToVMType(fd->GetResolvedType()), TypeInfoToVMType(mem->GetParentType())));
        //             }
        //         }
        //     } else if (MethodDecl* md = GetNode<MethodDecl>(field)) {
        //         if (md->GetRawIdentifier() == mem->GetMember()) {
        //             m_PendingOpCodes.emplace_back(OpCodeKind::LdFunc, fmt::format("{}::{}()", sd.Identifier, md->GetIdentifier()));
        //         }
        //     }  
        // }

        ARIA_ASSERT(false, "todo!");
    }

    void Emitter::EmitSelfExpr(Expr* expr, ExprValueKind valueKind) {
        ARIA_ASSERT(valueKind == ExprValueKind::LValue, "rvalue of 'self' is not yet supported");
        
        // Because self is a reference (aka pointer), we want to the load the actual value of it
        m_PendingOpCodes.emplace_back(OpCodeKind::LdArg, static_cast<size_t>(0));
    }

    void Emitter::EmitTemporaryExpr(Expr* expr, ExprValueKind valueKind) {
        TemporaryExpr& temp = expr->Temporary;

        EmitExpr(temp.Expression, valueKind);
        
        // Create a new temporary
        m_PendingOpCodes.emplace_back(OpCodeKind::DeclareLocal, m_ActiveStackFrame.LocalCount);
        m_PendingOpCodes.emplace_back(OpCodeKind::LdLocal, m_ActiveStackFrame.LocalCount);

        Declaration d;
        d.Type = temp.Expression->Type;
        d.Data = m_ActiveStackFrame.LocalCount;
        d.Destructor = temp.Destructor;
        m_ActiveStackFrame.LocalCount++;
        
        m_Temporaries.push_back(d);
    }

    void Emitter::EmitCopyExpr(Expr* expr, ExprValueKind valueKind) {
        CopyExpr copy = expr->Copy;

        EmitExpr(copy.Expression, ExprValueKind::LValue);

        if (copy.Constructor->Kind == DeclKind::BuiltinCopyConstructor) {
            switch (copy.Constructor->BuiltinCopyConstructor.Kind) {
                case BuiltinKind::String: m_PendingOpCodes.emplace_back(OpCodeKind::DupStr); break;
                default: ARIA_UNREACHABLE();
            }
        }
    }

    void Emitter::EmitCallExpr(Expr* expr, ExprValueKind valueKind) {
        CallExpr call = expr->Call;
        
        EmitExpr(call.Callee, call.Callee->ValueKind);
        
        for (size_t i = 0; i < call.Arguments.Size; i++) {
            EmitExpr(call.Arguments.Items[i], call.Arguments.Items[i]->ValueKind);
            m_PendingOpCodes.emplace_back(OpCodeKind::DeclareArg, i);
        }
        
        size_t retCount = 0;
        TypeInfo* retType = expr->Type;
        if (retType->Type != PrimitiveType::Void) {
            m_PendingOpCodes.emplace_back(OpCodeKind::Alloca, TypeInfoToVMType(retType));
            retCount = 1;
        }
        
        m_PendingOpCodes.emplace_back(OpCodeKind::Call, OpCodeCall(call.Arguments.Size, TypeInfoToVMType(retType)));
        
        // The only special case is when returning a reference and getting it as an rvalue
        if (valueKind == ExprValueKind::RValue) {
            if (retType->IsReference()) {
                retType->Reference = false;
                m_PendingOpCodes.emplace_back(OpCodeKind::Deref, TypeInfoToVMType(retType));
                retType->Reference = true;
            }
        }
    }

    void Emitter::EmitMethodCallExpr(Expr* expr, ExprValueKind valueKind) {
        // MethodCallExpr* call = GetNode<MethodCallExpr>(expr);
        // 
        // EmitExpr(call->GetCallee(), call->GetCallee()->GetValueKind());
        // 
        // // Push self
        // EmitExpr(call->GetCallee()->GetParent(), ExprValueKind::LValue);
        // m_PendingOpCodes.emplace_back(OpCodeKind::DeclareArg, static_cast<size_t>(0));
        // 
        // for (size_t i = 0; i < call->GetArguments().Size; i++) {
        //     EmitExpr(call->GetArguments().Items[i], call->GetArguments().Items[i]->GetValueKind());
        //     m_PendingOpCodes.emplace_back(OpCodeKind::DeclareArg, i + 1);
        // }
        // 
        // size_t retCount = 0;
        // TypeInfo* retType = call->GetResolvedType();
        // if (retType->Type != PrimitiveType::Void) {
        //     m_PendingOpCodes.emplace_back(OpCodeKind::Alloca, TypeInfoToVMType(retType));
        //     retCount = 1;
        // }
        // 
        // // We do size + 1 because self is an implicit parameter that isn't in the argument vector,
        // // However the VM certainly does need the actual amount of arguments
        // m_PendingOpCodes.emplace_back(OpCodeKind::Call, OpCodeCall(call->GetArguments().Size + 1, TypeInfoToVMType(retType)));
        // 
        // // The only special case is when returning a reference and getting it as an rvalue
        // if (valueKind == ExprValueKind::RValue) {
        //     if (retType->IsReference()) {
        //         retType->Reference = false;
        //         m_PendingOpCodes.emplace_back(OpCodeKind::Deref, TypeInfoToVMType(retType));
        //         retType->Reference = true;
        //     }
        // }

        ARIA_ASSERT(false, "todo!");
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
            EmitExpr(cast.Expression, ExprValueKind::RValue);
            m_PendingOpCodes.emplace_back(OpCodeKind::Cast, TypeInfoToVMType(expr->Type));
            return;
        }
        
        ARIA_UNREACHABLE();
    }

    void Emitter::EmitCastExpr(Expr* expr, ExprValueKind valueKind) {
        CastExpr cast = expr->Cast;
        
        if (cast.Kind == CastKind::LValueToRValue) {
            return EmitExpr(cast.Expression, ExprValueKind::RValue);
        } else {
            EmitExpr(cast.Expression, ExprValueKind::RValue);
            m_PendingOpCodes.emplace_back(OpCodeKind::Cast, TypeInfoToVMType(expr->Type));
            return;
        }
        
        ARIA_UNREACHABLE();
    }

    void Emitter::EmitUnaryOperatorExpr(Expr* expr, ExprValueKind valueKind) {
        UnaryOperatorExpr unop = expr->UnaryOperator;
        
        switch (unop.Operator) {
            case UnaryOperatorKind::Negate: EmitExpr(unop.Expression, unop.Expression->ValueKind); m_PendingOpCodes.emplace_back(OpCodeKind::Neg, TypeInfoToVMType(unop.Expression->Type)); break;
            default: ARIA_UNREACHABLE();
        }
    }

    void Emitter::EmitBinaryOperatorExpr(Expr* expr, ExprValueKind valueKind) {
        BinaryOperatorExpr binop = expr->BinaryOperator;
        
        #define BINOP(_enum, op) case BinaryOperatorKind::_enum: { \
                EmitExpr(binop.LHS, binop.LHS->ValueKind); \
                EmitExpr(binop.RHS, binop.RHS->ValueKind); \
                m_PendingOpCodes.emplace_back(OpCodeKind::op, TypeInfoToVMType(binop.LHS->Type)); \
                break; \
            }
        
        switch (binop.Operator) {
            BINOP(Add,         Add)
            BINOP(Sub,         Sub)
            BINOP(Mul,         Mul)
            BINOP(Div,         Div)
            BINOP(Mod,         Mod)
            BINOP(Less,        Lt)
            BINOP(LessOrEq,    Lte)
            BINOP(Greater,     Gt)
            BINOP(GreaterOrEq, Lte)
            BINOP(IsEq,        Cmp)
            BINOP(IsNotEq,     Ncmp)
        
            case BinaryOperatorKind::LogAnd: {
                std::string andEnd = fmt::format("logand.end_{}", m_AndCounter);
                m_AndCounter++;
        
                EmitExpr(binop.LHS, binop.LHS->ValueKind);
                m_PendingOpCodes.emplace_back(OpCodeKind::Jf, andEnd);
                EmitExpr(binop.RHS, binop.RHS->ValueKind);
                m_PendingOpCodes.emplace_back(OpCodeKind::And, TypeInfoToVMType(expr->Type));
                m_PendingOpCodes.emplace_back(OpCodeKind::Jmp, andEnd);
        
                m_PendingOpCodes.emplace_back(OpCodeKind::Label, andEnd);
        
                break;
            }
        
            case BinaryOperatorKind::LogOr: {
                std::string orEnd = fmt::format("logor.end_{}", m_OrCounter);
                m_OrCounter++;
        
                EmitExpr(binop.LHS, binop.LHS->ValueKind);
                m_PendingOpCodes.emplace_back(OpCodeKind::Jt, orEnd);
                EmitExpr(binop.RHS, binop.RHS->ValueKind);
                m_PendingOpCodes.emplace_back(OpCodeKind::Or, TypeInfoToVMType(expr->Type));
                m_PendingOpCodes.emplace_back(OpCodeKind::Jmp, orEnd);
        
                m_PendingOpCodes.emplace_back(OpCodeKind::Label, orEnd);
        
                break;
            }
        
            case BinaryOperatorKind::Eq: {
                EmitExpr(binop.LHS, binop.LHS->ValueKind);
                EmitExpr(binop.RHS, binop.RHS->ValueKind);
        
                m_PendingOpCodes.emplace_back(OpCodeKind::Store);
                EmitExpr(binop.LHS, valueKind);
                break;
            }
        }
        
        #undef BINOP
    }

    void Emitter::EmitCompoundAssignExpr(Expr* expr, ExprValueKind valueKind) {
        CompoundAssignExpr compAss = expr->CompoundAssign;
        
        #define BINOP(_enum, op) case BinaryOperatorKind::Compound##_enum: { \
                EmitExpr(compAss.LHS, ExprValueKind::RValue); \
                EmitExpr(compAss.RHS, compAss.RHS->ValueKind); \
                m_PendingOpCodes.emplace_back(OpCodeKind::op, TypeInfoToVMType(compAss.LHS->Type)); \
                break; \
            }
        
        // Load the destination
        EmitExpr(compAss.LHS, ExprValueKind::LValue);
        
        switch (compAss.Operator) {
            BINOP(Add, Add)
            BINOP(Sub, Sub)
            BINOP(Mul, Mul)
            BINOP(Div, Div)
            BINOP(Mod, Mod)
            BINOP(And, And)
            BINOP(Or, Or)
            BINOP(Xor, Xor)
        
            default: ARIA_UNREACHABLE();
        }
        
        m_PendingOpCodes.emplace_back(OpCodeKind::Store);
        EmitExpr(compAss.LHS, valueKind);
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
        } else if (expr->Kind == ExprKind::MethodCall) {
            EmitMethodCallExpr(expr, valueKind);
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

        if (expr->IsStmtExpr) {
            EmitDestructors(m_Temporaries);
            m_Temporaries.clear();

            if (!expr->Type->IsVoid()) {
                m_PendingOpCodes.emplace_back(OpCodeKind::Pop);
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
        std::string ident = fmt::format("{}", varDecl.Identifier);

        m_OpCodes.emplace_back(OpCodeKind::Alloca, TypeInfoToVMType(varDecl.Type));

        Declaration d;
        d.Type = varDecl.Type;
        
        if (varDecl.Type->Type == PrimitiveType::String) {
            d.Destructor = Decl::Create(m_Context, SourceLocation(), SourceRange(), DeclKind::BuiltinDestructor, BuiltinDestructorDecl(BuiltinKind::String));
        }

        // We want to allocate the variables up front (at the start of the stack frame)
        // This is why we use m_OpCodes instead of m_PendingOpCodes here
        if (IsGlobalScope()) {
            m_OpCodes.emplace_back(OpCodeKind::DeclareGlobal, ident);
            d.Data = ident;
        
            m_GlobalScope.DeclaredSymbols.push_back(d);
            m_GlobalScope.DeclaredSymbolMap[ident] = m_GlobalScope.DeclaredSymbols.size() - 1;
        } else {
            m_OpCodes.emplace_back(OpCodeKind::DeclareLocal, m_ActiveStackFrame.LocalCount);
            d.Data = m_ActiveStackFrame.LocalCount;
            m_ActiveStackFrame.LocalCount++;
        
            m_ActiveStackFrame.Scopes.back().DeclaredSymbols.push_back(d);
            m_ActiveStackFrame.Scopes.back().DeclaredSymbolMap[ident] = m_ActiveStackFrame.Scopes.back().DeclaredSymbols.size() - 1;
        }
        
        // For initializers we need to just store the value in the already declared variable
        if (varDecl.DefaultValue) {
            if (IsGlobalScope()) {
                m_PendingOpCodes.emplace_back(OpCodeKind::LdPtrGlobal, ident);
            } else {
                m_PendingOpCodes.emplace_back(OpCodeKind::LdPtrLocal, m_ActiveStackFrame.LocalCount - 1);
            }

            EmitExpr(varDecl.DefaultValue, varDecl.DefaultValue->ValueKind);
            m_PendingOpCodes.emplace_back(OpCodeKind::Store);
        }
    }
    
    void Emitter::EmitParamDecl(Decl* decl) {
        ParamDecl param = decl->Param;
        m_ActiveStackFrame.Parameters[fmt::format("{}", param.Identifier)] = m_ActiveStackFrame.ParameterCount++;
    }

    void Emitter::EmitFunctionDecl(Decl* decl) {
        FunctionDecl fnDecl = decl->Function;
        
        m_DeclarationsToDeclare.emplace_back(fmt::format("{}()", fnDecl.Identifier), decl);
    }

    void Emitter::EmitStructDecl(Decl* decl) {
        // StructDecl* s = GetNode<StructDecl>(decl);
        // 
        // RuntimeStructDeclaration sd;
        // 
        // for (Decl* field : s->GetFields()) {
        //     if (FieldDecl* fd = GetNode<FieldDecl>(field)) {
        //         sd.FieldIndices[fd->GetIdentifier()] = sd.FieldIndices.size();
        //     } else if (MethodDecl* md = GetNode<MethodDecl>(field)) {
        //         m_DeclarationsToDeclare.emplace_back(fmt::format("{}::{}()", s->GetIdentifier(), md->GetIdentifier()), md);
        //     }
        // }
        // 
        // m_Structs[s->GetIdentifier()] = sd;
        // m_DeclarationsToDeclare.emplace_back(s->GetIdentifier(), decl);

        ARIA_ASSERT(false, "todo!");
    }

    void Emitter::EmitDecl(Decl* decl) {
        if (decl->Kind == DeclKind::TranslationUnit) {
            return EmitTranslationUnitDecl(decl);
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
        
        std::string loopStart = fmt::format("loop.start_{}", m_LoopCounter);
        std::string loopEnd = fmt::format("loop.end_{}", m_LoopCounter);
        m_LoopCounter++;
        
        m_PendingOpCodes.emplace_back(OpCodeKind::Label, loopStart);
        EmitExpr(wh.Condition, wh.Condition->ValueKind);
        m_PendingOpCodes.emplace_back(OpCodeKind::JfPop, loopEnd);
        EmitBlockStmt(wh.Body);
        
        m_PendingOpCodes.emplace_back(OpCodeKind::Jmp, loopStart);
        m_PendingOpCodes.emplace_back(OpCodeKind::Label, loopEnd);
    }

    void Emitter::EmitDoWhileStmt(Stmt* stmt) {
        DoWhileStmt wh = stmt->DoWhile;
        
        std::string loopStart = fmt::format("loop.start_{}", m_LoopCounter);
        std::string loopEnd = fmt::format("loop.end_{}", m_LoopCounter);
        m_LoopCounter++;
        
        m_PendingOpCodes.emplace_back(OpCodeKind::Label, loopStart);
        EmitBlockStmt(wh.Body);
        
        EmitExpr(wh.Condition, wh.Condition->ValueKind);
        m_PendingOpCodes.emplace_back(OpCodeKind::JtPop, loopStart);
        m_PendingOpCodes.emplace_back(OpCodeKind::Jmp, loopEnd);

        m_PendingOpCodes.emplace_back(OpCodeKind::Label, loopEnd);
    }

    void Emitter::EmitForStmt(Stmt* stmt) {
        ForStmt fs = stmt->For;
        
        std::string loopStart = fmt::format("loop.start_{}", m_LoopCounter);
        std::string loopEnd = fmt::format("loop.end_{}", m_LoopCounter);
        m_LoopCounter++;
        
        PushScope();
        if (fs.Prologue) { EmitDecl(fs.Prologue); }
        
        m_PendingOpCodes.emplace_back(OpCodeKind::Label, loopStart);
        if (fs.Condition) {
            EmitExpr(fs.Condition, fs.Condition->ValueKind);
            m_PendingOpCodes.emplace_back(OpCodeKind::JfPop, loopEnd);
        }
        
        EmitBlockStmt(fs.Body);
            
        if (fs.Step) {
            EmitExpr(fs.Step, fs.Step->ValueKind);
            m_PendingOpCodes.emplace_back(OpCodeKind::Pop);
        }
        
        m_PendingOpCodes.emplace_back(OpCodeKind::Jmp, loopStart);
        m_PendingOpCodes.emplace_back(OpCodeKind::Label, loopEnd);
        
        PopScope();
    }

    void Emitter::EmitIfStmt(Stmt* stmt) {
        IfStmt ifs = stmt->If;
        
        std::string ifBody = fmt::format("if.body_{}", m_IfCounter);
        std::string ifEnd = fmt::format("if.end_{}", m_IfCounter);
        
        EmitExpr(ifs.Condition, ifs.Condition->ValueKind);
        m_PendingOpCodes.emplace_back(OpCodeKind::JtPop, ifBody);
        m_PendingOpCodes.emplace_back(OpCodeKind::Jmp, ifEnd);
        
        m_PendingOpCodes.emplace_back(OpCodeKind::Label, ifBody);
        EmitBlockStmt(ifs.Body);
        m_PendingOpCodes.emplace_back(OpCodeKind::Label, ifEnd);
        
        m_IfCounter++;
    }

    void Emitter::EmitBreakStmt(Stmt* stmt) {
        m_PendingOpCodes.emplace_back(OpCodeKind::Jmp, fmt::format("loop.end_{}", m_LoopCounter - 1));
    }

    void Emitter::EmitContinueStmt(Stmt* stmt) {
        m_PendingOpCodes.emplace_back(OpCodeKind::Jmp, fmt::format("loop.start_{}", m_LoopCounter - 1));
    }

    void Emitter::EmitReturnStmt(Stmt* stmt) {
        ReturnStmt ret = stmt->Return;
        if (ret.Value) {
            m_PendingOpCodes.emplace_back(OpCodeKind::LdPtrRet);
            EmitExpr(ret.Value, ret.Value->ValueKind);
            m_PendingOpCodes.emplace_back(OpCodeKind::Store);
        }
        
        m_PendingOpCodes.emplace_back(OpCodeKind::PopSF);
        m_PendingOpCodes.emplace_back(OpCodeKind::Ret);
    }

    void Emitter::EmitStmt(Stmt* stmt) {
        if (stmt->Kind == StmtKind::Nop) { return; }
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
            return EmitExpr(stmt->ExprStmt, stmt->ExprStmt->ValueKind);
        } else if (stmt->Kind == StmtKind::Decl) {
            return EmitDecl(stmt->DeclStmt);
        }

        ARIA_UNREACHABLE();
    }

    bool Emitter::IsStartStackFrame() {
        return m_ActiveStackFrame.Name == "_start$()";
    }

    bool Emitter::IsGlobalScope() {
        return IsStartStackFrame() && m_ActiveStackFrame.Scopes.size() == 1;
    }

    void Emitter::EmitDestructors(const std::vector<Declaration>& declarations) {
        for (auto it = declarations.rbegin(); it != declarations.rend(); it++) {
            auto& decl = *it;
        
            if (decl.Destructor) {
                if (std::holds_alternative<size_t>(decl.Data)) {
                    m_PendingOpCodes.emplace_back(OpCodeKind::LdPtrLocal, std::get<size_t>(decl.Data));
                } else if (std::holds_alternative<std::string>(decl.Data)) {
                    m_PendingOpCodes.emplace_back(OpCodeKind::LdPtrGlobal, std::get<std::string>(decl.Data));
                }
        
                if (decl.Destructor->Kind == DeclKind::BuiltinDestructor) {
                    switch (decl.Destructor->BuiltinDestructor.Kind) {
                        case BuiltinKind::String: m_PendingOpCodes.emplace_back(OpCodeKind::DestructStr); break;
                        default: ARIA_UNREACHABLE();
                    }
                }
            }
        }
    }

    void Emitter::PushStackFrame(const std::string& name) {
        m_ActiveStackFrame.Scopes.clear();
        m_ActiveStackFrame.Scopes.emplace_back();
        m_ActiveStackFrame.Name = name;
    }

    void Emitter::PopStackFrame() {
        m_ActiveStackFrame.Scopes.clear();
        m_ActiveStackFrame.Name.clear();
        m_ActiveStackFrame.Parameters.clear();
        m_ActiveStackFrame.ParameterCount = 0;
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
        m_OpCodes.reserve(m_OpCodes.size() + m_PendingOpCodes.size());
        m_OpCodes.insert(m_OpCodes.end(), m_PendingOpCodes.begin(), m_PendingOpCodes.end());
        m_PendingOpCodes.clear();
    }

    void Emitter::EmitDeclarations() {
        for (const auto&[name, decl] : m_DeclarationsToDeclare) {
            if (decl->Kind == DeclKind::Function) {
                FunctionDecl& fnDecl = decl->Function;
                if (fnDecl.Body) {
                    m_OpCodes.emplace_back(OpCodeKind::Function, name);
                    m_OpCodes.emplace_back(OpCodeKind::Label, "_entry$");
                    m_OpCodes.emplace_back(OpCodeKind::PushSF);
                    PushStackFrame(name);
                    
                    size_t returnSlot = (std::get<FunctionDeclaration>(fnDecl.Type->Data).ReturnType->Type == PrimitiveType::Void) ? 0 : 1;
                    
                    for (Decl* p : fnDecl.Parameters) {
                        EmitParamDecl(p);
                    }
                    
                    EmitStmt(fnDecl.Body);
                    MergePendingOpCodes();

                    if (m_OpCodes.back().Kind != OpCodeKind::Ret) {
                        m_OpCodes.emplace_back(OpCodeKind::PopSF);
                        m_OpCodes.emplace_back(OpCodeKind::Ret);
                    }
        
                    PopStackFrame();
        
                    CompilerReflectionDeclaration d;
                    d.Type = TypeInfoToVMType(std::get<FunctionDeclaration>(fnDecl.Type->Data).ReturnType);
                    d.Kind = ReflectionKind::Function;
        
                    m_ReflectionData.Declarations[name] = d;
                }
            } else if (decl->Kind == DeclKind::Method) {
                // m_OpCodes.emplace_back(OpCodeKind::Function, name);
                // m_OpCodes.emplace_back(OpCodeKind::Label, "_entry$");
                // 
                // m_OpCodes.emplace_back(OpCodeKind::PushSF);
                // PushStackFrame(name);
                // m_ActiveStackFrame.ParameterCount++; // Used for "self"
                // 
                // size_t returnSlot = (mDecl->GetResolvedType()->Type == PrimitiveType::Void) ? 0 : 1;
                // 
                // for (ParamDecl* p : mDecl->GetParameters()) {
                //     EmitParamDecl(p);
                // }
                // 
                // EmitCompoundStmt(mDecl->GetBody());
                // 
                // if (m_OpCodes.back().Kind != OpCodeKind::Ret) {
                //     m_OpCodes.emplace_back(OpCodeKind::PopSF);
                //     m_OpCodes.emplace_back(OpCodeKind::Ret);
                // }
                // 
                // PopStackFrame();
                // 
                // CompilerReflectionDeclaration d;
                // d.Type = TypeInfoToVMType(std::get<FunctionDeclaration>(mDecl->GetResolvedType()->Data).ReturnType);
                // d.Kind = ReflectionKind::Function;
                // 
                // m_ReflectionData.Declarations[name] = d;
            } else if (decl->Kind == DeclKind::Struct) {
                // std::vector<VMType> fields;
                // fields.reserve(stDecl->GetFields().Size);
                // 
                // for (Decl* field : stDecl->GetFields()) {
                //     if (FieldDecl* fd = GetNode<FieldDecl>(field)) {
                //         fields.push_back(TypeInfoToVMType(fd->GetResolvedType()));
                //     }
                // }
                // 
                // m_OpCodes.emplace_back(OpCodeKind::Struct, OpCodeStruct(fields, std::string_view(stDecl->GetRawIdentifier().Data(), stDecl->GetRawIdentifier().Size())));
            }
        }
    }

    VMType Emitter::TypeInfoToVMType(TypeInfo* t) {
        switch (t->Type) {
            case PrimitiveType::Void:   return { VMTypeKind::Void };

            case PrimitiveType::Bool:   return { VMTypeKind::I1 };

            case PrimitiveType::Char:   return { VMTypeKind::I8 };
            case PrimitiveType::UChar:  return { VMTypeKind::U8 };
            case PrimitiveType::Short:  return { VMTypeKind::I16 };
            case PrimitiveType::UShort: return { VMTypeKind::U16 };
            case PrimitiveType::Int:    return { VMTypeKind::I32 };
            case PrimitiveType::UInt:   return { VMTypeKind::U32 };
            case PrimitiveType::Long:   return { VMTypeKind::I64 };
            case PrimitiveType::ULong:  return { VMTypeKind::U64 };

            case PrimitiveType::Float:  return { VMTypeKind::F32 };
            case PrimitiveType::Double: return { VMTypeKind::F64 };

            case PrimitiveType::String: { return { VMTypeKind::String }; }

            case PrimitiveType::Structure: {
                StringView ident = std::get<StructDeclaration>(t->Data).Identifier;
                return { VMTypeKind::Struct, std::string_view(ident.Data(), ident.Size()) }; 
            }

            default: ARIA_ASSERT(false, "todo!");
        }
    }

} // namespace Aria::Internal
