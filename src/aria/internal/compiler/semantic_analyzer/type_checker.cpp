#include "aria/internal/compiler/semantic_analyzer/type_checker.hpp"
#include "aria/internal/compiler/ast/ast.hpp"

namespace Aria::Internal {

    TypeChecker::TypeChecker(CompilationContext* ctx) {
        m_Context = ctx;

        m_RootASTNode = ctx->GetRootASTNode();
    }

    void TypeChecker::CheckImpl() {
        // We always want to have at least one map for declarations (global space)
        m_Declarations.resize(1);

        HandleStmt(m_RootASTNode);
    }

    Expr* TypeChecker::HandleBooleanConstantExpr(Expr* expr) {
        expr->SetValueKind(ExprValueKind::RValue);
        return expr;
    }

    Expr* TypeChecker::HandleCharacterConstantExpr(Expr* expr) {
        expr->SetValueKind(ExprValueKind::RValue);
        return expr;
    }

    Expr* TypeChecker::HandleIntegerConstantExpr(Expr* expr) {
        expr->SetValueKind(ExprValueKind::RValue);
        return expr;
    }

    Expr* TypeChecker::HandleFloatingConstantExpr(Expr* expr) {
        expr->SetValueKind(ExprValueKind::RValue);
        return expr;
    }

    Expr* TypeChecker::HandleStringConstantExpr(Expr* expr) {
        expr->SetValueKind(ExprValueKind::RValue);
        ARIA_ASSERT(false, "todo!");
    }

    Expr* TypeChecker::HandleDeclRefExpr(Expr* expr) {
        DeclRefExpr* ref = GetNode<DeclRefExpr>(expr);

        std::string ident = ref->GetIdentifier();

        if (m_ActiveStruct) {
            StructDecl* s = GetNode<StructDecl>(std::get<StructDeclaration>(m_ActiveStruct->Data).SourceDecl);
            ARIA_ASSERT(s != nullptr, "how");

            for (Decl* d : s->GetFields()) {
                if (FieldDecl* fd = GetNode<FieldDecl>(d)) {
                    if (fd->GetRawIdentifier() == ref->GetRawIdentifier()) {
                        SelfExpr* self = m_Context->Allocate<SelfExpr>(m_Context, ref->Loc, ref->Range);
                        self->SetResolvedType(m_ActiveStruct);
                        self->SetValueKind(ExprValueKind::LValue);

                        MemberExpr* mem = m_Context->Allocate<MemberExpr>(m_Context, ref->Loc, ref->Range, ref->GetRawIdentifier(), self);
                        mem->SetResolvedType(fd->GetResolvedType());
                        mem->SetParentType(self->GetResolvedType());
                        mem->SetValueKind(ExprValueKind::LValue);
                        return mem;
                    }
                }
            }
        }
        
        for (size_t i = m_Declarations.size(); i > 0; i--) {
            auto& it = m_Declarations.at(i - 1);

            if (it.contains(ident)) {
                ref->SetResolvedType(it.at(ident).ResolvedType);
                ref->SetKind(it.at(ident).DeclKind);
                ref->SetValueKind(ExprValueKind::LValue);
                return ref;
            }
        }

        m_Context->ReportCompilerError(ref->Loc, ref->Range, fmt::format("Undeclared identifier \"{}\"", ref->GetRawIdentifier()));
        ref->SetResolvedType(TypeInfo::Create(m_Context, PrimitiveType::Void, false));
        ref->SetValueKind(ExprValueKind::LValue);
        return ref;
    }

    Expr* TypeChecker::HandleMemberExpr(Expr* expr) {
        MemberExpr* mem = GetNode<MemberExpr>(expr);

        mem->SetParent(HandleExpr(mem->GetParent()));
        TypeInfo* parentType = mem->GetParent()->GetResolvedType();
        TypeInfo* memberType = nullptr;
        StructDeclaration& sd = std::get<StructDeclaration>(parentType->Data);

        StructDecl* s = GetNode<StructDecl>(sd.SourceDecl);

        for (Decl* field : s->GetFields()) {
            if (FieldDecl* fd = GetNode<FieldDecl>(field)) {
                if (fd->GetRawIdentifier() == mem->GetMember()) {
                    memberType = fd->GetResolvedType();
                }
            } else if (MethodDecl* md = GetNode<MethodDecl>(field)) {
                if (md->GetRawIdentifier() == mem->GetMember()) {
                    memberType = md->GetResolvedType();
                }
            }
        }

        if (!memberType) {
            ARIA_ASSERT(false, "todo: add error msg");
        }

        mem->SetParentType(parentType);
        mem->SetResolvedType(memberType);
        mem->SetValueKind(ExprValueKind::LValue);
        return mem;
    }

    Expr* TypeChecker::HandleCallExpr(Expr* expr) {
        CallExpr* call = GetNode<CallExpr>(expr);

        TypeInfo* calleeType = HandleExpr(call->GetCallee())->GetResolvedType();
        FunctionDeclaration& fnDecl = std::get<FunctionDeclaration>(calleeType->Data);

        if (fnDecl.ParamTypes.Size != call->GetArguments().Size) {
            ARIA_ASSERT(false, "todo: error msg");
        }

        for (size_t i = 0; i < fnDecl.ParamTypes.Size; i++) {
            call->GetArguments().Items[i] = HandleInitializer(call->GetArguments().Items[i], fnDecl.ParamTypes.Items[i]);
        }

        call->SetResolvedType(fnDecl.ReturnType);
        call->SetValueKind((fnDecl.ReturnType->IsReference()) ? ExprValueKind::LValue : ExprValueKind::RValue);
        return call;
    }

    Expr* TypeChecker::HandleMethodCallExpr(Expr* expr) {
        MethodCallExpr* call = GetNode<MethodCallExpr>(expr);
        TypeInfo* calleeType = HandleExpr(call->GetCallee())->GetResolvedType();

        FunctionDeclaration& fnDecl = std::get<FunctionDeclaration>(calleeType->Data);

        if (fnDecl.ParamTypes.Size != call->GetArguments().Size) {
            ARIA_ASSERT(false, "todo: error msg");
        }

        for (size_t i = 0; i < fnDecl.ParamTypes.Size; i++) {
            TypeInfo* paramType = fnDecl.ParamTypes.Items[i];
            TypeInfo* argType = HandleExpr(call->GetArguments().Items[i])->GetResolvedType();

            ConversionCost cost = GetConversionCost(paramType, argType, call->GetArguments().Items[i]->GetValueKind());
            if (cost.CastNeeded) {
                if (cost.ImplicitCastPossible) {
                    call->SetArgument(i, InsertImplicitCast(paramType, argType, call->GetArguments().Items[i], cost.CaKind));
                } else {
                    ARIA_ASSERT(false, "todo: error msg");
                }
            }
        }

        call->SetResolvedType(fnDecl.ReturnType);
        return call;
    }

    Expr* TypeChecker::HandleParenExpr(Expr* expr) {
        ParenExpr* paren = GetNode<ParenExpr>(expr);
        paren->SetChildExpr(HandleExpr(paren->GetChildExpr()));
        paren->SetResolvedType(paren->GetChildExpr()->GetResolvedType());
        paren->SetValueKind(paren->GetChildExpr()->GetValueKind());
        return paren;
    }

    Expr* TypeChecker::HandleCastExpr(Expr* expr) {
        CastExpr* cast = GetNode<CastExpr>(expr);
        cast->SetValueKind(ExprValueKind::RValue);
        cast->SetChildExpr(HandleExpr(cast->GetChildExpr()));
        TypeInfo* srcType = cast->GetChildExpr()->GetResolvedType();
        TypeInfo* dstType = GetTypeInfoFromString(cast->GetParsedType());

        ConversionCost cost = GetConversionCost(dstType, srcType, cast->GetChildExpr()->GetValueKind());

        if (cost.CastNeeded) {
            if (cost.ExplicitCastPossible) {
                cast->SetResolvedType(dstType);
                cast->SetCastType(cost.CaKind);
            } else {
                ARIA_ASSERT(false, "todo: add error message");
            }
        }

        return cast;
    }

    Expr* TypeChecker::HandleUnaryOperatorExpr(Expr* expr) {
        UnaryOperatorExpr* unop = GetNode<UnaryOperatorExpr>(expr);

        unop->SetChildExpr(HandleExpr(unop->GetChildExpr()));
        TypeInfo* type = unop->GetChildExpr()->GetResolvedType();

        ConversionCost cost = GetConversionCost(type, type, unop->GetChildExpr()->GetValueKind());
        if (cost.CastNeeded) {
            if (cost.ImplicitCastPossible) {
                unop->SetChildExpr(InsertImplicitCast(type, type, unop->GetChildExpr(), cost.CaKind));
            } else {
                ARIA_ASSERT(false, "todo: TypeChecker::HandleVarDecl() error");
            }
        }

        switch (unop->GetUnaryOperator()) {
            case UnaryOperatorKind::Negate: {
                ARIA_ASSERT(type->IsNumeric(), "todo: add error message");
                unop->SetResolvedType(type);
                return unop;
            }
        }

        ARIA_UNREACHABLE();
    }

    Expr* TypeChecker::HandleBinaryOperatorExpr(Expr* expr) {
        BinaryOperatorExpr* binop = GetNode<BinaryOperatorExpr>(expr);

        binop->SetLHS(HandleExpr(binop->GetLHS()));
        binop->SetRHS(HandleExpr(binop->GetRHS()));

        Expr* LHS = binop->GetLHS();
        Expr* RHS = binop->GetRHS();

        TypeInfo* LHSType = LHS->GetResolvedType();
        TypeInfo* RHSType = RHS->GetResolvedType();

        switch (binop->GetBinaryOperator()) {
            case BinaryOperatorKind::Add:
            case BinaryOperatorKind::Sub:
            case BinaryOperatorKind::Mul:
            case BinaryOperatorKind::Div:
            case BinaryOperatorKind::Mod:
            case BinaryOperatorKind::Less:
            case BinaryOperatorKind::LessOrEq:
            case BinaryOperatorKind::Greater:
            case BinaryOperatorKind::GreaterOrEq:
            case BinaryOperatorKind::IsEq: 
            case BinaryOperatorKind::IsNotEq: {
                if (!LHSType->IsNumeric()) {
                    m_Context->ReportCompilerError(LHS->Loc, LHS->Range, fmt::format("expression must be of a numeric type but is of type '{}'", TypeInfoToString(LHSType)));
                }

                if (!RHSType->IsNumeric()) {
                    m_Context->ReportCompilerError(RHS->Loc, RHS->Range, fmt::format("expression must be of a numeric type but is of type '{}'", TypeInfoToString(RHSType)));
                }

                // See which conversion would be better
                ConversionCost costLHS = GetConversionCost(LHSType, RHSType, RHS->GetValueKind()); // Cast to LHS
                ConversionCost costRHS = GetConversionCost(RHSType, LHSType, LHS->GetValueKind()); // Cast to RHS

                if (costLHS.CastNeeded || costRHS.CastNeeded) {
                    bool lhsCastNeeded = costLHS.CastNeeded;
                    bool rhsCastNeeded = costRHS.CastNeeded;

                    if (costLHS.CoKind == ConversionKind::LValueToRValue) {
                        binop->SetRHS(InsertImplicitCast(LHSType, RHSType, RHS, costLHS.CaKind));
                        RHSType = LHSType;
                        lhsCastNeeded = false;
                    }
                    
                    if (costRHS.CoKind == ConversionKind::LValueToRValue) {
                        binop->SetLHS(InsertImplicitCast(RHSType, LHSType, LHS, costRHS.CaKind));
                        LHSType = RHSType;
                        rhsCastNeeded = false;
                    }

                    if (lhsCastNeeded || rhsCastNeeded) {
                        if (costLHS.CoKind == ConversionKind::Promotion) {
                            binop->SetRHS(InsertImplicitCast(LHSType, RHSType, RHS, costLHS.CaKind));
                            RHSType = LHSType;
                        } else if (costRHS.CoKind == ConversionKind::Promotion) {
                            binop->SetLHS(InsertImplicitCast(RHSType, LHSType, LHS, costRHS.CaKind));
                            LHSType = RHSType;
                        } else {
                            m_Context->ReportCompilerError(binop->Loc, binop->Range, fmt::format("mismatched types '{}' and '{}'", TypeInfoToString(LHSType), TypeInfoToString(RHSType)));
                        }
                    }
                }

                if (binop->GetBinaryOperator() == BinaryOperatorKind::Less ||
                    binop->GetBinaryOperator() == BinaryOperatorKind::LessOrEq ||
                    binop->GetBinaryOperator() == BinaryOperatorKind::Greater ||
                    binop->GetBinaryOperator() == BinaryOperatorKind::GreaterOrEq ||
                    binop->GetBinaryOperator() == BinaryOperatorKind::IsEq ||
                    binop->GetBinaryOperator() == BinaryOperatorKind::IsNotEq) 
                {
                    TypeInfo* boolType = TypeInfo::Create(m_Context, PrimitiveType::Bool, false);
                    binop->SetResolvedType(boolType);
                    binop->SetValueKind(ExprValueKind::RValue);
                    return binop;
                }

                binop->SetResolvedType(LHSType);
                binop->SetValueKind(ExprValueKind::RValue);
                return binop;
            }

            case BinaryOperatorKind::Eq: {
                if (binop->GetLHS()->GetValueKind() != ExprValueKind::LValue) {
                    m_Context->ReportCompilerError(LHS->Loc, LHS->Range, "expression must be a modifiable lvalue");
                }

                ConversionCost cost = GetConversionCost(LHSType, RHSType, binop->GetRHS()->GetValueKind());

                if (cost.CastNeeded) {
                    if (cost.ImplicitCastPossible) {
                        binop->SetRHS(InsertImplicitCast(LHSType, RHSType, RHS, cost.CaKind));
                        RHSType = LHSType;
                    } else {
                        m_Context->ReportCompilerError(binop->Loc, binop->Range, fmt::format("cannot implicitly convert from '{}' to '{}'", TypeInfoToString(RHSType), TypeInfoToString(LHSType)));
                    }
                }

                binop->SetResolvedType(LHSType);
                binop->SetValueKind(ExprValueKind::LValue);
                return binop;
            }

            case BinaryOperatorKind::BitAnd:
            case BinaryOperatorKind::BitOr: {
                TypeInfo* boolType = TypeInfo::Create(m_Context, PrimitiveType::Bool, false);

                ConversionCost costLHS = GetConversionCost(boolType, LHSType, LHS->GetValueKind());
                ConversionCost costRHS = GetConversionCost(boolType, RHSType, RHS->GetValueKind());

                if (costLHS.CastNeeded) {
                    if (costLHS.ImplicitCastPossible) {
                        binop->SetLHS(InsertImplicitCast(boolType, LHSType, LHS, costLHS.CaKind));
                    } else {
                        m_Context->ReportCompilerError(LHS->Loc, LHS->Range, fmt::format("cannot implicitly convert from '{}' to 'bool'", TypeInfoToString(LHSType)));
                    }
                }

                if (costRHS.CastNeeded) {
                    if (costRHS.ImplicitCastPossible) {
                        binop->SetRHS(InsertImplicitCast(boolType, RHSType, RHS, costRHS.CaKind));
                    } else {
                        m_Context->ReportCompilerError(LHS->Loc, LHS->Range, fmt::format("cannot implicitly convert from '{}' to 'bool'", TypeInfoToString(RHSType)));
                    }
                }

                binop->SetResolvedType(boolType);
                binop->SetValueKind(ExprValueKind::RValue);
                return binop;
            }
        }

        ARIA_UNREACHABLE();
    }

    Expr* TypeChecker::HandleCompoundAssignExpr(Expr* expr) {
        CompoundAssignExpr* compAss = GetNode<CompoundAssignExpr>(expr);
        
        compAss->SetLHS(HandleExpr(compAss->GetLHS()));
        compAss->SetRHS(HandleExpr(compAss->GetRHS()));

        Expr* LHS = compAss->GetLHS();
        Expr* RHS = compAss->GetRHS();

        TypeInfo* LHSType = LHS->GetResolvedType();
        TypeInfo* RHSType = RHS->GetResolvedType();

        if (compAss->GetLHS()->GetValueKind() != ExprValueKind::LValue) {
            m_Context->ReportCompilerError(compAss->GetLHS()->Loc, compAss->GetLHS()->Range, "Expression must be a modifiable lvalue");
        }

        ConversionCost cost = GetConversionCost(LHSType, RHSType, compAss->GetRHS()->GetValueKind());

        if (cost.CastNeeded) {
            if (cost.ImplicitCastPossible) {
                compAss->SetRHS(InsertImplicitCast(LHSType, RHSType, RHS, cost.CaKind));
                RHSType = LHSType;
            } else {
                m_Context->ReportCompilerError(compAss->GetRHS()->Loc, compAss->GetRHS()->Range, fmt::format("Cannot implicitly convert from '{}' to '{}'", TypeInfoToString(RHSType), TypeInfoToString(LHSType)));
            }
        }

        compAss->SetResolvedType(LHSType);
        compAss->SetValueKind(ExprValueKind::LValue);
        return compAss;
    }

    Expr* TypeChecker::HandleExpr(Expr* expr) {
        if (GetNode<BooleanConstantExpr>(expr)) {
            return HandleBooleanConstantExpr(expr);
        } else if (GetNode<CharacterConstantExpr>(expr)) {
            return HandleCharacterConstantExpr(expr);
        } else if (GetNode<IntegerConstantExpr>(expr)) {
            return HandleIntegerConstantExpr(expr);
        } else if (GetNode<FloatingConstantExpr>(expr)) {
            return HandleFloatingConstantExpr(expr);
        } else if (GetNode<StringConstantExpr>(expr)) {
            return HandleStringConstantExpr(expr);
        } else if (GetNode<DeclRefExpr>(expr)) {
            return HandleDeclRefExpr(expr);
        } else if (GetNode<MemberExpr>(expr)) {
            return HandleMemberExpr(expr);
        } else if (GetNode<CallExpr>(expr)) {
            return HandleCallExpr(expr);
        } else if (GetNode<MethodCallExpr>(expr)) {
            return HandleMethodCallExpr(expr);
        } else if (GetNode<ParenExpr>(expr)) {
            return HandleParenExpr(expr);
        } else if (GetNode<CastExpr>(expr)) {
            return HandleCastExpr(expr);
        } else if (GetNode<UnaryOperatorExpr>(expr)) {
            return HandleUnaryOperatorExpr(expr);
        } else if (GetNode<BinaryOperatorExpr>(expr)) {
            return HandleBinaryOperatorExpr(expr);
        } else if (GetNode<CompoundAssignExpr>(expr)) {
            return HandleCompoundAssignExpr(expr);
        }

        ARIA_UNREACHABLE();
    }

    void TypeChecker::HandleTranslationUnitDecl(Decl* decl) {
        TranslationUnitDecl* tu = GetNode<TranslationUnitDecl>(decl);       

        for (Stmt* stmt : tu->GetStmts()) {
            HandleStmt(stmt);
        }
    }

    void TypeChecker::HandleVarDecl(Decl* decl) {
        VarDecl* varDecl = GetNode<VarDecl>(decl);

        TypeInfo* resolvedType = GetTypeInfoFromString(varDecl->GetParsedType());
        varDecl->SetResolvedType(resolvedType);

        varDecl->SetDefaultValue(HandleInitializer(varDecl->GetDefaultValue(), resolvedType));

        DeclRefKind kind = DeclRefKind::LocalVar;
        if (m_Declarations.size() == 1) {
            kind = DeclRefKind::GlobalVar;
        }
        
        std::string ident = varDecl->GetIdentifier();
        m_Declarations.back()[ident] = { varDecl->GetResolvedType(), decl, kind };
    }

    void TypeChecker::HandleParamDecl(Decl* decl) {
        ParamDecl* paramDecl = GetNode<ParamDecl>(decl);

        TypeInfo* resolvedType = GetTypeInfoFromString(paramDecl->GetParsedType());
        paramDecl->SetResolvedType(resolvedType);

        std::string ident = paramDecl->GetIdentifier();
        m_Declarations.back()[ident] = { paramDecl->GetResolvedType(), decl, DeclRefKind::ParamVar };
    }

    void TypeChecker::HandleFunctionDecl(Decl* decl) {
        FunctionDecl* fnDecl = GetNode<FunctionDecl>(decl);

        TypeInfo* returnType = GetTypeInfoFromString(fnDecl->GetParsedType());
        TinyVector<TypeInfo*> paramTypes;
        m_ActiveReturnType = returnType;

        m_Declarations.emplace_back();

        for (ParamDecl* p : fnDecl->GetParameters()) {
            HandleParamDecl(p);
            TypeInfo* pType = p->GetResolvedType();
            paramTypes.Append(m_Context, pType);
        }

        // We make the function visible to itself by declaring it before the body
        FunctionDeclaration fd;
        fd.ParamTypes = paramTypes;
        fd.ReturnType = returnType;
        
        TypeInfo* resolvedType = TypeInfo::Create(m_Context, PrimitiveType::Function, false, fd);
        fnDecl->SetResolvedType(resolvedType);

        std::string ident = fnDecl->GetIdentifier();
        m_Declarations.front()[ident] = { fnDecl->GetResolvedType(), decl, DeclRefKind::Function };

        if (fnDecl->GetBody()) {
            HandleCompoundStmt(fnDecl->GetBody());
        }

        m_Declarations.pop_back();
        m_ActiveReturnType = nullptr;
    }

    void TypeChecker::HandleStructDecl(Decl* decl) {
        StructDecl* s = GetNode<StructDecl>(decl);

        StructDeclaration d;
        d.Identifier = s->GetRawIdentifier();
        d.SourceDecl = s;

        std::vector<MethodDecl*> methods;

        TypeInfo* structType = TypeInfo::Create(m_Context, PrimitiveType::Structure, false, d);

        for (Decl* field : s->GetFields()) {
            if (FieldDecl* fd = GetNode<FieldDecl>(field)) {
                fd->SetResolvedType(GetTypeInfoFromString(fd->GetParsedType()));
            } else if (MethodDecl* md = GetNode<MethodDecl>(field)) {
                methods.push_back(md);
            }
        }

        m_DeclaredTypes[s->GetIdentifier()] = structType;

        m_ActiveStruct = TypeInfo::Create(m_Context, structType->Type, true, structType->Data);

        for (MethodDecl* md : methods) {
            TypeInfo* returnType = GetTypeInfoFromString(md->GetParsedType());
            TinyVector<TypeInfo*> paramTypes;
            m_ActiveReturnType = returnType;
            

            m_Declarations.emplace_back();

            for (ParamDecl* p : md->GetParameters()) {
                HandleParamDecl(p);
                TypeInfo* pType = p->GetResolvedType();
                paramTypes.Append(m_Context, pType);
            }

            // We make the function visible to itself by declaring it before the body
            FunctionDeclaration fd;
            fd.ParamTypes = paramTypes;
            fd.ReturnType = returnType;
            
            TypeInfo* resolvedType = TypeInfo::Create(m_Context, PrimitiveType::Function, false, fd);
            md->SetResolvedType(resolvedType);

            std::string ident = md->GetIdentifier();
            m_Declarations.front()[ident] = { md->GetResolvedType(), decl, DeclRefKind::Function };

            if (md->GetBody()) {
                HandleCompoundStmt(md->GetBody());
            }

            m_Declarations.pop_back();
            m_ActiveReturnType = nullptr;
        }

        m_ActiveStruct = nullptr;
    }

    void TypeChecker::HandleDecl(Decl* decl) {
        if (GetNode<TranslationUnitDecl>(decl)) {
            return HandleTranslationUnitDecl(decl);
        } else if (GetNode<VarDecl>(decl)) {
            return HandleVarDecl(decl);
        } else if (GetNode<ParamDecl>(decl)) {
            return HandleParamDecl(decl);
        } else if (GetNode<FunctionDecl>(decl)) {
            return HandleFunctionDecl(decl);
        } else if (GetNode<StructDecl>(decl)) {
            return HandleStructDecl(decl);
        }

        ARIA_UNREACHABLE();
    }

    void TypeChecker::HandleCompoundStmt(Stmt* stmt) {
        CompoundStmt* compound = GetNode<CompoundStmt>(stmt);

        for (Stmt* s : compound->GetStmts()) {
            HandleStmt(s);
        }
    }

    void TypeChecker::HandleWhileStmt(Stmt* stmt) {
        WhileStmt* wh = GetNode<WhileStmt>(stmt);

        TypeInfo* type = HandleExpr(wh->GetCondition())->GetResolvedType();
        TypeInfo* boolType = TypeInfo::Create(m_Context, PrimitiveType::Bool, false);

        ConversionCost cost = GetConversionCost(boolType, type, wh->GetCondition()->GetValueKind());
        if (cost.CastNeeded) {
            if (cost.ImplicitCastPossible) {
                wh->SetCondition(InsertImplicitCast(boolType, type, wh->GetCondition(), cost.CaKind));
            } else {
                ARIA_ASSERT(false, "todo");
            }
        }

        HandleStmt(wh->GetBody());
    }

    void TypeChecker::HandleDoWhileStmt(Stmt* stmt) {
        DoWhileStmt* wh = GetNode<DoWhileStmt>(stmt);

        TypeInfo* type = HandleExpr(wh->GetCondition())->GetResolvedType();
        TypeInfo* boolType = TypeInfo::Create(m_Context, PrimitiveType::Bool, false);

        ConversionCost cost = GetConversionCost(boolType, type, wh->GetCondition()->GetValueKind());
        if (cost.CastNeeded) {
            if (cost.ImplicitCastPossible) {
                wh->SetCondition(InsertImplicitCast(boolType, type, wh->GetCondition(), cost.CaKind));
            } else {
                ARIA_ASSERT(false, "todo");
            }
        }

        HandleStmt(wh->GetBody());
    }

    void TypeChecker::HandleForStmt(Stmt* stmt) {
        ForStmt* fs = GetNode<ForStmt>(stmt);

        m_Declarations.emplace_back();

        if (fs->GetPrologue()) { HandleStmt(fs->GetPrologue()); }
        if (fs->GetCondition()) { fs->SetCondition(HandleExpr(fs->GetCondition())); }
        if (fs->GetEpilogue()) { fs->SetEpilogue(HandleExpr(fs->GetEpilogue())); }
        HandleStmt(fs->GetBody());

        m_Declarations.pop_back();
    }

    void TypeChecker::HandleIfStmt(Stmt* stmt) {
        IfStmt* ifs = GetNode<IfStmt>(stmt);
        HandleExpr(ifs->GetCondition());
        HandleStmt(ifs->GetBody());
    }

    void TypeChecker::HandleReturnStmt(Stmt* stmt) {
        ReturnStmt* ret = GetNode<ReturnStmt>(stmt);

        if (m_ActiveReturnType == nullptr) {
            ARIA_ASSERT(false, "todo: error msg");
        }

        ret->SetValue(HandleInitializer(ret->GetValue(), m_ActiveReturnType));
    }

    void TypeChecker::HandleStmt(Stmt* stmt) {
        if (GetNode<CompoundStmt>(stmt)) {
            m_Declarations.emplace_back();
            HandleCompoundStmt(stmt);
            m_Declarations.pop_back();
            return;
        } else if (GetNode<WhileStmt>(stmt)) {
            HandleWhileStmt(stmt);
            return;
        } else if (GetNode<DoWhileStmt>(stmt)) {
            HandleDoWhileStmt(stmt);
            return;
        } else if (GetNode<ForStmt>(stmt)) {
            HandleForStmt(stmt);
            return;
        } else if (GetNode<IfStmt>(stmt)) {
            HandleIfStmt(stmt);
            return;
        } else if (GetNode<ReturnStmt>(stmt)) {
            HandleReturnStmt(stmt);
            return;
        } else if (Expr* expr = GetNode<Expr>(stmt)) {
            stmt = HandleExpr(expr);
            GetNode<Expr>(stmt)->IsStmtExpr = true;
            return;
        } else if (Decl* decl = GetNode<Decl>(stmt)) {
            HandleDecl(decl);
            return;
        }

        ARIA_UNREACHABLE();
    }

    Expr* TypeChecker::HandleInitializer(Expr* initializer, TypeInfo* type) {
        // If we are initializing a reference, the initializer must be of the same type and an lvalue
        if (type->IsReference()) {
            ARIA_ASSERT(initializer != nullptr, "initial value of a reference must be an lvalue");
            TypeInfo* valType = HandleExpr(initializer)->GetResolvedType();

            if (!TypeIsEqual(type, valType) || initializer->GetValueKind() != ExprValueKind::LValue) {
                m_Context->ReportCompilerError(initializer->Loc, initializer->Range, "initial value of reference must be an lvalue");
            }

            return initializer;
        } else if (initializer) {
            TypeInfo* valType = HandleExpr(initializer)->GetResolvedType();

            ConversionCost cost = GetConversionCost(type, valType, initializer->GetValueKind());
            if (cost.CastNeeded) {
                if (cost.ImplicitCastPossible) {
                    return InsertImplicitCast(type, valType, initializer, cost.CaKind);
                } else {
                    m_Context->ReportCompilerError(initializer->Loc, initializer->Range, fmt::format("cannot implicitly convert from '{}' to '{}'", TypeInfoToString(valType), TypeInfoToString(type)));
                }
            }
        }

       return initializer;
    }

    TypeInfo* TypeChecker::GetTypeInfoFromString(StringView str) {
        size_t bracket = str.Find('[');
        size_t amp = str.Find('&');

        std::string isolatedType = fmt::format("{}", str);
        bool array = false;
        bool reference = false;

        if (amp != StringView::npos) {
            isolatedType = isolatedType.substr(0, amp);
            reference = true;
        }

        if (bracket != StringView::npos) {
            isolatedType = isolatedType.substr(0, bracket);
            array = true;
        }
        
        TypeInfo* type = nullptr;

        if (isolatedType == "void")   { type = TypeInfo::Create(m_Context, PrimitiveType::Void, reference);   }
        if (isolatedType == "bool")   { type = TypeInfo::Create(m_Context, PrimitiveType::Bool, reference);   }
        if (isolatedType == "char")   { type = TypeInfo::Create(m_Context, PrimitiveType::Char, reference);   }
        if (isolatedType == "uchar")  { type = TypeInfo::Create(m_Context, PrimitiveType::UChar, reference);  }
        if (isolatedType == "short")  { type = TypeInfo::Create(m_Context, PrimitiveType::Short, reference);  }
        if (isolatedType == "ushort") { type = TypeInfo::Create(m_Context, PrimitiveType::UShort, reference); }
        if (isolatedType == "int")    { type = TypeInfo::Create(m_Context, PrimitiveType::Int, reference);    }
        if (isolatedType == "uint")   { type = TypeInfo::Create(m_Context, PrimitiveType::UInt, reference);   }
        if (isolatedType == "long")   { type = TypeInfo::Create(m_Context, PrimitiveType::Long, reference);   }
        if (isolatedType == "ulong")  { type = TypeInfo::Create(m_Context, PrimitiveType::ULong, reference);  }
        if (isolatedType == "float")  { type = TypeInfo::Create(m_Context, PrimitiveType::Float, reference);  }
        if (isolatedType == "double") { type = TypeInfo::Create(m_Context, PrimitiveType::Double, reference); }
        if (isolatedType == "string") { type = TypeInfo::Create(m_Context, PrimitiveType::String, reference); }

        #undef TYPE

        // Handle user defined type
        if (!type) {
            if (m_DeclaredTypes.contains(isolatedType)) {
                TypeInfo* sType = m_DeclaredTypes.at(isolatedType);
                type = TypeInfo::Create(m_Context, sType->Type, reference, sType->Data);
            } else {
                // ErrorUndeclaredIdentifier(StringView(isolatedType.c_str(), isolatedType.size()), 0, 0);
                ARIA_ASSERT(false, "todo");
            }
        }

        return type;
    }

    ConversionCost TypeChecker::GetConversionCost(TypeInfo* dst, TypeInfo* src, ExprValueKind srcType) {
        ConversionCost cost{};
        cost.CastNeeded = true;
        cost.ExplicitCastPossible = true;
        cost.ImplicitCastPossible = true;

        if (TypeIsEqual(src, dst)) {
            if (srcType == ExprValueKind::LValue) {
                cost.CastNeeded = true;
                cost.CaKind = CastKind::LValueToRValue;
                cost.CoKind = ConversionKind::LValueToRValue;
            } else {
                cost.CastNeeded = false;
            }

            return cost;
        }

        if (src->IsIntegral()) {
            if (dst->IsIntegral()) { // Int to int
                if (TypeGetSize(src) > TypeGetSize(dst)) {
                    cost.CoKind = ConversionKind::Narrowing;
                    cost.CaKind = CastKind::Integral;
                } else if (TypeGetSize(src) < TypeGetSize(dst)) {
                    cost.CoKind = ConversionKind::Promotion;
                    cost.CaKind = CastKind::Integral;
                } else {
                    if (src->IsSigned() == dst->IsSigned()) {
                        cost.CoKind = ConversionKind::None;
                        cost.CastNeeded = false;
                    } else {
                        cost.CoKind = ConversionKind::SignChange;
                        cost.CaKind = CastKind::Integral;
                        cost.CastNeeded = true;
                    }
                }
            } else if (dst->IsFloatingPoint()) { // Int to float
                cost.CoKind = ConversionKind::Promotion;
                cost.CaKind = CastKind::IntegralToFloating;
            } else {
                cost.ExplicitCastPossible = false;
            }
        }

        if (src->IsFloatingPoint()) {
            if (dst->IsFloatingPoint()) { // Float to float
                if (TypeGetSize(src) > TypeGetSize(dst)) {
                    cost.CoKind = ConversionKind::Narrowing;
                    cost.CaKind = CastKind::Floating;
                } else if (TypeGetSize(src) < TypeGetSize(dst)) {
                    cost.CoKind = ConversionKind::Promotion;
                    cost.CaKind = CastKind::Floating;
                } else {
                    cost.CoKind = ConversionKind::None;
                    cost.CastNeeded = false;
                }
            } else if (dst->IsIntegral()) { // Float to int
                cost.ImplicitCastPossible = false;
                cost.CoKind = ConversionKind::Narrowing;
                cost.CaKind = CastKind::FloatingToIntegral;
            } else {
                cost.ExplicitCastPossible = false;
            }
        }

        return cost;
    }

    Expr* TypeChecker::InsertImplicitCast(TypeInfo* dstType, TypeInfo* srcType, Expr* srcExpr, CastKind castKind) {
        ImplicitCastExpr* newExpr = m_Context->Allocate<ImplicitCastExpr>(m_Context, srcExpr->Loc, srcExpr->Range, srcExpr, castKind);
        newExpr->SetResolvedType(dstType);
        newExpr->SetValueKind(ExprValueKind::RValue);
        return newExpr;
    }

    bool TypeChecker::TypeIsEqual(TypeInfo* lhs, TypeInfo* rhs) {
        if (lhs->IsTrivial() && rhs->IsTrivial()) {
            return lhs->Type == rhs->Type;
        }

        if (lhs->IsStructure() && rhs->IsStructure()) {
            StructDeclaration& sLhs = std::get<StructDeclaration>(lhs->Data);
            StructDeclaration& sRhs = std::get<StructDeclaration>(rhs->Data);

            return sLhs.Identifier == sRhs.Identifier;
        }

        return false;
    }

    size_t TypeChecker::TypeGetSize(TypeInfo* t) {
        switch (t->Type) {
            case PrimitiveType::Void:   return 0;

            case PrimitiveType::Bool:   return 1;

            case PrimitiveType::Char:   return 1;
            case PrimitiveType::UChar:  return 1;
            case PrimitiveType::Short:  return 2;
            case PrimitiveType::UShort: return 2;
            case PrimitiveType::Int:    return 4;
            case PrimitiveType::UInt:   return 4;
            case PrimitiveType::Long:   return 8;
            case PrimitiveType::ULong:  return 8;

            case PrimitiveType::Float:  return 4;
            case PrimitiveType::Double: return 8;

            default: ARIA_ASSERT(false, "TypeChecker::TypeGetSize() only supports trivial (non structure) types");
        }
    }

} // namespace Aria::Internal
