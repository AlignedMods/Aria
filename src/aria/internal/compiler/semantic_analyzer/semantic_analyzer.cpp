#include "aria/internal/compiler/semantic_analyzer/semantic_analyzer.hpp"

namespace Aria::Internal {

    SemanticAnalyzer::SemanticAnalyzer(CompilationContext* ctx) {
        m_Context = ctx;

        m_BuiltInStringDestructor = Decl::Create(m_Context, {}, {}, DeclKind::BuiltinDestructor, 0, BuiltinDestructorDecl(BuiltinKind::String));
        m_BuiltInStringCopyConstructor = Decl::Create(m_Context, {}, {}, DeclKind::BuiltinCopyConstructor, 0, BuiltinCopyConstructorDecl(BuiltinKind::String));

        SemaImpl();
    }

    void SemanticAnalyzer::SemaImpl() {
        PassImports();
        PassDecls();
        PassCode();
    }

    void SemanticAnalyzer::PassImports() {
        for (CompilationUnit* unit : m_Context->CompilationUnits) {
            m_Context->ActiveCompUnit = unit;
            ResolveUnitImports(unit->Parent, unit);
        }
    }

    void SemanticAnalyzer::PassDecls() {
        for (Module* mod : m_Context->Modules) {
            ResolveModuleDecls(mod);
        }
    }

    void SemanticAnalyzer::PassCode() {
        for (Module* mod : m_Context->Modules) {
            ResolveModuleCode(mod);
        }
    }

    void SemanticAnalyzer::AddUnitToModule(Module* module, CompilationUnit* unit) {
        for (CompilationUnit* comp : module->Units) {
            if (comp == unit) {
                return;
            }
        }

        module->Units.push_back(unit);
    }

    void SemanticAnalyzer::ResolveModuleImports(Module* module) {
        std::vector<CompilationUnit*> units = module->Units;

        for (CompilationUnit* unit : units) {
            ResolveUnitImports(module, unit);
        }

        m_ImportedModules.clear();
    }

    void SemanticAnalyzer::ResolveUnitImports(Module* module, CompilationUnit* unit) {
        m_ImportedModules[module->Name] = true;

        for (size_t i = 0; i < unit->Imports.size(); i++) {
            Stmt* stmt = unit->Imports[i];

            if (stmt->Kind == StmtKind::Error) { return; }
            ARIA_ASSERT(stmt->Kind == StmtKind::Import, "Invalid stmt in Imports");

            if (stmt->Import.Name == module->Name) {
                m_Context->ReportCompilerDiagnostic(stmt->Loc, stmt->Range, "Including self is not allowed");
                stmt->Kind = StmtKind::Error;
                return;
            }

            if (m_ImportedModules.contains(fmt::format("{}", stmt->Import.Name))) {
                m_Context->ReportCompilerDiagnostic(stmt->Loc, stmt->Range, "Recursive imports are not allowed");
                stmt->Kind = StmtKind::Error;
                return;
            }

            Module* resolvedModule = nullptr;

            for (size_t i = 0; i < m_Context->Modules.size(); i++) {
                Module* mod = m_Context->Modules[i];

                if (mod->Name == stmt->Import.Name) {
                    ResolveModuleImports(mod);
                    resolvedModule = mod;
                    break;
                }
            }

            if (!resolvedModule) {
                m_Context->ReportCompilerDiagnostic(stmt->Loc, stmt->Range, fmt::format("Could not find module '{}'", stmt->Import.Name));
            }

            stmt->Import.ResolvedModule = resolvedModule;
        }

        AddUnitToModule(module, unit);
        m_ImportedModules.clear();
    }

    void SemanticAnalyzer::ResolveModuleDecls(Module* module) {
        for (CompilationUnit* unit : module->Units) {
            ResolveUnitDecls(module, unit);
        }
    }

    void SemanticAnalyzer::ResolveUnitDecls(Module* module, CompilationUnit* unit) {
        m_Context->ActiveCompUnit = unit;

        for (Decl* global : unit->Globals) {
            ARIA_ASSERT(global->Kind == DeclKind::Var, "Invalid global in globals");

            VarDecl& var = global->Var;
            std::string ident = fmt::format("{}", var.Identifier);

            module->Symbols[ident] = global;
            unit->LocalSymbols[ident] = global;
        }

        for (Decl* func : unit->Funcs) {
            ARIA_ASSERT(func->Kind == DeclKind::Function, "Invalid func in funcs");

            FunctionDecl& f = func->Function;

            // do not mangle the main function
            if (f.Identifier == "main") { func->Flags |= DECL_FLAG_NOMANGLE; }

            std::string ident = fmt::format("{}", f.Identifier);

            if (module->Symbols.contains(ident)) {
                Decl* d = module->Symbols.at(ident);
                
                if (d->Kind == DeclKind::Function) { 
                    m_Context->ReportCompilerDiagnostic(func->Loc, func->Range, fmt::format("Redefining function '{}'", ident));
                } else if (d->Kind == DeclKind::Var) {
                    m_Context->ReportCompilerDiagnostic(func->Loc, func->Range, fmt::format("Redefining global variable '{}' as function", ident));
                }

                func->Kind = DeclKind::Error;
                return;
            }

            module->Symbols[ident] = func;
            unit->LocalSymbols[ident] = func;
        }

        for (Decl* struc : unit->Structs) {
            ARIA_ASSERT(struc->Kind == DeclKind::Struct, "Invalid struct in structs");

            StructDecl& s = struc->Struct;
            std::string ident = fmt::format("{}", s.Identifier);

            module->Symbols[ident] = struc;
            unit->LocalSymbols[ident] = struc;
        }
    }

    void SemanticAnalyzer::ResolveModuleCode(Module* module) {
        for (CompilationUnit* unit : module->Units) {
            ResolveUnitCode(module, unit);
        }
    }

    void SemanticAnalyzer::ResolveUnitCode(Module* module, CompilationUnit* unit) {
        m_Context->ActiveCompUnit = unit;
        ResolveStmt(unit->RootASTNode);
    }

    void SemanticAnalyzer::ResolveBooleanConstantExpr(Expr* expr) {
        if (expr->ResultDiscarded) {
            m_Context->ReportCompilerDiagnostic(expr->Loc, expr->Range, "Discarding result of boolean literal", CompilerDiagKind::Warning);
        }
    }

    void SemanticAnalyzer::ResolveCharacterConstantExpr(Expr* expr) {
        if (expr->ResultDiscarded) {
            m_Context->ReportCompilerDiagnostic(expr->Loc, expr->Range, "Discarding result of character literal", CompilerDiagKind::Warning);
        }
    }

    void SemanticAnalyzer::ResolveIntegerConstantExpr(Expr* expr) {
        if (expr->ResultDiscarded) {
            m_Context->ReportCompilerDiagnostic(expr->Loc, expr->Range, "Discarding result of integer literal", CompilerDiagKind::Warning);
        }
    }

    void SemanticAnalyzer::ResolveFloatingConstantExpr(Expr* expr) {
        if (expr->ResultDiscarded) {
            m_Context->ReportCompilerDiagnostic(expr->Loc, expr->Range, "Discarding result of floating point literal", CompilerDiagKind::Warning);
        }
    }

    void SemanticAnalyzer::ResolveStringConstantExpr(Expr* expr) {
        if (expr->ResultDiscarded) {
            m_Context->ReportCompilerDiagnostic(expr->Loc, expr->Range, "Discarding result of string literal is not allowed");
        }
    }

    void SemanticAnalyzer::ResolveDeclRefExpr(Expr* expr) {
        DeclRefExpr& ref = expr->DeclRef;
        std::string ident = fmt::format("{}", ref.Identifier);

        if (expr->ResultDiscarded) {
            m_Context->ReportCompilerDiagnostic(expr->Loc, expr->Range, "Discarding result of identifier", CompilerDiagKind::Warning);
        }

        if (ref.NameSpecifier) {
            ARIA_ASSERT(ref.NameSpecifier->Kind == SpecifierKind::Scope, "Invalid specifier");

            ScopeSpecifier& scope = ref.NameSpecifier->Scope;

            if (scope.Identifier == m_Context->ActiveCompUnit->Parent->Name) {
                scope.ReferencedModule = m_Context->ActiveCompUnit->Parent;
            } else {
                for (Stmt* stmt : m_Context->ActiveCompUnit->Imports) {
                    ARIA_ASSERT(stmt->Kind == StmtKind::Import, "Invalid import");
                
                    if (stmt->Import.Name == scope.Identifier) {
                        scope.ReferencedModule = stmt->Import.ResolvedModule;
                        break;
                    }
                }
            }

            if (scope.ReferencedModule) {
                Decl* d = nullptr;
                if (scope.ReferencedModule == m_Context->ActiveCompUnit->Parent) {
                    d = FindSymbolInModule(scope.ReferencedModule, ref.Identifier, true); // Allow accessing private declarations if we are in the same module
                } else {
                    d = FindSymbolInModule(scope.ReferencedModule, ref.Identifier, false);
                }

                if (!d) {
                    m_Context->ReportCompilerDiagnostic(expr->Loc, expr->Range, fmt::format("Module '{}' does not contain '{}'", scope.Identifier, ref.Identifier));
                    expr->Type = &ErrorType;
                    return;
                }

                switch (d->Kind) {
                    case DeclKind::Var: {
                        ref.Kind = DeclRefKind::GlobalVar;
                        ref.ReferencedDecl = d;
                        expr->Type = d->Var.Type;
                        break;
                    }

                    case DeclKind::Function: {
                        ref.Kind = DeclRefKind::Function;
                        ref.ReferencedDecl = d;
                        expr->Type = d->Function.Type;
                        break;
                    }

                    default: ARIA_UNREACHABLE();
                }

                return;
            } else {
                m_Context->ReportCompilerDiagnostic(ref.NameSpecifier->Loc, ref.NameSpecifier->Range, fmt::format("Could not find module '{}'", scope.Identifier));
                expr->Type = &ErrorType;
                return;
            }
        }

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
                ref.ReferencedDecl = it.Declarations.at(ident).SourceDeclaration;
                expr->Type = it.Declarations.at(ident).ResolvedType;
                return;
            }
        }

        if (Decl* d = FindSymbolInUnit(m_Context->ActiveCompUnit, ref.Identifier)) {
            switch (d->Kind) {
                case DeclKind::Var: {
                    ref.Kind = DeclRefKind::GlobalVar;
                    ref.ReferencedDecl = d;
                    expr->Type = d->Var.Type;
                    break;
                }

                case DeclKind::Function: {
                    ref.Kind = DeclRefKind::Function;
                    ref.ReferencedDecl = d;
                    expr->Type = d->Function.Type;
                    break;
                }

                default: ARIA_UNREACHABLE();
            }

            return;
        }

        m_Context->ReportCompilerDiagnostic(expr->Loc, expr->Range, fmt::format("Undeclared identifier \"{}\"", ref.Identifier));
        expr->Type = &ErrorType;
    }

    void SemanticAnalyzer::ResolveMemberExpr(Expr* expr) {
        MemberExpr& mem = expr->Member;

        ResolveExpr(mem.Parent);
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
            m_Context->ReportCompilerDiagnostic(expr->Loc, expr->Range, fmt::format("Unknown member \"{}\" in '{}'", mem.Member, TypeInfoToString(parentType)));
            expr->Type = &ErrorType;
            return;
        }

        expr->Type = memberType;

        if (expr->ResultDiscarded) {
            m_Context->ReportCompilerDiagnostic(expr->Loc, expr->Range, "Discarding result of member access", CompilerDiagKind::Warning);
        }
    }

    void SemanticAnalyzer::ResolveCallExpr(Expr* expr) {
        CallExpr& call = expr->Call;

        ResolveExpr(call.Callee);
        TypeInfo* calleeType = call.Callee->Type;

        if (calleeType->Type != PrimitiveType::Function && !calleeType->IsError()) {
            m_Context->ReportCompilerDiagnostic(expr->Loc, expr->Range, "Cannot call an object of non-function type");
            expr->Type = &ErrorType;
            return;
        }

        if (!calleeType->IsError()) {
            FunctionDeclaration& fnDecl = std::get<FunctionDeclaration>(calleeType->Data);

            if (fnDecl.ParamTypes.Size != call.Arguments.Size) {
                m_Context->ReportCompilerDiagnostic(expr->Loc, expr->Range, fmt::format("Mismatched argument count, function expects {} but got {}", fnDecl.ParamTypes.Size, call.Arguments.Size));
                for (size_t i = 0; i < call.Arguments.Size; i++) {
                    call.Arguments.Items[i]->Type = &ErrorType;
                }
            } else {
                for (size_t i = 0; i < fnDecl.ParamTypes.Size; i++) {
                    ResolveInitializer(call.Arguments.Items[i], fnDecl.ParamTypes.Items[i], true);
                }
            }

            expr->Type = fnDecl.ReturnType;
            expr->ValueKind = (fnDecl.ReturnType->IsReference()) ? ExprValueKind::LValue : ExprValueKind::RValue;

            return;
        }

        expr->Type = &ErrorType;
    }

    void SemanticAnalyzer::ResolveMethodCallExpr(Expr* expr) {
        ARIA_ASSERT(false, "todo!");
    }

    void SemanticAnalyzer::ResolveParenExpr(Expr* expr) {
        ParenExpr& paren = expr->Paren;
        ResolveExpr(paren.Expression);

        expr->Type = paren.Expression->Type;
        expr->ValueKind = paren.Expression->ValueKind;

        if (expr->ResultDiscarded) {
            m_Context->ReportCompilerDiagnostic(expr->Loc, expr->Range, "Discarding result of expression", CompilerDiagKind::Warning);
        }
    }

    void SemanticAnalyzer::ResolveCastExpr(Expr* expr) {
        CastExpr& cast = expr->Cast;
        
        ResolveExpr(cast.Expression);
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

        if (expr->ResultDiscarded) {
            m_Context->ReportCompilerDiagnostic(expr->Loc, expr->Range, "Discarding result of explicit cast", CompilerDiagKind::Warning);
        }
    }

    void SemanticAnalyzer::ResolveUnaryOperatorExpr(Expr* expr) {
        UnaryOperatorExpr& unop = expr->UnaryOperator;
        
        ResolveExpr(unop.Expression);
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

    void SemanticAnalyzer::ResolveBinaryOperatorExpr(Expr* expr) {
        BinaryOperatorExpr& binop = expr->BinaryOperator;

        ResolveExpr(binop.LHS);
        ResolveExpr(binop.RHS);

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
                if (!LHS->Type->IsError()) {
                    if (!LHS->Type->IsNumeric()) {
                        m_Context->ReportCompilerDiagnostic(LHS->Loc, LHS->Range, fmt::format("Expression must be of a numeric type but is of type '{}'", TypeInfoToString(LHS->Type)));
                    }

                    if (!LHS->Type->IsNumeric()) {
                        m_Context->ReportCompilerDiagnostic(RHS->Loc, RHS->Range, fmt::format("Expression must be of a numeric type but is of type '{}'", TypeInfoToString(RHS->Type)));
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
                    expr->Type = &BoolType;
                    expr->ValueKind = ExprValueKind::RValue;

                    if (expr->ResultDiscarded) {
                        m_Context->ReportCompilerDiagnostic(expr->Loc, expr->Range, "Discarding result of relational operator", CompilerDiagKind::Warning);
                    }
                    return;
                }

                expr->Type = LHS->Type;
                expr->ValueKind = ExprValueKind::RValue;

                if (expr->ResultDiscarded) {
                    m_Context->ReportCompilerDiagnostic(expr->Loc, expr->Range, "Discarding result of binary operator", CompilerDiagKind::Warning);
                }
                return;
            }

            case BinaryOperatorKind::BitAnd:
            case BinaryOperatorKind::BitOr:
            case BinaryOperatorKind::BitXor:
            case BinaryOperatorKind::Shl:
            case BinaryOperatorKind::Shr: {
                if (!LHS->Type->IsError()) {
                    if (!LHS->Type->IsIntegral() && !LHS->Type->IsString()) {
                        m_Context->ReportCompilerDiagnostic(LHS->Loc, LHS->Range, fmt::format("Expression must be of a integral type but is of type '{}'", TypeInfoToString(LHS->Type)));
                    }

                    if (!LHS->Type->IsIntegral()) {
                        m_Context->ReportCompilerDiagnostic(RHS->Loc, RHS->Range, fmt::format("Expression must be of a integral type but is of type '{}'", TypeInfoToString(RHS->Type)));
                    }
                }

                InsertArithmeticPromotion(LHS, RHS);

                expr->Type = LHS->Type;
                expr->ValueKind = ExprValueKind::RValue;

                if (expr->ResultDiscarded) {
                    m_Context->ReportCompilerDiagnostic(expr->Loc, expr->Range, "Discarding result of bitwise operator", CompilerDiagKind::Warning);
                }
                return;
            }

            case BinaryOperatorKind::Eq: {
                if (LHS->ValueKind != ExprValueKind::LValue) {
                    m_Context->ReportCompilerDiagnostic(LHS->Loc, LHS->Range, "Expression must be a modifiable lvalue");
                }

                ConversionCost cost = GetConversionCost(LHS->Type, RHS->Type, RHS->ValueKind);

                if (cost.CastNeeded) {
                    if (cost.ImplicitCastPossible) {
                        InsertImplicitCast(LHS->Type, RHS->Type, RHS, cost.CaKind);
                    } else {
                        m_Context->ReportCompilerDiagnostic(expr->Loc, expr->Range, fmt::format("Cannot implicitly convert from '{}' to '{}'", TypeInfoToString(RHS->Type), TypeInfoToString(LHS->Type)));
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
                        m_Context->ReportCompilerDiagnostic(LHS->Loc, LHS->Range, fmt::format("Cannot implicitly convert from '{}' to 'bool'", TypeInfoToString(LHS->Type)));
                    }
                }

                if (costRHS.CastNeeded) {
                    if (costRHS.ImplicitCastPossible) {
                        InsertImplicitCast(boolType, RHS->Type, RHS, costRHS.CaKind);
                    } else {
                        m_Context->ReportCompilerDiagnostic(LHS->Loc, LHS->Range, fmt::format("Cannot implicitly convert from '{}' to 'bool'", TypeInfoToString(RHS->Type)));
                    }
                }

                expr->Type = boolType;
                expr->ValueKind = ExprValueKind::RValue;

                if (expr->ResultDiscarded) {
                    m_Context->ReportCompilerDiagnostic(expr->Loc, expr->Range, "Discarding result of logical operator", CompilerDiagKind::Warning);
                }
                return;
            }

            default: ARIA_UNREACHABLE();
        }
    }

    void SemanticAnalyzer::ResolveCompoundAssignExpr(Expr* expr) {
        CompoundAssignExpr& compAss = expr->CompoundAssign;
        
        ResolveExpr(compAss.LHS);
        ResolveExpr(compAss.RHS);
        
        Expr* LHS = compAss.LHS;
        Expr* RHS = compAss.RHS;
        
        TypeInfo* LHSType = LHS->Type;
        TypeInfo* RHSType = RHS->Type;
        
        if (LHS->ValueKind != ExprValueKind::LValue) {
            m_Context->ReportCompilerDiagnostic(compAss.LHS->Loc, compAss.LHS->Range, "Expression must be a modifiable lvalue");
        }
        
        ConversionCost cost = GetConversionCost(LHSType, RHSType, compAss.RHS->ValueKind);
        
        if (cost.CastNeeded) {
            if (cost.ImplicitCastPossible) {
                InsertImplicitCast(LHSType, RHSType, RHS, cost.CaKind);
                RHSType = LHSType;
            } else {
                m_Context->ReportCompilerDiagnostic(compAss.RHS->Loc, compAss.RHS->Range, fmt::format("Cannot implicitly convert from '{}' to '{}'", TypeInfoToString(RHSType), TypeInfoToString(LHSType)));
            }
        }
        
        expr->Type = LHSType;
        expr->ValueKind = ExprValueKind::LValue;
    }

    void SemanticAnalyzer::ResolveExpr(Expr* expr) {
        if (expr->Kind == ExprKind::Error) { return; }
        else if (expr->Kind == ExprKind::BooleanConstant) {
            return ResolveBooleanConstantExpr(expr);
        } else if (expr->Kind == ExprKind::CharacterConstant) {
            return ResolveCharacterConstantExpr(expr);
        } else if (expr->Kind == ExprKind::IntegerConstant) {
            return ResolveIntegerConstantExpr(expr);
        } else if (expr->Kind == ExprKind::FloatingConstant) {
            return ResolveFloatingConstantExpr(expr);
        } else if (expr->Kind == ExprKind::StringConstant) {
            return ResolveStringConstantExpr(expr);
        } else if (expr->Kind == ExprKind::DeclRef) {
            return ResolveDeclRefExpr(expr);
        } else if (expr->Kind == ExprKind::Member) {
            return ResolveMemberExpr(expr);
        } else if (expr->Kind == ExprKind::Call) {
            return ResolveCallExpr(expr);
        } else if (expr->Kind == ExprKind::MethodCall) {
            return ResolveMethodCallExpr(expr);
        } else if (expr->Kind == ExprKind::Paren) {
            return ResolveParenExpr(expr);
        } else if (expr->Kind == ExprKind::Cast) {
            return ResolveCastExpr(expr);
        }else if (expr->Kind == ExprKind::UnaryOperator) {
            return ResolveUnaryOperatorExpr(expr);
        } else if (expr->Kind == ExprKind::BinaryOperator) {
            return ResolveBinaryOperatorExpr(expr);
        } else if (expr->Kind == ExprKind::CompoundAssign) {
            return ResolveCompoundAssignExpr(expr);
        }

        ARIA_UNREACHABLE();
    }

    void SemanticAnalyzer::ResolveTranslationUnitDecl(Decl* decl) {
        TranslationUnitDecl tu = decl->TranslationUnit;

        for (Stmt* stmt : tu.Stmts) {
            ResolveStmt(stmt);
        }
    }

    void SemanticAnalyzer::ResolveVarDecl(Decl* decl) {
        VarDecl& varDecl = decl->Var;
        std::string ident = fmt::format("{}", varDecl.Identifier);

        ResolveType(decl->Loc, decl->Range, varDecl.Type);

        if (varDecl.Type->IsVoid()) {
            m_Context->ReportCompilerDiagnostic(decl->Loc, decl->Range, "Cannot declare variable of 'void' type");
        }

        ResolveInitializer(varDecl.DefaultValue, varDecl.Type, false);

        if (m_Scopes.size() > 0) {
            if (m_Scopes.back().Declarations.contains(ident)) {
                m_Context->ReportCompilerDiagnostic(decl->Loc, decl->Range, fmt::format("Redeclaring symbol '{}'", ident));
            }

            m_Scopes.back().Declarations[ident] = { varDecl.Type, decl, DeclRefKind::LocalVar };
        }
    }

    void SemanticAnalyzer::ResolveParamDecl(Decl* decl) {
        ParamDecl& paramDecl = decl->Param;
        ResolveType(decl->Loc, decl->Range, paramDecl.Type);
        m_Scopes.back().Declarations[fmt::format("{}", paramDecl.Identifier)] = { paramDecl.Type, decl, DeclRefKind::ParamVar };
    }

    void SemanticAnalyzer::ResolveFunctionDecl(Decl* decl) {
        FunctionDecl fnDecl = decl->Function;

        std::string ident = fmt::format("{}", fnDecl.Identifier);
        ResolveType(decl->Loc, decl->Range, fnDecl.Type);
        
        if (fnDecl.Body) {
            m_CanReachEndOfFunction = true;
            m_ActiveReturnType = std::get<FunctionDeclaration>(fnDecl.Type->Data).ReturnType;
            PushScope();
            
            for (Decl* p : fnDecl.Parameters) {
                ResolveParamDecl(p);
            }
            
            if (fnDecl.Body) {
                ResolveBlockStmt(fnDecl.Body);
            }
            
            PopScope();
            m_ActiveReturnType = nullptr;

            if (m_CanReachEndOfFunction && !std::get<FunctionDeclaration>(fnDecl.Type->Data).ReturnType->IsVoid()) {
                m_Context->ReportCompilerDiagnostic(decl->Loc, decl->Range, "Control flow reaches end of function with a non void return type");
            }
        }
    }

    void SemanticAnalyzer::ResolveStructDecl(Decl* decl) {
        StructDecl& s = decl->Struct;
        std::string ident = fmt::format("{}", s.Identifier);

        StructDeclaration d;
        d.Identifier = s.Identifier;
        d.SourceDecl = decl;
        
        TypeInfo* structType = TypeInfo::Create(m_Context, PrimitiveType::Structure, false, d);

        for (Decl* field : s.Fields) {
            if (field->Kind == DeclKind::Field) {
                ResolveType(field->Loc, field->Range, field->Field.Type);
            }
        }

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

    void SemanticAnalyzer::ResolveDecl(Decl* decl) {
        if (decl->Kind == DeclKind::Error) { return; }
        else if (decl->Kind == DeclKind::TranslationUnit) {
            return ResolveTranslationUnitDecl(decl);
        } else if (decl->Kind == DeclKind::Module) {
            return;
        } else if (decl->Kind == DeclKind::Var) {
            return ResolveVarDecl(decl);
        } else if (decl->Kind == DeclKind::Param) {
            return ResolveParamDecl(decl);
        } else if (decl->Kind == DeclKind::Function) {
            return ResolveFunctionDecl(decl);
        } else if (decl->Kind == DeclKind::Struct) {
            return ResolveStructDecl(decl);
        }

        ARIA_UNREACHABLE();
    }

    void SemanticAnalyzer::ResolveImportStmt(Stmt* stmt) {
        // ImportStmt& imp = stmt->Import;
        // CompilationUnit* curr = m_Context->m_ActiveCompUnit;
        // 
        // curr->Globals.insert(curr->Globals.end(), imp.Module->Globals.begin(), imp.Module->Globals.end());
        // curr->Funcs.insert(curr->Funcs.end(), imp.Module->Funcs.begin(), imp.Module->Funcs.end());
        // curr->Structs.insert(curr->Structs.end(), imp.Module->Structs.begin(), imp.Module->Structs.end());
    }

    void SemanticAnalyzer::ResolveBlockStmt(Stmt* stmt) {
        BlockStmt block = stmt->Block;

        for (Stmt* s : block.Stmts) {
            ResolveStmt(s);
        }
    }

    void SemanticAnalyzer::ResolveWhileStmt(Stmt* stmt) {
        WhileStmt wh = stmt->While;

        PushScope(true, true);

        ResolveExpr(wh.Condition);
        RequireRValue(wh.Condition);

        if (!wh.Condition->Type->IsBoolean()) {
            ARIA_ASSERT(false, "todo: add error");
        }

        ResolveBlockStmt(wh.Body);

        PopScope();
    }

    void SemanticAnalyzer::ResolveDoWhileStmt(Stmt* stmt) {
        DoWhileStmt wh = stmt->DoWhile;

        PushScope(true, true);

        ResolveExpr(wh.Condition);
        RequireRValue(wh.Condition);

        if (!wh.Condition->Type->IsBoolean()) {
            ARIA_ASSERT(false, "todo: add error");
        }

        ResolveBlockStmt(wh.Body);

        PopScope();
    }

    void SemanticAnalyzer::ResolveForStmt(Stmt* stmt) {
        ForStmt fs = stmt->For;

        PushScope(true, true);
        
        if (fs.Prologue) { ResolveDecl(fs.Prologue); }

        if (fs.Condition) {
            ResolveExpr(fs.Condition);
            RequireRValue(fs.Condition);

            if (!fs.Condition->Type->IsBoolean() && !fs.Condition->Type->IsError()) {
                m_Context->ReportCompilerDiagnostic(fs.Condition->Loc, fs.Condition->Range, fmt::format("For loop condition must be of a boolean type but is '{}'", TypeInfoToString(fs.Condition->Type)));
            }
        }

        if (fs.Step) { ResolveExpr(fs.Step); }
        ResolveBlockStmt(fs.Body);
        
        PopScope();
    }

    void SemanticAnalyzer::ResolveIfStmt(Stmt* stmt) {
        IfStmt ifs = stmt->If;
        PushScope();

        ResolveExpr(ifs.Condition);
        RequireRValue(ifs.Condition);

        if (!ifs.Condition->Type->IsBoolean()) {
            ARIA_ASSERT(false, "todo: add error");
        }

        ResolveBlockStmt(ifs.Body);

        PopScope();
    }

    void SemanticAnalyzer::ResolveBreakStmt(Stmt* stmt) {
        if (!m_Scopes.back().AllowBreakStmt) {
            m_Context->ReportCompilerDiagnostic(stmt->Loc, stmt->Range, "Cannot use 'break' here");
        }
    }

    void SemanticAnalyzer::ResolveContinueStmt(Stmt* stmt) {
        if (!m_Scopes.back().AllowContinueStmt) {
            m_Context->ReportCompilerDiagnostic(stmt->Loc, stmt->Range, "Cannot use 'continue' here");
        }
    }

    void SemanticAnalyzer::ResolveReturnStmt(Stmt* stmt) {
        ReturnStmt& ret = stmt->Return;
        
        if (m_ActiveReturnType == nullptr) {
            m_Context->ReportCompilerDiagnostic(stmt->Loc, stmt->Range, "'return' statement out of function body is not allowed");
            ret.Value->Type = &ErrorType;
            return;
        }
        
        ResolveInitializer(ret.Value, m_ActiveReturnType, false);

        if (m_Scopes.size() == 1) {
            m_CanReachEndOfFunction = false;
        }
    }

    void SemanticAnalyzer::ResolveStmt(Stmt* stmt) {
        if (stmt->Kind == StmtKind::Error) { return; }
        else if (stmt->Kind == StmtKind::Nop) { return; }
        else if (stmt->Kind == StmtKind::Import) {
            return ResolveImportStmt(stmt);
        } else if (stmt->Kind == StmtKind::Block) {
            return ResolveBlockStmt(stmt);
        } else if (stmt->Kind == StmtKind::While) {
            return ResolveWhileStmt(stmt);
        } else if (stmt->Kind == StmtKind::DoWhile) {
            return ResolveDoWhileStmt(stmt);
        } else if (stmt->Kind == StmtKind::For) {
            return ResolveForStmt(stmt);
        } else if (stmt->Kind == StmtKind::If) {
            return ResolveIfStmt(stmt);
        } else if (stmt->Kind == StmtKind::Break) {
            return ResolveBreakStmt(stmt);
        } else if (stmt->Kind == StmtKind::Continue) {
            return ResolveContinueStmt(stmt);
        } else if (stmt->Kind == StmtKind::Return) {
            return ResolveReturnStmt(stmt);
        } else if (stmt->Kind == StmtKind::Expr) {
            return ResolveExpr(stmt->ExprStmt);
        } else if (stmt->Kind == StmtKind::Decl) {
            return ResolveDecl(stmt->DeclStmt);
        }

        ARIA_UNREACHABLE();
    }

    void SemanticAnalyzer::ResolveType(SourceLocation loc, SourceRange range, TypeInfo* type) {
        if (type->Type == PrimitiveType::Unresolved) {
            StringView ident = std::get<UnresolvedType>(type->Data).Identifier;
            Decl* d = FindSymbolInUnit(m_Context->ActiveCompUnit, ident);

            if (!d) {
                m_Context->ReportCompilerDiagnostic(loc, range, fmt::format("Could not find type '{}')", ident));
                return;
            }

            type->Type = PrimitiveType::Structure;
            type->Data = StructDeclaration(ident, d);
        } else if (type->Type == PrimitiveType::Function) {
            FunctionDeclaration& fn = std::get<FunctionDeclaration>(type->Data);

            for (TypeInfo* param : fn.ParamTypes) {
                ResolveType(loc, range, param);
            }

            ResolveType(loc, range, fn.ReturnType);
        }
    }

    void SemanticAnalyzer::ResolveInitializer(Expr* initializer, TypeInfo* type, bool temporary) {
        // If we are initializing a reference, the initializer must be of the same type and an lvalue
        if (type->IsReference()) {
            ARIA_ASSERT(initializer != nullptr, "initial value of a reference must be an lvalue");
            ResolveExpr(initializer);
            TypeInfo* initType = initializer->Type;

            if (!TypeIsEqual(type, initType) || initializer->ValueKind != ExprValueKind::LValue) {
                m_Context->ReportCompilerDiagnostic(initializer->Loc, initializer->Range, "Initial value of reference must be an lvalue");
            }
        } else if (initializer) {
            ResolveExpr(initializer);
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
                    m_Context->ReportCompilerDiagnostic(initializer->Loc, initializer->Range, fmt::format("Cannot implicitly convert from '{}' to '{}'", TypeInfoToString(initType), TypeInfoToString(type)));
                }
            }

            if (temporary) {
                if (initializer->Type->Type == PrimitiveType::String) {
                    ReplaceExpr(initializer, Expr::Create(m_Context, initializer->Loc, initializer->Range, 
                        ExprKind::Temporary, ExprValueKind::RValue, initializer->Type, 
                        TemporaryExpr(Expr::Dup(m_Context, initializer), m_BuiltInStringDestructor)));
                }
            }
        }
    }

    void SemanticAnalyzer::PushScope(bool allowBreak, bool allowContinue) {
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

    void SemanticAnalyzer::PopScope() {
        m_Scopes.pop_back();
    }

    ConversionCost SemanticAnalyzer::GetConversionCost(TypeInfo* dst, TypeInfo* src, ExprValueKind srcType) {
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

    void SemanticAnalyzer::InsertImplicitCast(TypeInfo* dstType, TypeInfo* srcType, Expr* srcExpr, CastKind castKind) {
        Expr* src = Expr::Dup(m_Context, srcExpr); // We must copy the original expression to avoid overwriting the same memory
        Expr* implicitCast = Expr::Create(m_Context, src->Loc, src->Range, ExprKind::ImplicitCast, ExprValueKind::RValue, dstType, ImplicitCastExpr(src, castKind));

        ReplaceExpr(srcExpr, implicitCast);
    }

    void SemanticAnalyzer::RequireRValue(Expr* expr) {
        if (expr->ValueKind == ExprValueKind::LValue) {
            InsertImplicitCast(expr->Type, expr->Type, expr, CastKind::LValueToRValue);
        }
    }

    void SemanticAnalyzer::InsertArithmeticPromotion(Expr* lhs, Expr* rhs) {
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
                m_Context->ReportCompilerDiagnostic(lhs->Loc, SourceRange(lhs->Range.Start, rhs->Range.End), fmt::format("Mismatched types '{}' and '{}' (implicit signedness conversions are not allowed here)", TypeInfoToString(lhsType), TypeInfoToString(rhsType)));
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

    void SemanticAnalyzer::ReplaceExpr(Expr* src, Expr* newExpr) {
        *src = *newExpr;
    }

    void SemanticAnalyzer::ReplaceDecl(Decl* src, Decl* newDecl) {
        *src = *newDecl;
    }

    bool SemanticAnalyzer::TypeIsEqual(TypeInfo* lhs, TypeInfo* rhs) {
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

    size_t SemanticAnalyzer::TypeGetSize(TypeInfo* t) {
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

            default: ARIA_ASSERT(false, "SemanticAnalyzer::TypeGetSize() only supports trivial (non structure) types");
        }
    }

} // namespace Aria::Internal
