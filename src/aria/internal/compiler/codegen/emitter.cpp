#include "aria/internal/compiler/codegen/emitter.hpp"
#include "aria/internal/compiler/ast/ast.hpp"

namespace Aria::Internal {

    Emitter::Emitter(CompilationContext* ctx) {
        m_Context = ctx;
        m_RootASTNode = ctx->GetRootASTNode();

        EmitImpl();
    }

    void Emitter::EmitImpl() {
        m_OpCodes.emplace_back(OpCodeKind::Function, "_start$()");
        m_OpCodes.emplace_back(OpCodeKind::Label,    "_entry$");
        m_OpCodes.emplace_back(OpCodeKind::PushSF);
        PushStackFrame("_start$()");

        EmitStmt(m_RootASTNode);

        PopStackFrame();
        m_OpCodes.emplace_back(OpCodeKind::Ret);

        m_OpCodes.emplace_back(OpCodeKind::Function, "_end$()");
        m_OpCodes.emplace_back(OpCodeKind::Label,    "_entry$");
        m_OpCodes.emplace_back(OpCodeKind::PushSF);
        PushStackFrame("_end$()");

        EmitDestructors(m_GlobalScope.DeclaredSymbols);

        PopStackFrame();
        m_OpCodes.emplace_back(OpCodeKind::Ret);

        EmitDeclarations();

        m_Context->SetOpCodes(m_OpCodes);
        m_Context->SetReflectionData(m_ReflectionData);
    }

    void Emitter::EmitBooleanConstantExpr(Expr* expr, ExprValueKind valueKind) {
        BooleanConstantExpr* bc = GetNode<BooleanConstantExpr>(expr);

        m_OpCodes.emplace_back(OpCodeKind::Ldc, OpCodeLdc(bc->GetValue(), TypeInfoToVMType(bc->GetResolvedType())));
    }

    void Emitter::EmitCharacterConstantExpr(Expr* expr, ExprValueKind valueKind) {
        CharacterConstantExpr* cc = GetNode<CharacterConstantExpr>(expr);

        m_OpCodes.emplace_back(OpCodeKind::Ldc, OpCodeLdc(cc->GetValue(), TypeInfoToVMType(cc->GetResolvedType())));
    }

    void Emitter::EmitIntegerConstantExpr(Expr* expr,ExprValueKind valueKind) {
        IntegerConstantExpr* ic = GetNode<IntegerConstantExpr>(expr);
        
        if (ic->GetResolvedType()->Type == PrimitiveType::Long) {
            m_OpCodes.emplace_back(OpCodeKind::Ldc, OpCodeLdc(static_cast<i64>(ic->GetValue()), TypeInfoToVMType(ic->GetResolvedType())));
        } else if (ic->GetResolvedType()->Type == PrimitiveType::ULong) {
            m_OpCodes.emplace_back(OpCodeKind::Ldc, OpCodeLdc(ic->GetValue(), TypeInfoToVMType(ic->GetResolvedType())));
        } else {
            ARIA_UNREACHABLE();
        }
    }

    void Emitter::EmitFloatingConstantExpr(Expr* expr, ExprValueKind valueKind) {
        FloatingConstantExpr* fc = GetNode<FloatingConstantExpr>(expr);

        m_OpCodes.emplace_back(OpCodeKind::Ldc, OpCodeLdc(fc->GetValue(), TypeInfoToVMType(fc->GetResolvedType())));
    }

    void Emitter::EmitStringConstantExpr(Expr* expr, ExprValueKind valueKind) {
        StringConstantExpr* sc = GetNode<StringConstantExpr>(expr);
        m_OpCodes.emplace_back(OpCodeKind::LdStr, fmt::format("{}", sc->GetValue()));
    }

    void Emitter::EmitDeclRefExpr(Expr* expr, ExprValueKind valueKind) {
        DeclRefExpr* declRef = GetNode<DeclRefExpr>(expr);

        // This may look a bit messy but all it is doing is emitting the correct op code based off of what we need
        if (declRef->GetKind() == DeclRefKind::LocalVar) {
            for (size_t i = m_ActiveStackFrame.Scopes.size(); i > 0; i--) {
                Scope& s = m_ActiveStackFrame.Scopes[i - 1];
                if (s.DeclaredSymbolMap.contains(declRef->GetIdentifier())) {
                    Declaration& decl = s.DeclaredSymbols[s.DeclaredSymbolMap.at(declRef->GetIdentifier())];
                    
                    if (valueKind == ExprValueKind::LValue) {
                        if (expr->GetResolvedType()->IsReference()) {
                            m_OpCodes.emplace_back(OpCodeKind::LdLocal, std::get<size_t>(decl.Data));
                        } else {
                            m_OpCodes.emplace_back(OpCodeKind::LdPtrLocal, std::get<size_t>(decl.Data));
                        }
                    } else if (valueKind == ExprValueKind::RValue) {
                        m_OpCodes.emplace_back(OpCodeKind::LdLocal, std::get<size_t>(decl.Data));

                        if (expr->GetResolvedType()->IsReference()) {
                            expr->GetResolvedType()->Reference = false;
                            m_OpCodes.emplace_back(OpCodeKind::Deref, TypeInfoToVMType(expr->GetResolvedType()));
                            expr->GetResolvedType()->Reference = true;
                        }
                    }
                }
            }
        } else if (declRef->GetKind() == DeclRefKind::ParamVar) {
            if (valueKind == ExprValueKind::LValue) {
                if (expr->GetResolvedType()->IsReference()) {
                    m_OpCodes.emplace_back(OpCodeKind::LdArg, m_ActiveStackFrame.Parameters.at(declRef->GetIdentifier()));
                } else {
                    m_OpCodes.emplace_back(OpCodeKind::LdPtrArg, m_ActiveStackFrame.Parameters.at(declRef->GetIdentifier()));
                }
            } else if (valueKind == ExprValueKind::RValue) {
                m_OpCodes.emplace_back(OpCodeKind::LdArg, m_ActiveStackFrame.Parameters.at(declRef->GetIdentifier()));

                if (expr->GetResolvedType()->IsReference()) {
                    expr->GetResolvedType()->Reference = false;
                    m_OpCodes.emplace_back(OpCodeKind::Deref, TypeInfoToVMType(expr->GetResolvedType()));
                    expr->GetResolvedType()->Reference = true;
                }
            }
        } else if (declRef->GetKind() == DeclRefKind::GlobalVar) {
            if (valueKind == ExprValueKind::LValue) {
                if (expr->GetResolvedType()->IsReference()) {
                    m_OpCodes.emplace_back(OpCodeKind::LdGlobal, declRef->GetIdentifier());
                } else {
                    m_OpCodes.emplace_back(OpCodeKind::LdPtrGlobal, declRef->GetIdentifier());
                }
            } else if (valueKind == ExprValueKind::RValue) {
                m_OpCodes.emplace_back(OpCodeKind::LdGlobal, declRef->GetIdentifier());

                if (expr->GetResolvedType()->IsReference()) {
                    expr->GetResolvedType()->Reference = false;
                    m_OpCodes.emplace_back(OpCodeKind::Deref, TypeInfoToVMType(expr->GetResolvedType()));
                    expr->GetResolvedType()->Reference = true;
                }
            }
        } else if (declRef->GetKind() == DeclRefKind::Function) {
            m_OpCodes.emplace_back(OpCodeKind::LdFunc, fmt::format("{}()", declRef->GetRawIdentifier()));
        }
    }

    void Emitter::EmitMemberExpr(Expr* expr, ExprValueKind valueKind) {
        MemberExpr* mem = GetNode<MemberExpr>(expr);

        StructDeclaration& sd = std::get<StructDeclaration>(mem->GetParentType()->Data);
        StructDecl* s = GetNode<StructDecl>(sd.SourceDecl);
        
        for (Decl* field : s->GetFields()) {
            if (FieldDecl* fd = GetNode<FieldDecl>(field)) {
                if (fd->GetRawIdentifier() == mem->GetMember()) {
                    RuntimeStructDeclaration& sdecl = m_Structs.at(s->GetIdentifier());

                    EmitExpr(mem->GetParent(), ExprValueKind::LValue);
        
                    if (valueKind == ExprValueKind::LValue) {
                        m_OpCodes.emplace_back(OpCodeKind::LdPtrMember, OpCodeMember(sdecl.FieldIndices.at(fd->GetIdentifier()), {VMTypeKind::Ptr}, TypeInfoToVMType(mem->GetParentType())));
                    } else if (valueKind == ExprValueKind::RValue) {
                        m_OpCodes.emplace_back(OpCodeKind::LdMember, OpCodeMember(sdecl.FieldIndices.at(fd->GetIdentifier()), TypeInfoToVMType(fd->GetResolvedType()), TypeInfoToVMType(mem->GetParentType())));
                    }
                }
            } else if (MethodDecl* md = GetNode<MethodDecl>(field)) {
                if (md->GetRawIdentifier() == mem->GetMember()) {
                    m_OpCodes.emplace_back(OpCodeKind::LdFunc, fmt::format("{}::{}()", sd.Identifier, md->GetIdentifier()));
                }
            }  
        }
    }

    void Emitter::EmitSelfExpr(Expr* expr, ExprValueKind valueKind) {
        SelfExpr* self = GetNode<SelfExpr>(expr);

        ARIA_ASSERT(valueKind == ExprValueKind::LValue, "rvalue of 'self' is not yet supported");

        // Because self is a reference (aka pointer), we want to the load the actual value of it
        m_OpCodes.emplace_back(OpCodeKind::LdArg, static_cast<size_t>(0));
    }

    void Emitter::EmitTemporaryExpr(Expr* expr, ExprValueKind valueKind) {
        TemporaryExpr* temp = GetNode<TemporaryExpr>(expr);

        EmitExpr(temp->GetExpression(), valueKind);

        // Create a new temporary
        m_OpCodes.emplace_back(OpCodeKind::DeclareLocal, m_ActiveStackFrame.LocalCount);

        Declaration d;
        d.Type = temp->GetResolvedType();
        d.Data = m_ActiveStackFrame.LocalCount;
        m_ActiveStackFrame.LocalCount++;

        m_Temporaries.push_back(d);
    }

    void Emitter::EmitCallExpr(Expr* expr, ExprValueKind valueKind) {
        CallExpr* call = GetNode<CallExpr>(expr);

        EmitExpr(call->GetCallee(), call->GetCallee()->GetValueKind());

        for (size_t i = 0; i < call->GetArguments().Size; i++) {
            EmitExpr(call->GetArguments().Items[i], call->GetArguments().Items[i]->GetValueKind());
            m_OpCodes.emplace_back(OpCodeKind::DeclareArg, i);
        }

        size_t retCount = 0;
        TypeInfo* retType = call->GetResolvedType();
        if (retType->Type != PrimitiveType::Void) {
            m_OpCodes.emplace_back(OpCodeKind::Alloca, TypeInfoToVMType(retType));
            retCount = 1;
        }

        m_OpCodes.emplace_back(OpCodeKind::Call, OpCodeCall(call->GetArguments().Size, TypeInfoToVMType(retType)));

        // The only special case is when returning a reference and getting it as an rvalue
        if (valueKind == ExprValueKind::RValue) {
            if (retType->IsReference()) {
                retType->Reference = false;
                m_OpCodes.emplace_back(OpCodeKind::Deref, TypeInfoToVMType(retType));
                retType->Reference = true;
            }
        }
    }

    void Emitter::EmitMethodCallExpr(Expr* expr, ExprValueKind valueKind) {
        MethodCallExpr* call = GetNode<MethodCallExpr>(expr);

        EmitExpr(call->GetCallee(), call->GetCallee()->GetValueKind());

        // Push self
        EmitExpr(call->GetCallee()->GetParent(), ExprValueKind::LValue);
        m_OpCodes.emplace_back(OpCodeKind::DeclareArg, static_cast<size_t>(0));

        for (size_t i = 0; i < call->GetArguments().Size; i++) {
            EmitExpr(call->GetArguments().Items[i], call->GetArguments().Items[i]->GetValueKind());
            m_OpCodes.emplace_back(OpCodeKind::DeclareArg, i + 1);
        }

        size_t retCount = 0;
        TypeInfo* retType = call->GetResolvedType();
        if (retType->Type != PrimitiveType::Void) {
            m_OpCodes.emplace_back(OpCodeKind::Alloca, TypeInfoToVMType(retType));
            retCount = 1;
        }

        // We do size + 1 because self is an implicit parameter that isn't in the argument vector,
        // However the VM certainly does need the actual amount of arguments
        m_OpCodes.emplace_back(OpCodeKind::Call, OpCodeCall(call->GetArguments().Size + 1, TypeInfoToVMType(retType)));

        // The only special case is when returning a reference and getting it as an rvalue
        if (valueKind == ExprValueKind::RValue) {
            if (retType->IsReference()) {
                retType->Reference = false;
                m_OpCodes.emplace_back(OpCodeKind::Deref, TypeInfoToVMType(retType));
                retType->Reference = true;
            }
        }
    }

    void Emitter::EmitParenExpr(Expr* expr, ExprValueKind valueKind) {
        ParenExpr* paren = GetNode<ParenExpr>(expr);
        EmitExpr(paren->GetChildExpr(), paren->GetValueKind());
    }

    void Emitter::EmitImplicitCastExpr(Expr* expr, ExprValueKind valueKind) {
        ImplicitCastExpr* cast = GetNode<ImplicitCastExpr>(expr);
        
        if (cast->GetCastKind() == CastKind::LValueToRValue) {
            return EmitExpr(cast->GetChildExpr(), ExprValueKind::RValue);
        } else {
            EmitExpr(cast->GetChildExpr(), ExprValueKind::RValue);
            m_OpCodes.emplace_back(OpCodeKind::Cast, TypeInfoToVMType(cast->GetResolvedType()));
            return;
        }

        ARIA_UNREACHABLE();
    }

    void Emitter::EmitCastExpr(Expr* expr, ExprValueKind valueKind) {
        CastExpr* cast = GetNode<CastExpr>(expr);

        if (cast->GetCastKind() == CastKind::LValueToRValue) {
            return EmitExpr(cast->GetChildExpr(), ExprValueKind::RValue);
        } else {
            EmitExpr(cast->GetChildExpr(), cast->GetValueKind());
            m_OpCodes.emplace_back(OpCodeKind::Cast, TypeInfoToVMType(cast->GetResolvedType()));
            return;
        }

        ARIA_UNREACHABLE();
    }

    void Emitter::EmitUnaryOperatorExpr(Expr* expr, ExprValueKind valueKind) {
        UnaryOperatorExpr* unop = GetNode<UnaryOperatorExpr>(expr);

        switch (unop->GetUnaryOperator()) {
            case UnaryOperatorKind::Negate: EmitExpr(unop->GetChildExpr(), unop->GetChildExpr()->GetValueKind()); m_OpCodes.emplace_back(OpCodeKind::Neg, TypeInfoToVMType(unop->GetResolvedType())); break;
            default: ARIA_UNREACHABLE();
        }
    }

    void Emitter::EmitBinaryOperatorExpr(Expr* expr, ExprValueKind valueKind) {
        BinaryOperatorExpr* binop = GetNode<BinaryOperatorExpr>(expr);
      
        #define BINOP(_enum, op) case BinaryOperatorKind::_enum: { \
                EmitExpr(binop->GetLHS(), binop->GetLHS()->GetValueKind()); \
                EmitExpr(binop->GetRHS(), binop->GetRHS()->GetValueKind()); \
                m_OpCodes.emplace_back(OpCodeKind::op, TypeInfoToVMType(binop->GetLHS()->GetResolvedType())); \
                break; \
            }
        
        switch (binop->GetBinaryOperator()) {
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
        
            case BinaryOperatorKind::BitAnd: {
                std::string andEnd = fmt::format("and.end_{}", m_AndCounter);
                m_AndCounter++;

                EmitExpr(binop->GetLHS(), binop->GetLHS()->GetValueKind());
                m_OpCodes.emplace_back(OpCodeKind::Jf, andEnd);
                EmitExpr(binop->GetRHS(), binop->GetRHS()->GetValueKind());
                m_OpCodes.emplace_back(OpCodeKind::And, TypeInfoToVMType(binop->GetResolvedType()));
                m_OpCodes.emplace_back(OpCodeKind::Jmp, andEnd);

                m_OpCodes.emplace_back(OpCodeKind::Label, andEnd);

                break;
            }

            case BinaryOperatorKind::BitOr: {
                std::string orEnd = fmt::format("or.end_{}", m_OrCounter);
                m_OrCounter++;

                EmitExpr(binop->GetLHS(), binop->GetLHS()->GetValueKind());
                m_OpCodes.emplace_back(OpCodeKind::Jt, orEnd);
                EmitExpr(binop->GetRHS(), binop->GetRHS()->GetValueKind());
                m_OpCodes.emplace_back(OpCodeKind::Or, TypeInfoToVMType(binop->GetResolvedType()));
                m_OpCodes.emplace_back(OpCodeKind::Jmp, orEnd);

                m_OpCodes.emplace_back(OpCodeKind::Label, orEnd);

                break;
            }

            case BinaryOperatorKind::Eq: {
                EmitExpr(binop->GetLHS(), binop->GetLHS()->GetValueKind());
                EmitExpr(binop->GetRHS(), binop->GetRHS()->GetValueKind());

                m_OpCodes.emplace_back(OpCodeKind::Store);
                EmitExpr(binop->GetLHS(), valueKind);
                break;
            }
        }

        #undef BINOP
    }

    void Emitter::EmitCompoundAssignExpr(Expr* expr, ExprValueKind valueKind) {
        CompoundAssignExpr* compAss = GetNode<CompoundAssignExpr>(expr);

        #define BINOP(_enum, op) case BinaryOperatorKind::Compound##_enum: { \
                EmitExpr(compAss->GetLHS(), ExprValueKind::RValue); \
                EmitExpr(compAss->GetRHS(), compAss->GetRHS()->GetValueKind()); \
                m_OpCodes.emplace_back(OpCodeKind::op, TypeInfoToVMType(compAss->GetLHS()->GetResolvedType())); \
                break; \
            }

        // Load the destination
        EmitExpr(compAss->GetLHS(), ExprValueKind::LValue);

        switch (compAss->GetBinaryOperator()) {
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

        m_OpCodes.emplace_back(OpCodeKind::Store);
        EmitExpr(compAss->GetLHS(), valueKind);
    }

    void Emitter::EmitExpr(Expr* expr, ExprValueKind valueKind) {
        if (GetNode<BooleanConstantExpr>(expr)) {
            return EmitBooleanConstantExpr(expr, valueKind);
        } else if (GetNode<CharacterConstantExpr>(expr)) {
            return EmitCharacterConstantExpr(expr, valueKind);
        } else if (GetNode<IntegerConstantExpr>(expr)) {
            return EmitIntegerConstantExpr(expr, valueKind);
        } else if (GetNode<FloatingConstantExpr>(expr)) {
            return EmitFloatingConstantExpr(expr, valueKind);
        } else if (GetNode<StringConstantExpr>(expr)) {
            return EmitStringConstantExpr(expr, valueKind);
        } else if (GetNode<DeclRefExpr>(expr)) {
            return EmitDeclRefExpr(expr, valueKind);
        } else if (GetNode<MemberExpr>(expr)) {
            return EmitMemberExpr(expr, valueKind);
        } else if (GetNode<SelfExpr>(expr)) {
            return EmitSelfExpr(expr, valueKind);
        } else if (GetNode<TemporaryExpr>(expr)) {
            return EmitTemporaryExpr(expr, valueKind);
        } else if (GetNode<CallExpr>(expr)) {
            return EmitCallExpr(expr, valueKind);
        } else if (GetNode<MethodCallExpr>(expr)) {
            return EmitMethodCallExpr(expr, valueKind);
        } else if (GetNode<ParenExpr>(expr)) {
            return EmitParenExpr(expr, valueKind);
        } else if (GetNode<CastExpr>(expr)) {
            return EmitCastExpr(expr, valueKind);
        } else if (GetNode<ImplicitCastExpr>(expr)) {
            return EmitImplicitCastExpr(expr, valueKind);
        } else if (GetNode<UnaryOperatorExpr>(expr)) {
            return EmitUnaryOperatorExpr(expr, valueKind);
        } else if (GetNode<BinaryOperatorExpr>(expr)) {
            return EmitBinaryOperatorExpr(expr, valueKind);
        } else if (GetNode<CompoundAssignExpr>(expr)) {
            return EmitCompoundAssignExpr(expr, valueKind);
        }

        ARIA_UNREACHABLE();
    }

    void Emitter::EmitTranslationUnitDecl(Decl* decl) {
        TranslationUnitDecl* tu = GetNode<TranslationUnitDecl>(decl);

        for (Stmt* stmt : tu->GetStmts()) {
            EmitStmt(stmt);
        }
    }

    void Emitter::EmitVarDecl(Decl* decl) {
        VarDecl* varDecl = GetNode<VarDecl>(decl);

        if (varDecl->GetDefaultValue()) {
            EmitExpr(varDecl->GetDefaultValue(), varDecl->GetDefaultValue()->GetValueKind());
        } else {
            m_OpCodes.emplace_back(OpCodeKind::Alloca, TypeInfoToVMType(varDecl->GetResolvedType()));
        }

        Declaration d;
        d.Type = varDecl->GetResolvedType();

        if (IsGlobalScope()) {
            m_OpCodes.emplace_back(OpCodeKind::DeclareGlobal, varDecl->GetIdentifier());
            d.Data = varDecl->GetIdentifier();

            m_GlobalScope.DeclaredSymbols.push_back(d);
            m_GlobalScope.DeclaredSymbolMap[varDecl->GetIdentifier()] = m_GlobalScope.DeclaredSymbols.size() - 1;
        } else {
            m_OpCodes.emplace_back(OpCodeKind::DeclareLocal, m_ActiveStackFrame.LocalCount);
            d.Data = m_ActiveStackFrame.LocalCount;
            m_ActiveStackFrame.LocalCount++;

            m_ActiveStackFrame.Scopes.back().DeclaredSymbols.push_back(d);
            m_ActiveStackFrame.Scopes.back().DeclaredSymbolMap[varDecl->GetIdentifier()] = m_ActiveStackFrame.Scopes.back().DeclaredSymbols.size() - 1;
        }
    }
    
    void Emitter::EmitParamDecl(Decl* decl) {
        ParamDecl* param = GetNode<ParamDecl>(decl);
        m_ActiveStackFrame.Parameters[param->GetIdentifier()] = m_ActiveStackFrame.ParameterCount++;
    }

    void Emitter::EmitFunctionDecl(Decl* decl) {
        FunctionDecl* fnDecl = GetNode<FunctionDecl>(decl);

        if (fnDecl->IsExtern()) { return; }

        m_DeclarationsToDeclare.emplace_back(fmt::format("{}()", fnDecl->GetRawIdentifier()), decl);
    }

    void Emitter::EmitStructDecl(Decl* decl) {
        StructDecl* s = GetNode<StructDecl>(decl);
        
        RuntimeStructDeclaration sd;

        for (Decl* field : s->GetFields()) {
            if (FieldDecl* fd = GetNode<FieldDecl>(field)) {
                sd.FieldIndices[fd->GetIdentifier()] = sd.FieldIndices.size();
            } else if (MethodDecl* md = GetNode<MethodDecl>(field)) {
                m_DeclarationsToDeclare.emplace_back(fmt::format("{}::{}()", s->GetIdentifier(), md->GetIdentifier()), md);
            }
        }

        m_Structs[s->GetIdentifier()] = sd;
        m_DeclarationsToDeclare.emplace_back(s->GetIdentifier(), decl);
    }

    void Emitter::EmitDecl(Decl* decl) {
        if (GetNode<TranslationUnitDecl>(decl)) {
            return EmitTranslationUnitDecl(decl);
        } else if (GetNode<VarDecl>(decl)) {
            return EmitVarDecl(decl);
        } else if (GetNode<FunctionDecl>(decl)) {
            return EmitFunctionDecl(decl);
        } else if (GetNode<StructDecl>(decl)) {
            return EmitStructDecl(decl);
        }

        ARIA_UNREACHABLE();
    }

    void Emitter::EmitCompoundStmt(Stmt* stmt) {
        CompoundStmt* compound = GetNode<CompoundStmt>(stmt);

        for (Stmt* stmt : compound->GetStmts()) {
            EmitStmt(stmt);
        }
    }

    void Emitter::EmitWhileStmt(Stmt* stmt) {
        WhileStmt* wh = GetNode<WhileStmt>(stmt);

        std::string loopStart = fmt::format("while.start_{}", m_WhileCounter);
        std::string loopEnd = fmt::format("while.end_{}", m_WhileCounter);
        m_WhileCounter++;

        m_OpCodes.emplace_back(OpCodeKind::Label, loopStart);
        EmitExpr(wh->GetCondition(), wh->GetCondition()->GetValueKind());
        m_OpCodes.emplace_back(OpCodeKind::Jf, loopEnd);
        m_OpCodes.emplace_back(OpCodeKind::Pop);
        EmitStmt(wh->GetBody());

        m_OpCodes.emplace_back(OpCodeKind::Jmp, loopStart);
        m_OpCodes.emplace_back(OpCodeKind::Label, loopEnd);
        m_OpCodes.emplace_back(OpCodeKind::Pop);
    }

    void Emitter::EmitDoWhileStmt(Stmt* stmt) {
        DoWhileStmt* wh = GetNode<DoWhileStmt>(stmt);

        std::string loopStart = fmt::format("dowhile.start_{}", m_DoWhileCounter);
        m_DoWhileCounter++;

        m_OpCodes.emplace_back(OpCodeKind::Label, loopStart);
        EmitStmt(wh->GetBody());

        EmitExpr(wh->GetCondition(), wh->GetCondition()->GetValueKind());
        m_OpCodes.emplace_back(OpCodeKind::Jt, loopStart);
    }

    void Emitter::EmitForStmt(Stmt* stmt) {
        ForStmt* fs = GetNode<ForStmt>(stmt);

        std::string loopStart = fmt::format("for.start_{}", m_ForCounter);
        std::string loopEnd = fmt::format("for.end_{}", m_ForCounter);
        m_ForCounter++;

        PushScope();
        if (fs->GetPrologue()) { EmitStmt(fs->GetPrologue()); }

        m_OpCodes.emplace_back(OpCodeKind::Label, loopStart);
        if (fs->GetCondition()) {
            EmitExpr(fs->GetCondition(), fs->GetCondition()->GetValueKind());
            m_OpCodes.emplace_back(OpCodeKind::Jf, loopEnd);
        }

        EmitStmt(fs->GetBody());
            
        if (fs->GetEpilogue()) {
            EmitExpr(fs->GetEpilogue(), fs->GetEpilogue()->GetValueKind());
            m_OpCodes.emplace_back(OpCodeKind::Pop);
        }

        m_OpCodes.emplace_back(OpCodeKind::Jmp, loopStart);
        m_OpCodes.emplace_back(OpCodeKind::Label, loopEnd);
        m_OpCodes.emplace_back(OpCodeKind::Pop);

        PopScope();
    }

    void Emitter::EmitIfStmt(Stmt* stmt) {
        IfStmt* ifs = GetNode<IfStmt>(stmt);

        EmitExpr(ifs->GetCondition(), ifs->GetCondition()->GetValueKind());
        std::string ifBody = fmt::format("if.body_{}", m_IfCounter);
        std::string ifEnd = fmt::format("if.end_{}", m_IfCounter);

        m_OpCodes.emplace_back(OpCodeKind::Jt, ifBody);
        m_OpCodes.emplace_back(OpCodeKind::Jmp, ifEnd);

        m_OpCodes.emplace_back(OpCodeKind::Label, ifBody);
        EmitStmt(ifs->GetBody());
        m_OpCodes.emplace_back(OpCodeKind::Label, ifEnd);

        m_IfCounter++;
    }

    void Emitter::EmitReturnStmt(Stmt* stmt) {
        ReturnStmt* ret = GetNode<ReturnStmt>(stmt);
        if (ret->GetValue()) {
            m_OpCodes.emplace_back(OpCodeKind::LdPtrRet);
            EmitExpr(ret->GetValue(), ret->GetValue()->GetValueKind());
            m_OpCodes.emplace_back(OpCodeKind::Store);
        }
        
        m_OpCodes.emplace_back(OpCodeKind::PopSF);
        m_OpCodes.emplace_back(OpCodeKind::Ret);
    }

    void Emitter::EmitStmt(Stmt* stmt) {
        if (GetNode<CompoundStmt>(stmt)) {
            PushScope();
            EmitCompoundStmt(stmt);
            PopScope();
            return;
        } else if (GetNode<WhileStmt>(stmt)) {
            EmitWhileStmt(stmt);
            return;
        } else if (GetNode<DoWhileStmt>(stmt)) {
            EmitDoWhileStmt(stmt);
            return;
        } else if (GetNode<ForStmt>(stmt)) {
            EmitForStmt(stmt);
            return;
        } else if (GetNode<IfStmt>(stmt)) {
            EmitIfStmt(stmt);
            return;
        } else if (GetNode<ReturnStmt>(stmt)) {
            EmitReturnStmt(stmt);
            return;
        } else if (Expr* expr = GetNode<Expr>(stmt)) {
            EmitExpr(expr, expr->GetValueKind());

            EmitDestructors(m_Temporaries);
            m_Temporaries.clear();

            if (!expr->GetResolvedType()->IsVoid()) {
                m_OpCodes.emplace_back(OpCodeKind::Pop);
            }

            return;
        } else if (Decl* decl = GetNode<Decl>(stmt)) {
            EmitDecl(decl);
            return;
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

            if (decl.Type->Type == PrimitiveType::String) {
                if (std::holds_alternative<size_t>(decl.Data)) {
                    m_OpCodes.emplace_back(OpCodeKind::LdPtrLocal, std::get<size_t>(decl.Data));
                } else if (std::holds_alternative<std::string>(decl.Data)) {
                    m_OpCodes.emplace_back(OpCodeKind::LdPtrGlobal, std::get<std::string>(decl.Data));
                }

                m_OpCodes.emplace_back(OpCodeKind::DestructStr);
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
        m_WhileCounter = 0;
        m_DoWhileCounter = 0;
        m_IfCounter = 0;
    }

    void Emitter::PushScope() {
        m_ActiveStackFrame.Scopes.emplace_back();
    }

    void Emitter::PopScope() {
        EmitDestructors(m_ActiveStackFrame.Scopes.back().DeclaredSymbols);
        m_ActiveStackFrame.Scopes.pop_back();
    }

    void Emitter::EmitDeclarations() {
        for (const auto&[name, decl] : m_DeclarationsToDeclare) {
            if (FunctionDecl* fnDecl = GetNode<FunctionDecl>(decl)) {
                if (fnDecl->GetBody()) {
                    m_OpCodes.emplace_back(OpCodeKind::Function, name);
                    m_OpCodes.emplace_back(OpCodeKind::Label, "_entry$");

                    m_OpCodes.emplace_back(OpCodeKind::PushSF);
                    PushStackFrame(name);
                    
                    size_t returnSlot = (fnDecl->GetResolvedType()->Type == PrimitiveType::Void) ? 0 : 1;
                    
                    for (ParamDecl* p : fnDecl->GetParameters()) {
                        EmitParamDecl(p);
                    }
                    
                    EmitCompoundStmt(fnDecl->GetBody());

                    if (m_OpCodes.back().Kind != OpCodeKind::Ret) {
                        m_OpCodes.emplace_back(OpCodeKind::PopSF);
                        m_OpCodes.emplace_back(OpCodeKind::Ret);
                    }

                    PopStackFrame();

                    CompilerReflectionDeclaration d;
                    d.Type = TypeInfoToVMType(std::get<FunctionDeclaration>(fnDecl->GetResolvedType()->Data).ReturnType);
                    d.Kind = ReflectionKind::Function;

                    m_ReflectionData.Declarations[name] = d;
                }
            } else if (MethodDecl* mDecl = GetNode<MethodDecl>(decl)) {
                m_OpCodes.emplace_back(OpCodeKind::Function, name);
                m_OpCodes.emplace_back(OpCodeKind::Label, "_entry$");

                m_OpCodes.emplace_back(OpCodeKind::PushSF);
                PushStackFrame(name);
                m_ActiveStackFrame.ParameterCount++; // Used for "self"
                
                size_t returnSlot = (mDecl->GetResolvedType()->Type == PrimitiveType::Void) ? 0 : 1;
                
                for (ParamDecl* p : mDecl->GetParameters()) {
                    EmitParamDecl(p);
                }
                
                EmitCompoundStmt(mDecl->GetBody());

                if (m_OpCodes.back().Kind != OpCodeKind::Ret) {
                    m_OpCodes.emplace_back(OpCodeKind::PopSF);
                    m_OpCodes.emplace_back(OpCodeKind::Ret);
                }

                PopStackFrame();

                CompilerReflectionDeclaration d;
                d.Type = TypeInfoToVMType(std::get<FunctionDeclaration>(mDecl->GetResolvedType()->Data).ReturnType);
                d.Kind = ReflectionKind::Function;

                m_ReflectionData.Declarations[name] = d;
            } else if (StructDecl* stDecl = GetNode<StructDecl>(decl)) {
                std::vector<VMType> fields;
                fields.reserve(stDecl->GetFields().Size);

                for (Decl* field : stDecl->GetFields()) {
                    if (FieldDecl* fd = GetNode<FieldDecl>(field)) {
                        fields.push_back(TypeInfoToVMType(fd->GetResolvedType()));
                    }
                }

                m_OpCodes.emplace_back(OpCodeKind::Struct, OpCodeStruct(fields, std::string_view(stDecl->GetRawIdentifier().Data(), stDecl->GetRawIdentifier().Size())));
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
