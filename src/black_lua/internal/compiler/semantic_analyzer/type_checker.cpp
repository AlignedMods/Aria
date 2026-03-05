#include "black_lua/internal/compiler/semantic_analyzer/type_checker.hpp"
#include "black_lua/internal/compiler/ast/ast.hpp"

namespace BlackLua::Internal {

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

    TypeInfo* TypeChecker::HandleVarRefExpr(Expr* expr) {
        VarRefExpr* ref = GetNode<VarRefExpr>(expr);

        std::string ident = ref->GetIdentifier();
        
        for (auto it = m_Declarations.rbegin(); it != m_Declarations.rend(); it++) {
            if (it->contains(ident)) {
                ref->SetResolvedType(it->at(ident));
                return ref->GetResolvedType();
            }
        }

        BLUA_ASSERT(false, "todo: add error for TypeChecker::HandleVarRefExpr()");
        // m_Context->ReportCompilerError(expr->Loc.Line, expr->Loc.Column, 
        //                                expr->Range.Start.Line, expr->Range.Start.Column,
        //                                expr->Range.End.Line, expr->Range.End.Column,
        //                                fmt::format("Undeclared identifier \"{}\"", ref->Identifier));
        return nullptr;
    }

    TypeInfo* TypeChecker::HandleCallExpr(Expr* expr) {
        CallExpr* call = GetNode<CallExpr>(expr);
        BLUA_ASSERT(false, "todo: TypeChecker::HandleCallExpr()");
    }

    TypeInfo* TypeChecker::HandleParenExpr(Expr* expr) {
        ParenExpr* paren = GetNode<ParenExpr>(expr);
        HandleExpr(paren->GetChildExpr());
        return paren->GetResolvedType();
    }

    TypeInfo* TypeChecker::HandleCastExpr(Expr* expr) { BLUA_ASSERT(false, "todo: TypeChecker::HandleCastExpr()"); }
    TypeInfo* TypeChecker::HandleUnaryOperatorExpr(Expr* expr) { BLUA_ASSERT(false, "todo: TypeChecker::HandleUnaryOperatorExpr()"); }

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
            case BinaryOperatorType::GreaterOrEq: {
                // See which conversion would be better
                ConversionCost costLHS = GetConversionCost(LHSType, RHSType, LHS->IsLValue());
                ConversionCost costRHS = GetConversionCost(RHSType, LHSType, RHS->IsLValue());

                if (costLHS.CastNeeded || costRHS.CastNeeded) {
                    if (costLHS.ConversionType == ConversionType::LValueToRValue) {
                        binop->SetLHS(InsertImplicitCast(LHSType, RHSType, LHS, costLHS.CastType));
                        RHSType = LHSType;
                    }
                    
                    if (costRHS.ConversionType == ConversionType::LValueToRValue) {
                        binop->SetRHS(InsertImplicitCast(RHSType, LHSType, RHS, costLHS.CastType));
                        LHSType = RHSType;
                    }

                    if (costLHS.ConversionType == ConversionType::Promotion) {
                        binop->SetLHS(InsertImplicitCast(LHSType, RHSType, LHS, costLHS.CastType));
                        RHSType = LHSType;
                    } else if (costRHS.ConversionType == ConversionType::Promotion) {
                        binop->SetRHS(InsertImplicitCast(RHSType, LHSType, RHS, costLHS.CastType));
                        LHSType = RHSType;
                    } else {
                        BLUA_ASSERT(false, "todo: add error for TypeChecker::HandleBinaryOperatorExpr()");
                        // m_Context->ReportCompilerError(expr->Loc.Line, expr->Loc.Column, 
                        //                                expr->Range.Start.Line, expr->Range.Start.Column,
                        //                                expr->Range.End.Line, expr->Range.End.Column,
                        //                                fmt::format("Mismatched types '{}' and '{}', no viable implicit cast", TypeInfoToString(LHSType), TypeInfoToString(RHSType)));
                    }
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
                if (!binop->GetLHS()->IsLValue()) {
                    // m_Context->ReportCompilerError(binop->LHS->Loc.Line, binop->LHS->Loc.Column, 
                    //                                binop->LHS->Range.Start.Line, binop->LHS->Range.Start.Column,
                    //                                binop->LHS->Range.End.Line, binop->LHS->Range.End.Column,
                    //                                "Expression must be a modifiable lvalue");
                    BLUA_ASSERT(false, "todo: add error for TypeChecker::HandleBinaryOperatorExpr()");
                }

                ConversionCost cost = GetConversionCost(LHSType, RHSType, binop->GetRHS()->IsLValue());

                if (cost.CastNeeded) {
                    if (cost.ImplicitCastPossible) {
                        binop->SetRHS(InsertImplicitCast(LHSType, RHSType, RHS, cost.CastType));
                        RHSType = LHSType;
                    } else {
                        // m_Context->ReportCompilerError(binop->RHS->Loc.Line, binop->RHS->Loc.Column, 
                        //                                binop->RHS->Range.Start.Line, binop->RHS->Range.Start.Column,
                        //                                binop->RHS->Range.End.Line, binop->RHS->Range.End.Column,
                        //                                fmt::format("Cannot implicitly cast from {} to {}", TypeInfoToString(RHSType), TypeInfoToString(LHSType)));
                        BLUA_ASSERT(false, "todo: add error for TypeChecker::HandleBinaryOperatorExpr()");
                    }
                }

                binop->SetResolvedType(LHSType);
                return LHSType;
            }
        }

        BLUA_UNREACHABLE();
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
        } else if (GetNode<VarRefExpr>(expr)) {
            return HandleVarRefExpr(expr);
        } else if (GetNode<CallExpr>(expr)) {
            return HandleCallExpr(expr);
        } else if (GetNode<CastExpr>(expr)) {
            return HandleCastExpr(expr);
        } else if (GetNode<UnaryOperatorExpr>(expr)) {
            return HandleUnaryOperatorExpr(expr);
        } else if (GetNode<BinaryOperatorExpr>(expr)) {
            return HandleBinaryOperatorExpr(expr);
        }

        BLUA_UNREACHABLE();
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

            ConversionCost cost = GetConversionCost(resolvedType, valType, varDecl->GetDefaultValue()->IsLValue());
            if (cost.CastNeeded) {
                if (cost.ImplicitCastPossible) {
                    varDecl->SetDefaultValue(InsertImplicitCast(resolvedType, valType, varDecl->GetDefaultValue(), cost.CastType));
                } else {
                    BLUA_ASSERT(false, "todo: TypeChecker::HandleVarDecl() error");
                }
            }
        }
        
        std::string ident = varDecl->GetIdentifier();
        m_Declarations.back()[ident] = varDecl->GetResolvedType();
    }

    void TypeChecker::HandleParamDecl(Decl* decl) {
        ParamDecl* paramDecl = GetNode<ParamDecl>(decl);

        TypeInfo* resolvedType = GetTypeInfoFromString(paramDecl->GetParsedType());
        paramDecl->SetResolvedType(resolvedType);

        std::string ident = paramDecl->GetIdentifier();
        m_Declarations.back()[ident] = paramDecl->GetResolvedType();
    }

    void TypeChecker::HandleFunctionDecl(Decl* decl) {
        FunctionDecl* fnDecl = GetNode<FunctionDecl>(decl);

        TypeInfo* resolvedType = GetTypeInfoFromString(fnDecl->GetParsedType());
        fnDecl->SetResolvedType(resolvedType);

        std::string ident = fnDecl->GetIdentifier();
        m_Declarations.front()[ident] = fnDecl->GetResolvedType();
        
        m_Declarations.emplace_back();

        for (ParamDecl* p : fnDecl->GetParameters()) {
            HandleParamDecl(p);
        }

        if (fnDecl->GetBody()) {
            HandleCompoundStmt(fnDecl->GetBody());
        }

        m_Declarations.pop_back();
    }

    void TypeChecker::HandleDecl(Decl* decl) {
        if (GetNode<TranslationUnitDecl>(decl)) {
            HandleTranslationUnitDecl(decl);
            return;
        } else if (GetNode<VarDecl>(decl)) {
            HandleVarDecl(decl);
            return;
        } else if (GetNode<ParamDecl>(decl)) {
            HandleParamDecl(decl);
            return;
        } else if (GetNode<FunctionDecl>(decl)) {
            HandleFunctionDecl(decl);
            return;
        }

        BLUA_UNREACHABLE();
    }

    void TypeChecker::HandleCompoundStmt(Stmt* stmt) {
        CompoundStmt* compound = GetNode<CompoundStmt>(stmt);

        for (Stmt* s : compound->GetStmts()) {
            HandleStmt(s);
        }
    }

    void TypeChecker::HandleWhileStmt(Stmt* stmt) { BLUA_ASSERT(false, "todo"); }
    void TypeChecker::HandleDoWhileStmt(Stmt* stmt) { BLUA_ASSERT(false, "todo"); }
    void TypeChecker::HandleForStmt(Stmt* stmt) { BLUA_ASSERT(false, "todo"); }
    void TypeChecker::HandleIfStmt(Stmt* stmt) { BLUA_ASSERT(false, "todo"); }
    void TypeChecker::HandleReturnStmt(Stmt* stmt) { BLUA_ASSERT(false, "todo"); }

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

        BLUA_UNREACHABLE();
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
        if (type->Type == PrimitiveType::Invalid) {
            BLUA_ASSERT(false, "todo");
            // if (m_DeclaredStructs.contains(isolatedType)) {
            //     type->Type = PrimitiveType::Structure;
            //     type->Data = m_DeclaredStructs.at(isolatedType);
            // } else {
            //     ErrorUndeclaredIdentifier(StringView(isolatedType.c_str(), isolatedType.size()), 0, 0);
            // }
        }

        if (array) {
            BLUA_ASSERT(false, "todo");
            // ArrayDeclaration decl;
            // decl.Type = type;
            // 
            // VariableType* arrType = CreateVarType(m_Context, PrimitiveType::Array, decl);
            // type = arrType;
        }

        return type;
    }

    ConversionCost TypeChecker::GetConversionCost(TypeInfo* dst, TypeInfo* src, bool srcLValue) {
        ConversionCost cost{};
        cost.CastNeeded = true;
        cost.ExplicitCastPossible = true;
        cost.ImplicitCastPossible = true;

        if (TypeInfo::IsEqual(src, dst)) {
            if (srcLValue) {
                cost.CastNeeded = true;
                cost.CastType = CastType::LValueToRValue;
            } else {
                cost.CastNeeded = false;
            }

            return cost;
        }

        if (src->IsIntegral()) {
            if (dst->IsIntegral()) { // Int to int
                if (src->GetSize() > dst->GetSize()) {
                    cost.ConversionType = ConversionType::Narrowing;
                    cost.CastType = CastType::Integral;
                } else if (src->GetSize() < dst->GetSize()) {
                    cost.ConversionType = ConversionType::Promotion;
                    cost.CastType = CastType::Integral;
                } else {
                    if (src->IsSigned() == dst->IsSigned()) {
                        cost.ConversionType = ConversionType::None;
                        cost.CastNeeded = false;
                    } else {
                        cost.ConversionType = ConversionType::SignChange;
                        cost.CastNeeded = true;
                    }
                }
            } else if (dst->IsFloatingPoint()) { // Int to float
                cost.ConversionType = ConversionType::Promotion;
                cost.CastType = CastType::IntegralToFloating;
            } else {
                cost.ExplicitCastPossible = false;
            }
        }

        if (src->IsFloatingPoint()) {
            if (dst->IsFloatingPoint()) { // Float to float
                if (src->GetSize() > dst->GetSize()) {
                    cost.ConversionType = ConversionType::Narrowing;
                    cost.CastType = CastType::Floating;
                } else if (src->GetSize() < dst->GetSize()) {
                    cost.ConversionType = ConversionType::Promotion;
                    cost.CastType = CastType::Floating;
                } else {
                    cost.ConversionType = ConversionType::None;
                    cost.CastNeeded = false;
                }
            } else if (dst->IsIntegral()) { // Float to int
                cost.ImplicitCastPossible = false;
                cost.ConversionType = ConversionType::Narrowing;
                cost.CastType = CastType::FloatingToIntegral;
            } else {
                cost.ExplicitCastPossible = false;
            }
        }

        return cost;
    }

    Expr* TypeChecker::InsertImplicitCast(TypeInfo* dstType, TypeInfo* srcType, Expr* srcExpr, CastType castType) {
        return m_Context->Allocate<ImplicitCastExpr>(m_Context, srcExpr, castType, dstType);
    }

} // namespace BlackLua::Internal
