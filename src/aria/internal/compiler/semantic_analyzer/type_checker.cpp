#include "aria/internal/compiler/semantic_analyzer/type_checker.hpp"

namespace Aria::Internal {

    TypeChecker::TypeChecker(CompilationContext* ctx) {
        m_Context = ctx;

        m_RootASTNode = ctx->GetRootASTNode();
    }

    void TypeChecker::CheckImpl() {
        m_ErrorType = TypeInfo::Create(m_Context, PrimitiveType::Error, false);
        m_BuiltInStringDestructor = Decl::Create(m_Context, SourceLocation(), SourceRange(), DeclKind::BuiltinDestructor, BuiltinDestructorDecl(BuiltinKind::String));
        m_BuiltInStringCopyConstructor = Decl::Create(m_Context, SourceLocation(), SourceRange(), DeclKind::BuiltinCopyConstructor, BuiltinCopyConstructorDecl(BuiltinKind::String));

        HandleStmt(m_RootASTNode);
    }

    void TypeChecker::HandleBooleanConstantExpr(Expr* expr) {}
    void TypeChecker::HandleCharacterConstantExpr(Expr* expr) {}
    void TypeChecker::HandleIntegerConstantExpr(Expr* expr) {}
    void TypeChecker::HandleFloatingConstantExpr(Expr* expr) {}
    void TypeChecker::HandleStringConstantExpr(Expr* expr) {}

    void TypeChecker::HandleDeclRefExpr(Expr* expr) {
        DeclRefExpr& ref = expr->DeclRef;
        std::string ident = fmt::format("{}", ref.Identifier);

        if (m_ActiveStruct) {
            StructDecl s = std::get<StructDeclaration>(m_ActiveStruct->Data).SourceDecl->Struct;

            // Check for implicit self
            for (Decl* d : s.Fields) {
                if (d->Kind == DeclKind::Field) {
                    FieldDecl fd = d->Field;
                    if (fd.Identifier == ref.Identifier) {
                        Expr* self = Expr::Create(m_Context, expr->Loc, expr->Range, ExprKind::Self, ExprValueKind::LValue, m_ActiveStruct, SelfExpr());
                        Expr* member = Expr::Create(m_Context, expr->Loc, expr->Range, ExprKind::Member, ExprValueKind::LValue, fd.Type, MemberExpr(ref.Identifier, self));
                        ReplaceExpr(expr, member);
                        return;
                    }
                }
            }
        }
        
        for (size_t i = m_Scopes.size(); i > 0; i--) {
            auto& it = m_Scopes.at(i - 1);

            if (it.Declarations.contains(ident)) {
                ref.Kind = it.Declarations.at(ident).DeclKind;
                expr->Type = it.Declarations.at(ident).ResolvedType;
                return;
            }
        }

        for (Decl* global : m_Context->m_ActiveCompUnit->Globals) {
            ARIA_ASSERT(global->Kind == DeclKind::Var, "Invalid global in Globals");

            if (global->Var.Identifier == ref.Identifier) {
                ref.Kind = DeclRefKind::GlobalVar;
                expr->Type = global->Var.Type;
                return;
            }
        }

        for (Decl* func : m_Context->m_ActiveCompUnit->Funcs) {
            ARIA_ASSERT(func->Kind == DeclKind::Function, "Invalid function in Funcs");

            if (func->Function.Identifier == ref.Identifier) {
                ref.Kind = DeclRefKind::Function;
                expr->Type = func->Function.Type;
                return;
            }
        }

        m_Context->ReportCompilerError(expr->Loc, expr->Range, fmt::format("Undeclared identifier \"{}\"", ref.Identifier));
        expr->Type = m_ErrorType;
    }

    void TypeChecker::HandleMemberExpr(Expr* expr) {
        MemberExpr& mem = expr->Member;

        HandleExpr(mem.Parent);
        TypeInfo* parentType = mem.Parent->Type;
        TypeInfo* memberType = nullptr;
        StructDeclaration& sd = std::get<StructDeclaration>(parentType->Data);

        StructDecl s = sd.SourceDecl->Struct;

        for (Decl* field : s.Fields) {
            if (field->Kind == DeclKind::Field) {
                FieldDecl fd = field->Field;
                if (fd.Identifier == mem.Member) {
                    memberType = fd.Type;
                }
            } else if (field->Kind == DeclKind::Method) {
                MethodDecl md = field->Method;
                if (md.Identifier == mem.Member) {
                    memberType = md.Type;
                }
            }
        }

        if (!memberType) {
            m_Context->ReportCompilerError(expr->Loc, expr->Range, fmt::format("Unknown member \"{}\" in '{}'", mem.Member, TypeInfoToString(parentType)));
            expr->Type = m_ErrorType;
            return;
        }

        expr->Type = memberType;
    }

    void TypeChecker::HandleCallExpr(Expr* expr) {
        CallExpr& call = expr->Call;

        HandleExpr(call.Callee);
        TypeInfo* calleeType = call.Callee->Type;

        if (calleeType->Type != PrimitiveType::Function && !calleeType->IsError()) {
            m_Context->ReportCompilerError(expr->Loc, expr->Range, "Cannot call an object of non-function type");
            expr->Type = m_ErrorType;
            return;
        }

        if (!calleeType->IsError()) {
            FunctionDeclaration& fnDecl = std::get<FunctionDeclaration>(calleeType->Data);

            if (fnDecl.ParamTypes.Size != call.Arguments.Size) {
                m_Context->ReportCompilerError(expr->Loc, expr->Range, fmt::format("Mismatched argument count, function expects {} but got {}", fnDecl.ParamTypes.Size, call.Arguments.Size));
                for (size_t i = 0; i < call.Arguments.Size; i++) {
                    call.Arguments.Items[i]->Type = m_ErrorType;
                }
            } else {
                for (size_t i = 0; i < fnDecl.ParamTypes.Size; i++) {
                    HandleInitializer(call.Arguments.Items[i], fnDecl.ParamTypes.Items[i], true);
                }
            }

            expr->Type = fnDecl.ReturnType;
            expr->ValueKind = (fnDecl.ReturnType->IsReference()) ? ExprValueKind::LValue : ExprValueKind::RValue;
        }
    }

    void TypeChecker::HandleMethodCallExpr(Expr* expr) {
        // MethodCallExpr* call = GetNode<MethodCallExpr>(expr);
        // TypeInfo* calleeType = HandleExpr(call->GetCallee())->GetResolvedType();
        // 
        // FunctionDeclaration& fnDecl = std::get<FunctionDeclaration>(calleeType->Data);
        // 
        // if (fnDecl.ParamTypes.Size != call->GetArguments().Size) {
        //     ARIA_ASSERT(false, "todo: error msg");
        // }
        // 
        // for (size_t i = 0; i < fnDecl.ParamTypes.Size; i++) {
        //     TypeInfo* paramType = fnDecl.ParamTypes.Items[i];
        //     TypeInfo* argType = HandleExpr(call->GetArguments().Items[i])->GetResolvedType();
        // 
        //     ConversionCost cost = GetConversionCost(paramType, argType, call->GetArguments().Items[i]->GetValueKind());
        //     if (cost.CastNeeded) {
        //         if (cost.ImplicitCastPossible) {
        //             call->SetArgument(i, InsertImplicitCast(paramType, argType, call->GetArguments().Items[i], cost.CaKind));
        //         } else {
        //             ARIA_ASSERT(false, "todo: error msg");
        //         }
        //     }
        // }
        // 
        // call->SetResolvedType(fnDecl.ReturnType);
        // return call;
        ARIA_ASSERT(false, "todo!");
    }

    void TypeChecker::HandleParenExpr(Expr* expr) {
        ParenExpr& paren = expr->Paren;
        HandleExpr(paren.Expression);

        expr->Type = paren.Expression->Type;
        expr->ValueKind = paren.Expression->ValueKind;
    }

    void TypeChecker::HandleCastExpr(Expr* expr) {
        CastExpr& cast = expr->Cast;
        
        HandleExpr(cast.Expression);
        expr->Type = cast.Type;

        TypeInfo* srcType = cast.Expression->Type;
        TypeInfo* dstType = cast.Type;

        ConversionCost cost = GetConversionCost(dstType, srcType, cast.Expression->ValueKind);

        if (cost.CastNeeded) {
            if (cost.ExplicitCastPossible) {
                cast.Kind = cost.CaKind;
            } else {
                ARIA_ASSERT(false, "todo: add error message");
            }
        }
    }

    void TypeChecker::HandleUnaryOperatorExpr(Expr* expr) {
        UnaryOperatorExpr& unop = expr->UnaryOperator;
        
        HandleExpr(unop.Expression);
        RequireRValue(unop.Expression);
        TypeInfo* type = unop.Expression->Type;
        
        switch (unop.Operator) {
            case UnaryOperatorKind::Negate: {
                ARIA_ASSERT(type->IsNumeric(), "todo: add error message");
                expr->Type = type;
                break;
            }

            default: ARIA_UNREACHABLE();
        }
    }

    void TypeChecker::HandleBinaryOperatorExpr(Expr* expr) {
        BinaryOperatorExpr& binop = expr->BinaryOperator;

        HandleExpr(binop.LHS);
        HandleExpr(binop.RHS);

        Expr* LHS = binop.LHS;
        Expr* RHS = binop.RHS;

        switch (binop.Operator) {
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
                if (!LHS->Type->IsError() && !RHS->Type->IsError()) {
                    if (!LHS->Type->IsNumeric()) {
                        m_Context->ReportCompilerError(LHS->Loc, LHS->Range, fmt::format("Expression must be of a numeric type but is of type '{}'", TypeInfoToString(LHS->Type)));
                    }

                    if (!LHS->Type->IsNumeric()) {
                        m_Context->ReportCompilerError(RHS->Loc, RHS->Range, fmt::format("Expression must be of a numeric type but is of type '{}'", TypeInfoToString(RHS->Type)));
                    }
                }

                InsertArithmeticPromotion(LHS, RHS);

                if (binop.Operator == BinaryOperatorKind::Less ||
                    binop.Operator == BinaryOperatorKind::LessOrEq ||
                    binop.Operator == BinaryOperatorKind::Greater ||
                    binop.Operator == BinaryOperatorKind::GreaterOrEq ||
                    binop.Operator == BinaryOperatorKind::IsEq ||
                    binop.Operator == BinaryOperatorKind::IsNotEq) 
                {
                    TypeInfo* boolType = TypeInfo::Create(m_Context, PrimitiveType::Bool, false);
                    expr->Type = boolType;
                    expr->ValueKind = ExprValueKind::RValue;
                    return;
                }

                expr->Type = LHS->Type;
                expr->ValueKind = ExprValueKind::RValue;

                return;
            }

            case BinaryOperatorKind::Eq: {
                if (LHS->ValueKind != ExprValueKind::LValue) {
                    m_Context->ReportCompilerError(LHS->Loc, LHS->Range, "Expression must be a modifiable lvalue");
                }

                ConversionCost cost = GetConversionCost(LHS->Type, RHS->Type, RHS->ValueKind);

                if (cost.CastNeeded) {
                    if (cost.ImplicitCastPossible) {
                        InsertImplicitCast(LHS->Type, RHS->Type, RHS, cost.CaKind);
                    } else {
                        m_Context->ReportCompilerError(expr->Loc, expr->Range, fmt::format("Cannot implicitly convert from '{}' to '{}'", TypeInfoToString(RHS->Type), TypeInfoToString(LHS->Type)));
                    }
                }

                expr->Type = LHS->Type;
                expr->ValueKind = ExprValueKind::LValue;

                return;
            }

            case BinaryOperatorKind::LogAnd:
            case BinaryOperatorKind::LogOr: {
                TypeInfo* boolType = TypeInfo::Create(m_Context, PrimitiveType::Bool, false);

                ConversionCost costLHS = GetConversionCost(boolType, LHS->Type, LHS->ValueKind);
                ConversionCost costRHS = GetConversionCost(boolType, RHS->Type, RHS->ValueKind);

                if (costLHS.CastNeeded) {
                    if (costLHS.ImplicitCastPossible) {
                        InsertImplicitCast(boolType, LHS->Type, LHS, costLHS.CaKind);
                    } else {
                        m_Context->ReportCompilerError(LHS->Loc, LHS->Range, fmt::format("Cannot implicitly convert from '{}' to 'bool'", TypeInfoToString(LHS->Type)));
                    }
                }

                if (costRHS.CastNeeded) {
                    if (costRHS.ImplicitCastPossible) {
                        InsertImplicitCast(boolType, RHS->Type, RHS, costRHS.CaKind);
                    } else {
                        m_Context->ReportCompilerError(LHS->Loc, LHS->Range, fmt::format("Cannot implicitly convert from '{}' to 'bool'", TypeInfoToString(RHS->Type)));
                    }
                }

                expr->Type = boolType;
                expr->ValueKind = ExprValueKind::RValue;

                return;
            }
        }

        ARIA_UNREACHABLE();
    }

    void TypeChecker::HandleCompoundAssignExpr(Expr* expr) {
        CompoundAssignExpr& compAss = expr->CompoundAssign;
        
        HandleExpr(compAss.LHS);
        HandleExpr(compAss.RHS);
        
        Expr* LHS = compAss.LHS;
        Expr* RHS = compAss.RHS;
        
        TypeInfo* LHSType = LHS->Type;
        TypeInfo* RHSType = RHS->Type;
        
        if (LHS->ValueKind != ExprValueKind::LValue) {
            m_Context->ReportCompilerError(compAss.LHS->Loc, compAss.LHS->Range, "Expression must be a modifiable lvalue");
        }
        
        ConversionCost cost = GetConversionCost(LHSType, RHSType, compAss.RHS->ValueKind);
        
        if (cost.CastNeeded) {
            if (cost.ImplicitCastPossible) {
                InsertImplicitCast(LHSType, RHSType, RHS, cost.CaKind);
                RHSType = LHSType;
            } else {
                m_Context->ReportCompilerError(compAss.RHS->Loc, compAss.RHS->Range, fmt::format("Cannot implicitly convert from '{}' to '{}'", TypeInfoToString(RHSType), TypeInfoToString(LHSType)));
            }
        }
        
        expr->Type = LHSType;
        expr->ValueKind = ExprValueKind::LValue;
    }

    void TypeChecker::HandleExpr(Expr* expr) {
        if (expr->Kind == ExprKind::Error) { return; }
        else if (expr->Kind == ExprKind::BooleanConstant) {
            return HandleBooleanConstantExpr(expr);
        } else if (expr->Kind == ExprKind::CharacterConstant) {
            return HandleCharacterConstantExpr(expr);
        } else if (expr->Kind == ExprKind::IntegerConstant) {
            return HandleIntegerConstantExpr(expr);
        } else if (expr->Kind == ExprKind::FloatingConstant) {
            return HandleFloatingConstantExpr(expr);
        } else if (expr->Kind == ExprKind::StringConstant) {
            return HandleStringConstantExpr(expr);
        } else if (expr->Kind == ExprKind::DeclRef) {
            return HandleDeclRefExpr(expr);
        } else if (expr->Kind == ExprKind::Member) {
            return HandleMemberExpr(expr);
        } else if (expr->Kind == ExprKind::Call) {
            return HandleCallExpr(expr);
        } else if (expr->Kind == ExprKind::MethodCall) {
            return HandleMethodCallExpr(expr);
        } else if (expr->Kind == ExprKind::Paren) {
            return HandleParenExpr(expr);
        } else if (expr->Kind == ExprKind::Cast) {
            return HandleCastExpr(expr);
        } else if (expr->Kind == ExprKind::UnaryOperator) {
            return HandleUnaryOperatorExpr(expr);
        } else if (expr->Kind == ExprKind::BinaryOperator) {
            return HandleBinaryOperatorExpr(expr);
        } else if (expr->Kind == ExprKind::CompoundAssign) {
            return HandleCompoundAssignExpr(expr);
        }

        ARIA_UNREACHABLE();
    }

    void TypeChecker::HandleTranslationUnitDecl(Decl* decl) {
        TranslationUnitDecl tu = decl->TranslationUnit;       

        for (Stmt* stmt : tu.Stmts) {
            HandleStmt(stmt);
        }
    }

    void TypeChecker::HandleVarDecl(Decl* decl) {
        VarDecl varDecl = decl->Var;
        std::string ident = fmt::format("{}", varDecl.Identifier);

        if (varDecl.Type->IsVoid()) {
            m_Context->ReportCompilerError(decl->Loc, decl->Range, "Cannot declare variable of 'void' type");
        }

        HandleInitializer(varDecl.DefaultValue, varDecl.Type, false);

        if (m_Scopes.size() == 0) {
            m_Context->m_ActiveCompUnit->Globals.push_back(decl);
        } else {
            if (m_Scopes.back().Declarations.contains(ident)) {
                m_Context->ReportCompilerError(decl->Loc, decl->Range, fmt::format("Redeclaring symbol '{}'", ident));
            }

            m_Scopes.back().Declarations[ident] = { varDecl.Type, decl, DeclRefKind::LocalVar };
        }
    }

    void TypeChecker::HandleParamDecl(Decl* decl) {
        ParamDecl paramDecl = decl->Param;
        m_Scopes.back().Declarations[fmt::format("{}", paramDecl.Identifier)] = { paramDecl.Type, decl, DeclRefKind::ParamVar };
    }

    void TypeChecker::HandleFunctionDecl(Decl* decl) {
        FunctionDecl fnDecl = decl->Function;
        
        std::string ident = fmt::format("{}", fnDecl.Identifier);

        FunctionDecl* prevDecl = nullptr;
        for (Decl* func : m_Context->m_ActiveCompUnit->Funcs) {
            ARIA_ASSERT(func->Kind == DeclKind::Function, "Invalid function in Funcs");

            if (func->Function.Identifier == fnDecl.Identifier) { prevDecl = &func->Function; }
        }

        if (prevDecl) {
            TypeInfo* type = prevDecl->Type;
            if (!TypeIsEqual(fnDecl.Type, type)) {
                m_Context->ReportCompilerError(decl->Loc, decl->Range, fmt::format("Redeclaring function '{}' with different type '{}'", ident, TypeInfoToString(fnDecl.Type)));
            } else {
                if (prevDecl->Body != nullptr) {
                    m_Context->ReportCompilerError(decl->Loc, decl->Range, fmt::format("Redefining function body of '{}'", ident));
                }
            }
        }

        m_Context->m_ActiveCompUnit->Funcs.push_back(decl);

        if (fnDecl.Body) {
            m_ActiveReturnType = std::get<FunctionDeclaration>(fnDecl.Type->Data).ReturnType;
            PushScope();
            
            for (Decl* p : fnDecl.Parameters) {
                HandleParamDecl(p);
            }
            
            if (fnDecl.Body) {
                HandleBlockStmt(fnDecl.Body);
            }
            
            PopScope();
            m_ActiveReturnType = nullptr;
        }
    }

    void TypeChecker::HandleStructDecl(Decl* decl) {
        StructDecl& s = decl->Struct;
        
        StructDeclaration d;
        d.Identifier = s.Identifier;
        d.SourceDecl = decl;
        
        TypeInfo* structType = TypeInfo::Create(m_Context, PrimitiveType::Structure, false, d);
        
        for (Decl* field : s.Fields) {
            if (field->Kind == DeclKind::Field) {}
        }
        
        m_Context->m_ActiveCompUnit->Structs.push_back(decl);
        m_ActiveStruct = TypeInfo::Create(m_Context, structType->Type, true, structType->Data);
        
        // for (MethodDecl* md : methods) {
        //     TypeInfo* returnType = GetTypeInfoFromString(md->GetParsedType());
        //     TinyVector<TypeInfo*> paramTypes;
        //     m_ActiveReturnType = returnType;
        //     
        // 
        //     m_Declarations.emplace_back();
        // 
        //     for (ParamDecl* p : md->GetParameters()) {
        //         HandleParamDecl(p);
        //         TypeInfo* pType = p->GetResolvedType();
        //         paramTypes.Append(m_Context, pType);
        //     }
        // 
        //     // We make the function visible to itself by declaring it before the body
        //     FunctionDeclaration fd;
        //     fd.ParamTypes = paramTypes;
        //     fd.ReturnType = returnType;
        //     
        //     TypeInfo* resolvedType = TypeInfo::Create(m_Context, PrimitiveType::Function, false, fd);
        //     md->SetResolvedType(resolvedType);
        // 
        //     std::string ident = md->GetIdentifier();
        //     m_Declarations.front()[ident] = { md->GetResolvedType(), decl, DeclRefKind::Function };
        // 
        //     if (md->GetBody()) {
        //         HandleCompoundStmt(md->GetBody());
        //     }
        // 
        //     m_Declarations.pop_back();
        //     m_ActiveReturnType = nullptr;
        // }
        
        m_ActiveStruct = nullptr;
    }

    void TypeChecker::HandleDecl(Decl* decl) {
        if(decl->Kind == DeclKind::Error) { return; }
        else if (decl->Kind == DeclKind::TranslationUnit) {
            return HandleTranslationUnitDecl(decl);
        } else if (decl->Kind == DeclKind::Module) {
            return;
        } else if (decl->Kind == DeclKind::Var) {
            return HandleVarDecl(decl);
        } else if (decl->Kind == DeclKind::Param) {
            return HandleParamDecl(decl);
        } else if (decl->Kind == DeclKind::Function) {
            return HandleFunctionDecl(decl);
        } else if (decl->Kind == DeclKind::Struct) {
            return HandleStructDecl(decl);
        }

        ARIA_UNREACHABLE();
    }

    void TypeChecker::HandleBlockStmt(Stmt* stmt) {
        BlockStmt block = stmt->Block;

        for (Stmt* s : block.Stmts) {
            HandleStmt(s);
        }
    }

    void TypeChecker::HandleWhileStmt(Stmt* stmt) {
        WhileStmt wh = stmt->While;

        PushScope(true, true);

        HandleExpr(wh.Condition);
        RequireRValue(wh.Condition);

        if (!wh.Condition->Type->IsBoolean()) {
            ARIA_ASSERT(false, "todo: add error");
        }

        HandleBlockStmt(wh.Body);

        PopScope();
    }

    void TypeChecker::HandleDoWhileStmt(Stmt* stmt) {
        DoWhileStmt wh = stmt->DoWhile;

        PushScope(true, true);

        HandleExpr(wh.Condition);
        RequireRValue(wh.Condition);

        if (!wh.Condition->Type->IsBoolean()) {
            ARIA_ASSERT(false, "todo: add error");
        }

        HandleBlockStmt(wh.Body);

        PopScope();
    }

    void TypeChecker::HandleForStmt(Stmt* stmt) {
        ForStmt fs = stmt->For;

        PushScope(true, true);
        
        if (fs.Prologue) { HandleDecl(fs.Prologue); }

        if (fs.Condition) {
            HandleExpr(fs.Condition);
            RequireRValue(fs.Condition);

            if (!fs.Condition->Type->IsBoolean() && !fs.Condition->Type->IsError()) {
                m_Context->ReportCompilerError(fs.Condition->Loc, fs.Condition->Range, fmt::format("While loop condition must be of a boolean type but is '{}'", TypeInfoToString(fs.Condition->Type)));
            }
        }

        if (fs.Step) { HandleExpr(fs.Step); }
        HandleBlockStmt(fs.Body);
        
        PopScope();
    }

    void TypeChecker::HandleIfStmt(Stmt* stmt) {
        IfStmt ifs = stmt->If;
        HandleExpr(ifs.Condition);
        RequireRValue(ifs.Condition);

        if (!ifs.Condition->Type->IsBoolean()) {
            ARIA_ASSERT(false, "todo: add error");
        }

        HandleBlockStmt(ifs.Body);
    }

    void TypeChecker::HandleBreakStmt(Stmt* stmt) {
        if (!m_Scopes.back().AllowBreakStmt) {
            m_Context->ReportCompilerError(stmt->Loc, stmt->Range, "Cannot use 'break' here");
        }
    }

    void TypeChecker::HandleContinueStmt(Stmt* stmt) {
        if (!m_Scopes.back().AllowContinueStmt) {
            m_Context->ReportCompilerError(stmt->Loc, stmt->Range, "Cannot use 'continue' here");
        }
    }

    void TypeChecker::HandleReturnStmt(Stmt* stmt) {
        ReturnStmt& ret = stmt->Return;
        
        if (m_ActiveReturnType == nullptr) {
            m_Context->ReportCompilerError(stmt->Loc, stmt->Range, "\"return\" statement out of function body is not allowed");
            ret.Value->Type = m_ErrorType;
            return;
        }
        
        HandleInitializer(ret.Value, m_ActiveReturnType, false);
    }

    void TypeChecker::HandleStmt(Stmt* stmt) {
        if (stmt->Kind == StmtKind::Error) { return; }
        else if (stmt->Kind == StmtKind::Nop) { return; }
        else if (stmt->Kind == StmtKind::Import) {
            return;
        }
        else if (stmt->Kind == StmtKind::Block) {
            PushScope();
            HandleBlockStmt(stmt);
            PopScope();
            return;
        } else if (stmt->Kind == StmtKind::While) {
            return HandleWhileStmt(stmt);
        } else if (stmt->Kind == StmtKind::DoWhile) {
            return HandleDoWhileStmt(stmt);
        } else if (stmt->Kind == StmtKind::For) {
            return HandleForStmt(stmt);
        } else if (stmt->Kind == StmtKind::If) {
            return HandleIfStmt(stmt);
        } else if (stmt->Kind == StmtKind::Break) {
            return HandleBreakStmt(stmt);
        } else if (stmt->Kind == StmtKind::Continue) {
            return HandleContinueStmt(stmt);
        } else if (stmt->Kind == StmtKind::Return) {
            return HandleReturnStmt(stmt);
        } else if (stmt->Kind == StmtKind::Expr) {
            HandleExpr(stmt->ExprStmt);
            stmt->ExprStmt->IsStmtExpr = true;
            return;
        } else if (stmt->Kind == StmtKind::Decl) {
            return HandleDecl(stmt->DeclStmt);
        }

        ARIA_UNREACHABLE();
    }

    void TypeChecker::HandleInitializer(Expr* initializer, TypeInfo* type, bool temporary) {
        // If we are initializing a reference, the initializer must be of the same type and an lvalue
        if (type->IsReference()) {
            ARIA_ASSERT(initializer != nullptr, "initial value of a reference must be an lvalue");
            HandleExpr(initializer);
            TypeInfo* initType = initializer->Type;

            if (!TypeIsEqual(type, initType) || initializer->ValueKind != ExprValueKind::LValue) {
                m_Context->ReportCompilerError(initializer->Loc, initializer->Range, "Initial value of reference must be an lvalue");
            }
        } else if (initializer) {
            HandleExpr(initializer);
            TypeInfo* initType = initializer->Type;

            if (initializer->ValueKind == ExprValueKind::LValue) {
                if (initType->Type == PrimitiveType::String) {
                    ReplaceExpr(initializer, Expr::Create(m_Context, initializer->Loc, initializer->Range, 
                        ExprKind::Copy, ExprValueKind::LValue, initializer->Type, 
                        CopyExpr(Expr::Dup(m_Context, initializer), m_BuiltInStringCopyConstructor)));
                }
            }

            ConversionCost cost = GetConversionCost(type, initType, initializer->ValueKind);
            if (cost.CastNeeded) {
                if (cost.ImplicitCastPossible) {
                    InsertImplicitCast(type, initType, initializer, cost.CaKind);
                } else {
                    m_Context->ReportCompilerError(initializer->Loc, initializer->Range, fmt::format("Cannot implicitly convert from '{}' to '{}'", TypeInfoToString(initType), TypeInfoToString(type)));
                }
            }

            if (temporary) {
                if (initializer->Type->Type == PrimitiveType::String) {
                    ReplaceExpr(initializer, Expr::Create(m_Context, initializer->Loc, initializer->Range, 
                        ExprKind::Temporary, ExprValueKind::RValue, initializer->Type, 
                        TemporaryExpr(Expr::Dup(m_Context, initializer), m_BuiltInStringDestructor)));
                    m_TemporaryContext = true;
                }
            }
        }
    }

    void TypeChecker::PushScope(bool allowBreak, bool allowContinue) {
        Scope s;

        if (m_Scopes.size() > 0) {
            if (m_Scopes.back().AllowBreakStmt) { s.AllowBreakStmt = true; }
            else { s.AllowBreakStmt = allowBreak; }

            if (m_Scopes.back().AllowContinueStmt) { s.AllowContinueStmt = true; }
            else { s.AllowContinueStmt = allowContinue; }
        } else {
            s.AllowBreakStmt = allowBreak;
            s.AllowContinueStmt = allowContinue;
        }

        m_Scopes.push_back(s);
    }

    void TypeChecker::PopScope() {
        m_Scopes.pop_back();
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

    void TypeChecker::InsertImplicitCast(TypeInfo* dstType, TypeInfo* srcType, Expr* srcExpr, CastKind castKind) {
        Expr* src = Expr::Dup(m_Context, srcExpr); // We must copy the original expression to avoid overwriting the same memory
        Expr* implicitCast = Expr::Create(m_Context, src->Loc, src->Range, ExprKind::ImplicitCast, ExprValueKind::RValue, dstType, ImplicitCastExpr(src, castKind));

        ReplaceExpr(srcExpr, implicitCast);
    }

    void TypeChecker::RequireRValue(Expr* expr) {
        if (expr->ValueKind == ExprValueKind::LValue) {
            InsertImplicitCast(expr->Type, expr->Type, expr, CastKind::LValueToRValue);
        }
    }

    void TypeChecker::InsertArithmeticPromotion(Expr* lhs, Expr* rhs) {
        TypeInfo* lhsType = lhs->Type;
        TypeInfo* rhsType = rhs->Type;

        if (lhsType->Type == PrimitiveType::Error || rhsType->Type == PrimitiveType::Error) {
            return;
        }

        if (TypeIsEqual(lhsType, rhsType)) {
            RequireRValue(lhs);
            RequireRValue(rhs);
            return;
        }

        if (lhsType->IsIntegral() && rhsType->IsIntegral()) {
            size_t lSize = TypeGetSize(lhsType);
            size_t rSize = TypeGetSize(rhsType);

            if (lSize > rSize) {
                InsertImplicitCast(lhsType, rhsType, rhs, CastKind::Integral);
                RequireRValue(lhs);
            } else if (rSize > lSize) {
                InsertImplicitCast(rhsType, lhsType, lhs, CastKind::Integral);
                RequireRValue(rhs);
            } else if (lSize == rSize) {
                // We know that the types are not equal so we likely have a signed/unsigned mismatch
                m_Context->ReportCompilerError(lhs->Loc, SourceRange(lhs->Range.Start, rhs->Range.End), fmt::format("Mismatched types '{}' and '{}' (implicit signedness conversions are not allowed here)", TypeInfoToString(lhsType), TypeInfoToString(rhsType)));
            }

            return;
        }

        if (lhsType->IsIntegral() && rhsType->IsFloatingPoint()) {
            InsertImplicitCast(rhsType, lhsType, lhs, CastKind::IntegralToFloating);
            RequireRValue(rhs);
            return;
        }

        if (lhsType->IsFloatingPoint() && rhsType->IsIntegral()) {
            InsertImplicitCast(lhsType, rhsType, rhs, CastKind::IntegralToFloating);
            RequireRValue(lhs);
            return;
        }

        if (lhsType->IsFloatingPoint() && rhsType->IsFloatingPoint()) {
            size_t lSize = TypeGetSize(lhsType);
            size_t rSize = TypeGetSize(rhsType);

            if (lSize > rSize) {
                InsertImplicitCast(lhsType, rhsType, rhs, CastKind::Floating);
                RequireRValue(lhs);
            } else if (rSize > lSize) {
                InsertImplicitCast(rhsType, lhsType, lhs, CastKind::Floating);
                RequireRValue(rhs);
            }

            return;
        }

        ARIA_UNREACHABLE();
    }

    void TypeChecker::ReplaceExpr(Expr* src, Expr* newExpr) {
        *src = *newExpr;
    }

    bool TypeChecker::TypeIsEqual(TypeInfo* lhs, TypeInfo* rhs) {
        if (lhs->IsTrivial() && rhs->IsTrivial()) {
            return lhs->Type == rhs->Type;
        }

        if (lhs->IsString() && rhs->IsString()) {
            return true;
        }

        if (lhs->IsFunction() && rhs->IsFunction()) {
            FunctionDeclaration& fLhs = std::get<FunctionDeclaration>(lhs->Data);
            FunctionDeclaration& fRhs = std::get<FunctionDeclaration>(rhs->Data);

            if (!TypeIsEqual(fLhs.ReturnType, fRhs.ReturnType)) { return false; }
            if (fLhs.ParamTypes.Size != fRhs.ParamTypes.Size) { return false; }

            for (size_t i = 0; i < fLhs.ParamTypes.Size; i++) {
                if (!TypeIsEqual(fLhs.ParamTypes.Items[i], fRhs.ParamTypes.Items[i])) { return false; }
            }

            return true;
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
