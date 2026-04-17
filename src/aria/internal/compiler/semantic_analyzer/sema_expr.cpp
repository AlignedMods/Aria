#include "aria/internal/compiler/semantic_analyzer/semantic_analyzer.hpp"

namespace Aria::Internal {

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
                    ResolveInitializer(&call.Arguments.Items[i], fnDecl.ParamTypes.Items[i], true);
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

} // namespace Aria::Internal