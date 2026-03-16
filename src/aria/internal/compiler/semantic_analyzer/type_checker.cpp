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

    TypeInfo* TypeChecker::HandleBooleanConstantExpr(Expr* expr) {
        return expr->GetResolvedType();
    }

    TypeInfo* TypeChecker::HandleCharacterConstantExpr(Expr* expr) {
        return expr->GetResolvedType();
    }

    TypeInfo* TypeChecker::HandleIntegerConstantExpr(Expr* expr) {
        return expr->GetResolvedType();
    }

    TypeInfo* TypeChecker::HandleFloatingConstantExpr(Expr* expr) {
        return expr->GetResolvedType();
    }

    TypeInfo* TypeChecker::HandleStringConstantExpr(Expr* expr) {
        return expr->GetResolvedType();
    }

    TypeInfo* TypeChecker::HandleDeclRefExpr(Expr* expr) {
        DeclRefExpr* ref = GetNode<DeclRefExpr>(expr);

        std::string ident = ref->GetIdentifier();
        
        for (size_t i = m_Declarations.size(); i > 0; i--) {
            auto& it = m_Declarations.at(i - 1);

            if (it.contains(ident)) {
                ref->SetResolvedType(it.at(ident).ResolvedType);
                ref->SetType(it.at(ident).DeclType);
                return ref->GetResolvedType();
            }
        }

        ARIA_ASSERT(false, "todo: add error for TypeChecker::HandleVarRefExpr()");
        // m_Context->ReportCompilerError(expr->Loc.Line, expr->Loc.Column, 
        //                                expr->Range.Start.Line, expr->Range.Start.Column,
        //                                expr->Range.End.Line, expr->Range.End.Column,
        //                                fmt::format("Undeclared identifier \"{}\"", ref->Identifier));
        return nullptr;
    }

    TypeInfo* TypeChecker::HandleMemberExpr(Expr* expr) {
        MemberExpr* mem = GetNode<MemberExpr>(expr);

        TypeInfo* parentType = HandleExpr(mem->GetParent());
        TypeInfo* memberType = nullptr;
        StructDeclaration& sd = std::get<StructDeclaration>(parentType->Data);

        StructDecl* s = GetNode<StructDecl>(sd.SourceDecl);

        for (Decl* field : s->GetFields()) {
            if (FieldDecl* fd = GetNode<FieldDecl>(field)) {
                memberType = fd->GetResolvedType();
            } else if (MethodDecl* md = GetNode<MethodDecl>(field)) {
                memberType = md->GetResolvedType();
            }
        }

        if (!memberType) {
            ARIA_ASSERT(false, "todo: add error msg");
        }

        mem->SetParentType(parentType);
        mem->SetResolvedType(memberType);

        return memberType;
    }

    TypeInfo* TypeChecker::HandleCallExpr(Expr* expr) {
        CallExpr* call = GetNode<CallExpr>(expr);

        TypeInfo* calleeType = HandleExpr(call->GetCallee());
        FunctionDeclaration& fnDecl = std::get<FunctionDeclaration>(calleeType->Data);

        if (fnDecl.ParamTypes.Size != call->GetArguments().Size) {
            ARIA_ASSERT(false, "todo: error msg");
        }

        for (size_t i = 0; i < fnDecl.ParamTypes.Size; i++) {
            TypeInfo* paramType = fnDecl.ParamTypes.Items[i];
            TypeInfo* argType = HandleExpr(call->GetArguments().Items[i]);

            ConversionCost cost = GetConversionCost(paramType, argType, call->GetArguments().Items[i]->GetValueType());
            if (cost.CastNeeded) {
                if (cost.ImplicitCastPossible) {
                    call->SetArgument(i, InsertImplicitCast(paramType, argType, call->GetArguments().Items[i], cost.CaType));
                } else {
                    ARIA_ASSERT(false, "todo: error msg");
                }
            }
        }

        call->SetResolvedType(fnDecl.ReturnType);
        return fnDecl.ReturnType;
    }

    TypeInfo* TypeChecker::HandleMethodCallExpr(Expr* expr) {
        MethodCallExpr* call = GetNode<MethodCallExpr>(expr);
        TypeInfo* calleeType = HandleMemberExpr(call->GetCallee());

        FunctionDeclaration& fnDecl = std::get<FunctionDeclaration>(calleeType->Data);

        if (fnDecl.ParamTypes.Size != call->GetArguments().Size) {
            ARIA_ASSERT(false, "todo: error msg");
        }

        for (size_t i = 0; i < fnDecl.ParamTypes.Size; i++) {
            TypeInfo* paramType = fnDecl.ParamTypes.Items[i];
            TypeInfo* argType = HandleExpr(call->GetArguments().Items[i]);

            ConversionCost cost = GetConversionCost(paramType, argType, call->GetArguments().Items[i]->GetValueType());
            if (cost.CastNeeded) {
                if (cost.ImplicitCastPossible) {
                    call->SetArgument(i, InsertImplicitCast(paramType, argType, call->GetArguments().Items[i], cost.CaType));
                } else {
                    ARIA_ASSERT(false, "todo: error msg");
                }
            }
        }

        call->SetResolvedType(fnDecl.ReturnType);
        return fnDecl.ReturnType;
    }

    TypeInfo* TypeChecker::HandleParenExpr(Expr* expr) {
        ParenExpr* paren = GetNode<ParenExpr>(expr);
        HandleExpr(paren->GetChildExpr());
        return paren->GetResolvedType();
    }

    TypeInfo* TypeChecker::HandleCastExpr(Expr* expr) {
        CastExpr* cast = GetNode<CastExpr>(expr);

        TypeInfo* srcType = HandleExpr(cast->GetChildExpr());
        TypeInfo* dstType = GetTypeInfoFromString(cast->GetParsedType());

        ConversionCost cost = GetConversionCost(dstType, srcType, cast->GetChildExpr()->GetValueType());

        if (cost.CastNeeded) {
            if (cost.ExplicitCastPossible) {
                cast->SetResolvedType(dstType);
                cast->SetCastType(cost.CaType);
            } else {
                ARIA_ASSERT(false, "todo: add error message");
            }
        }

        return dstType;
    }

    TypeInfo* TypeChecker::HandleUnaryOperatorExpr(Expr* expr) {
        UnaryOperatorExpr* unop = GetNode<UnaryOperatorExpr>(expr);

        TypeInfo* type = HandleExpr(unop->GetChildExpr());

        ConversionCost cost = GetConversionCost(type, type, unop->GetChildExpr()->GetValueType());
        if (cost.CastNeeded) {
            if (cost.ImplicitCastPossible) {
                unop->SetChildExpr(InsertImplicitCast(type, type, unop->GetChildExpr(), cost.CaType));
            } else {
                ARIA_ASSERT(false, "todo: TypeChecker::HandleVarDecl() error");
            }
        }

        switch (unop->GetUnaryOperator()) {
            case UnaryOperatorType::Negate: {
                ARIA_ASSERT(type->IsNumeric(), "todo: add error message");
                unop->SetResolvedType(type);
                return type;
            }
        }

        ARIA_UNREACHABLE();
    }

    TypeInfo* TypeChecker::HandleBinaryOperatorExpr(Expr* expr) {
        BinaryOperatorExpr* binop = GetNode<BinaryOperatorExpr>(expr);

        Expr* LHS = binop->GetLHS();
        Expr* RHS = binop->GetRHS();

        TypeInfo* LHSType = HandleExpr(binop->GetLHS());
        TypeInfo* RHSType = HandleExpr(binop->GetRHS());

        switch (binop->GetBinaryOperator()) {
            case BinaryOperatorType::Add:
            case BinaryOperatorType::Sub:
            case BinaryOperatorType::Mul:
            case BinaryOperatorType::Div:
            case BinaryOperatorType::Mod:
            case BinaryOperatorType::Less:
            case BinaryOperatorType::LessOrEq:
            case BinaryOperatorType::Greater:
            case BinaryOperatorType::GreaterOrEq:
            case BinaryOperatorType::IsEq: 
            case BinaryOperatorType::IsNotEq: {
                // See which conversion would be better
                ConversionCost costLHS = GetConversionCost(LHSType, RHSType, LHS->GetValueType());
                ConversionCost costRHS = GetConversionCost(RHSType, LHSType, RHS->GetValueType());

                if (costLHS.CastNeeded || costRHS.CastNeeded) {
                    bool lhsCastNeeded = costLHS.CastNeeded;
                    bool rhsCastNeeded = costRHS.CastNeeded;

                    if (costLHS.CoType == ConversionType::LValueToRValue) {
                        binop->SetLHS(InsertImplicitCast(LHSType, RHSType, LHS, costLHS.CaType));
                        RHSType = LHSType;
                        lhsCastNeeded = false;
                    }
                    
                    if (costRHS.CoType == ConversionType::LValueToRValue) {
                        binop->SetRHS(InsertImplicitCast(RHSType, LHSType, RHS, costRHS.CaType));
                        LHSType = RHSType;
                        rhsCastNeeded = false;
                    }

                    if (lhsCastNeeded || rhsCastNeeded) {
                        if (costLHS.CoType == ConversionType::Promotion) {
                            binop->SetLHS(InsertImplicitCast(LHSType, RHSType, LHS, costLHS.CaType));
                            RHSType = LHSType;
                        } else if (costRHS.CoType == ConversionType::Promotion) {
                            binop->SetRHS(InsertImplicitCast(RHSType, LHSType, RHS, costLHS.CaType));
                            LHSType = RHSType;
                        } else {
                            ARIA_ASSERT(false, "todo: add error for TypeChecker::HandleBinaryOperatorExpr()");
                            // m_Context->ReportCompilerError(expr->Loc.Line, expr->Loc.Column, 
                            //                                expr->Range.Start.Line, expr->Range.Start.Column,
                            //                                expr->Range.End.Line, expr->Range.End.Column,
                            //                                fmt::format("Mismatched types '{}' and '{}', no viable implicit cast", TypeInfoToString(LHSType), TypeInfoToString(RHSType)));
                        }
                    }
                }

                if (binop->GetBinaryOperator() == BinaryOperatorType::Less ||
                    binop->GetBinaryOperator() == BinaryOperatorType::LessOrEq ||
                    binop->GetBinaryOperator() == BinaryOperatorType::Greater ||
                    binop->GetBinaryOperator() == BinaryOperatorType::GreaterOrEq ||
                    binop->GetBinaryOperator() == BinaryOperatorType::IsEq ||
                    binop->GetBinaryOperator() == BinaryOperatorType::IsNotEq) 
                {
                    TypeInfo* boolType = TypeInfo::Create(m_Context, PrimitiveType::Bool);
                    binop->SetResolvedType(boolType);
                    return boolType;
                }

                binop->SetResolvedType(LHSType);
                return LHSType;
            }

            case BinaryOperatorType::AddInPlace:
            case BinaryOperatorType::SubInPlace:
            case BinaryOperatorType::MulInPlace:
            case BinaryOperatorType::DivInPlace:
            case BinaryOperatorType::ModInPlace:
            case BinaryOperatorType::Eq: {
                if (binop->GetLHS()->GetValueType() != ExprValueType::LValue) {
                    // m_Context->ReportCompilerError(binop->LHS->Loc.Line, binop->LHS->Loc.Column, 
                    //                                binop->LHS->Range.Start.Line, binop->LHS->Range.Start.Column,
                    //                                binop->LHS->Range.End.Line, binop->LHS->Range.End.Column,
                    //                                "Expression must be a modifiable lvalue");
                    ARIA_ASSERT(false, "todo: add error for TypeChecker::HandleBinaryOperatorExpr()");
                }

                ConversionCost cost = GetConversionCost(LHSType, RHSType, binop->GetRHS()->GetValueType());

                if (cost.CastNeeded) {
                    if (cost.ImplicitCastPossible) {
                        binop->SetRHS(InsertImplicitCast(LHSType, RHSType, RHS, cost.CaType));
                        RHSType = LHSType;
                    } else {
                        // m_Context->ReportCompilerError(binop->RHS->Loc.Line, binop->RHS->Loc.Column, 
                        //                                binop->RHS->Range.Start.Line, binop->RHS->Range.Start.Column,
                        //                                binop->RHS->Range.End.Line, binop->RHS->Range.End.Column,
                        //                                fmt::format("Cannot implicitly cast from {} to {}", TypeInfoToString(RHSType), TypeInfoToString(LHSType)));
                        ARIA_ASSERT(false, "todo: add error for TypeChecker::HandleBinaryOperatorExpr()");
                    }
                }

                binop->SetResolvedType(LHSType);
                return LHSType;
            }

            case BinaryOperatorType::BitAnd:
            case BinaryOperatorType::BitOr: {
                TypeInfo* boolType = TypeInfo::Create(m_Context, PrimitiveType::Bool);

                ConversionCost costLHS = GetConversionCost(boolType, RHSType, LHS->GetValueType());
                ConversionCost costRHS = GetConversionCost(boolType, LHSType, RHS->GetValueType());

                if (costLHS.CastNeeded) {
                    if (costLHS.ImplicitCastPossible) {
                        binop->SetLHS(InsertImplicitCast(boolType, LHSType, LHS, costLHS.CaType));
                    } else {
                        ARIA_ASSERT(false, "todo: add error for TypeChecker::HandleBinaryOperatorExpr()");
                    }
                }

                if (costRHS.CastNeeded) {
                    if (costRHS.ImplicitCastPossible) {
                        binop->SetRHS(InsertImplicitCast(boolType, RHSType, RHS, costRHS.CaType));
                    } else {
                        ARIA_ASSERT(false, "todo: add error for TypeChecker::HandleBinaryOperatorExpr()");
                    }
                }

                binop->SetResolvedType(boolType);
                return boolType;
            }
        }

        ARIA_UNREACHABLE();
    }

    TypeInfo* TypeChecker::HandleExpr(Expr* expr) {
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

        if (varDecl->GetDefaultValue()) {
            TypeInfo* valType = HandleExpr(varDecl->GetDefaultValue());

            ConversionCost cost = GetConversionCost(resolvedType, valType, varDecl->GetDefaultValue()->GetValueType());
            if (cost.CastNeeded) {
                if (cost.ImplicitCastPossible) {
                    varDecl->SetDefaultValue(InsertImplicitCast(resolvedType, valType, varDecl->GetDefaultValue(), cost.CaType));
                } else {
                    ARIA_ASSERT(false, "todo: TypeChecker::HandleVarDecl() error");
                }
            }
        }

        DeclRefType type = DeclRefType::LocalVar;
        if (m_Declarations.size() == 1) {
            type = DeclRefType::GlobalVar;
        }
        
        std::string ident = varDecl->GetIdentifier();
        m_Declarations.back()[ident] = { varDecl->GetResolvedType(), decl, type };
    }

    void TypeChecker::HandleParamDecl(Decl* decl) {
        ParamDecl* paramDecl = GetNode<ParamDecl>(decl);

        TypeInfo* resolvedType = GetTypeInfoFromString(paramDecl->GetParsedType());
        paramDecl->SetResolvedType(resolvedType);

        std::string ident = paramDecl->GetIdentifier();
        m_Declarations.back()[ident] = { paramDecl->GetResolvedType(), decl, DeclRefType::ParamVar };
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
        
        TypeInfo* resolvedType = TypeInfo::Create(m_Context, PrimitiveType::Function, fd);
        fnDecl->SetResolvedType(resolvedType);

        std::string ident = fnDecl->GetIdentifier();
        m_Declarations.front()[ident] = { fnDecl->GetResolvedType(), decl, DeclRefType::Function };

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

        for (Decl* field : s->GetFields()) {
            if (FieldDecl* fd = GetNode<FieldDecl>(field)) {
                fd->SetResolvedType(GetTypeInfoFromString(fd->GetParsedType()));
            } else if (MethodDecl* md = GetNode<MethodDecl>(field)) {
                TypeInfo* returnType = GetTypeInfoFromString(md->GetParsedType());
                TinyVector<TypeInfo*> paramTypes;
                m_ActiveReturnType = returnType;

                m_Declarations.emplace_back();

                for (ParamDecl* p : md->GetParameters()) {
                    HandleParamDecl(p);
                    TypeInfo* pType = p->GetResolvedType();
                    paramTypes.Append(m_Context, pType);
                }

                md->SetResolvedType(TypeInfo::Create(m_Context, PrimitiveType::Function, FunctionDeclaration(returnType, paramTypes)));
            }
        }

        m_DeclaredTypes[s->GetIdentifier()] = TypeInfo::Create(m_Context, PrimitiveType::Structure, d);
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

        TypeInfo* type = HandleExpr(wh->GetCondition());
        TypeInfo* boolType = TypeInfo::Create(m_Context, PrimitiveType::Bool);

        ConversionCost cost = GetConversionCost(boolType, type, wh->GetCondition()->GetValueType());
        if (cost.CastNeeded) {
            if (cost.ImplicitCastPossible) {
                wh->SetCondition(InsertImplicitCast(boolType, type, wh->GetCondition(), cost.CaType));
            } else {
                ARIA_ASSERT(false, "todo");
            }
        }

        HandleStmt(wh->GetBody());
    }

    void TypeChecker::HandleDoWhileStmt(Stmt* stmt) {
        DoWhileStmt* wh = GetNode<DoWhileStmt>(stmt);

        TypeInfo* type = HandleExpr(wh->GetCondition());
        TypeInfo* boolType = TypeInfo::Create(m_Context, PrimitiveType::Bool);

        ConversionCost cost = GetConversionCost(boolType, type, wh->GetCondition()->GetValueType());
        if (cost.CastNeeded) {
            if (cost.ImplicitCastPossible) {
                wh->SetCondition(InsertImplicitCast(boolType, type, wh->GetCondition(), cost.CaType));
            } else {
                ARIA_ASSERT(false, "todo");
            }
        }

        HandleStmt(wh->GetBody());
    }

    void TypeChecker::HandleForStmt(Stmt* stmt) { ARIA_ASSERT(false, "todo"); }

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

        Expr* value = ret->GetValue();
        TypeInfo* valType = HandleExpr(value);

        ConversionCost cost = GetConversionCost(m_ActiveReturnType, valType, value->GetValueType());
        if (cost.CastNeeded) {
            if (cost.ImplicitCastPossible) {
                ret->SetValue(InsertImplicitCast(m_ActiveReturnType, valType, value, cost.CaType));
            } else {
                ARIA_ASSERT(false, "todo: error");
            }
        }
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
            HandleExpr(expr);
            return;
        } else if (Decl* decl = GetNode<Decl>(stmt)) {
            HandleDecl(decl);
            return;
        }

        ARIA_UNREACHABLE();
    }

    TypeInfo* TypeChecker::GetTypeInfoFromString(StringView str) {
        size_t bracket = str.Find('[');

        std::string isolatedType;
        bool array = false;

        if (bracket != StringView::npos) {
            isolatedType = fmt::format("{}", str.SubStr(0, bracket));
            array = true;
        } else {
            isolatedType = fmt::format("{}", str);
        }

        TypeInfo* type = nullptr;

        if (isolatedType == "void") { type = TypeInfo::Create(m_Context, PrimitiveType::Void); }
        if (isolatedType == "bool") { type = TypeInfo::Create(m_Context, PrimitiveType::Bool, true); }
        if (isolatedType == "char") { type = TypeInfo::Create(m_Context, PrimitiveType::Char, true); }
        if (isolatedType == "uchar") { type = TypeInfo::Create(m_Context, PrimitiveType::Char, false); }
        if (isolatedType == "short") { type = TypeInfo::Create(m_Context, PrimitiveType::Short, true); }
        if (isolatedType == "ushort") { type = TypeInfo::Create(m_Context, PrimitiveType::Short, false); }
        if (isolatedType == "int") { type = TypeInfo::Create(m_Context, PrimitiveType::Int, true); }
        if (isolatedType == "uint") { type = TypeInfo::Create(m_Context, PrimitiveType::Int, false); }
        if (isolatedType == "long") { type = TypeInfo::Create(m_Context, PrimitiveType::Long, true); }
        if (isolatedType == "ulong") { type = TypeInfo::Create(m_Context, PrimitiveType::Long, false); }
        if (isolatedType == "float") { type = TypeInfo::Create(m_Context, PrimitiveType::Float); }
        if (isolatedType == "double") { type = TypeInfo::Create(m_Context, PrimitiveType::Double); }
        if (isolatedType == "string") { type = TypeInfo::Create(m_Context, PrimitiveType::String); }

        #undef TYPE

        // Handle user defined type
        if (!type) {
            if (m_DeclaredTypes.contains(isolatedType)) {
                type = m_DeclaredTypes.at(isolatedType);
            } else {
                // ErrorUndeclaredIdentifier(StringView(isolatedType.c_str(), isolatedType.size()), 0, 0);
                ARIA_ASSERT(false, "todo");
            }
        }

        return type;
    }

    ConversionCost TypeChecker::GetConversionCost(TypeInfo* dst, TypeInfo* src, ExprValueType srcType) {
        ConversionCost cost{};
        cost.CastNeeded = true;
        cost.ExplicitCastPossible = true;
        cost.ImplicitCastPossible = true;

        if (TypeIsEqual(src, dst)) {
            if (srcType == ExprValueType::LValue) {
                cost.CastNeeded = true;
                cost.CaType = CastType::LValueToRValue;
                cost.CoType = ConversionType::LValueToRValue;
            } else {
                cost.CastNeeded = false;
            }

            return cost;
        }

        if (src->IsIntegral()) {
            if (dst->IsIntegral()) { // Int to int
                if (TypeGetSize(src) > TypeGetSize(dst)) {
                    cost.CoType = ConversionType::Narrowing;
                    cost.CaType = CastType::Integral;
                } else if (TypeGetSize(src) < TypeGetSize(dst)) {
                    cost.CoType = ConversionType::Promotion;
                    cost.CaType = CastType::Integral;
                } else {
                    if (src->IsSigned() == dst->IsSigned()) {
                        cost.CoType = ConversionType::None;
                        cost.CastNeeded = false;
                    } else {
                        cost.CoType = ConversionType::SignChange;
                        cost.CastNeeded = true;
                    }
                }
            } else if (dst->IsFloatingPoint()) { // Int to float
                cost.CoType = ConversionType::Promotion;
                cost.CaType = CastType::IntegralToFloating;
            } else {
                cost.ExplicitCastPossible = false;
            }
        }

        if (src->IsFloatingPoint()) {
            if (dst->IsFloatingPoint()) { // Float to float
                if (TypeGetSize(src) > TypeGetSize(dst)) {
                    cost.CoType = ConversionType::Narrowing;
                    cost.CaType = CastType::Floating;
                } else if (TypeGetSize(src) < TypeGetSize(dst)) {
                    cost.CoType = ConversionType::Promotion;
                    cost.CaType = CastType::Floating;
                } else {
                    cost.CoType = ConversionType::None;
                    cost.CastNeeded = false;
                }
            } else if (dst->IsIntegral()) { // Float to int
                cost.ImplicitCastPossible = false;
                cost.CoType = ConversionType::Narrowing;
                cost.CaType = CastType::FloatingToIntegral;
            } else {
                cost.ExplicitCastPossible = false;
            }
        }

        return cost;
    }

    Expr* TypeChecker::InsertImplicitCast(TypeInfo* dstType, TypeInfo* srcType, Expr* srcExpr, CastType castType) {
        return m_Context->Allocate<ImplicitCastExpr>(m_Context, srcExpr, castType, dstType);
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
