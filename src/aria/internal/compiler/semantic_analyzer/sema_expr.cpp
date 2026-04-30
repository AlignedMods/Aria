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

    void SemanticAnalyzer::ResolveNullExpr(Expr* expr) {
        if (expr->ResultDiscarded) {
            m_Context->ReportCompilerDiagnostic(expr->Loc, expr->Range, "Discarding result of null", CompilerDiagKind::Warning);
        }
    }

    void SemanticAnalyzer::ResolveDeclRefExpr(Expr* expr) {
        DeclRefExpr& ref = expr->DeclRef;
        std::string ident = fmt::format("{}", ref.Identifier);

        auto getType = [](Decl* d) -> TypeInfo* {
            switch (d->Kind) {
                case DeclKind::Var: { return d->Var.Type; }
                case DeclKind::Param: { return d->Param.Type; }
                case DeclKind::Function: { return d->Function.Type; }
                case DeclKind::OverloadedFunction: { return &ErrorType; }
                case DeclKind::Struct: { return &ErrorType; }

                default: ARIA_UNREACHABLE();
            }      
        };

        if (expr->ResultDiscarded) {
            m_Context->ReportCompilerDiagnostic(expr->Loc, expr->Range, "Discarding result of identifier", CompilerDiagKind::Warning);
        }

        Module* mod = nullptr;

        if (!ref.NameSpecifier) {
            for (auto& scope : m_Scopes) {
                if (scope.Declarations.contains(ident)) {
                    Decl* d = scope.Declarations.at(ident).SourceDeclaration;
                    expr->Type = getType(d);
                    ref.ReferencedDecl = d;
                    return;
                }
            }
        }

        if (ref.NameSpecifier) {
            ARIA_ASSERT(ref.NameSpecifier->Kind == SpecifierKind::Scope, "Invalid specifier");

            // We may be referencing ourselves
            if (ref.NameSpecifier->Scope.Identifier == m_Context->ActiveCompUnit->Parent->Name) {
                mod = m_Context->ActiveCompUnit->Parent;
            }

            for (Stmt* import : m_Context->ActiveCompUnit->Imports) {
                ARIA_ASSERT(import->Kind == StmtKind::Import, "Invalid import stmt");
                if (import->Import.Name == ref.NameSpecifier->Scope.Identifier) {
                    mod = import->Import.ResolvedModule;
                }
            }

            if (!mod) {
                m_Context->ReportCompilerDiagnostic(ref.NameSpecifier->Loc, ref.NameSpecifier->Range, fmt::format("Could not find module '{}'", ref.NameSpecifier->Scope.Identifier));
                expr->Type = &ErrorType;
                ref.ReferencedDecl = &g_ErrorDecl;
                return;
            }
        } else {
            mod = m_Context->ActiveCompUnit->Parent;
        }

        if (m_Context->ActiveCompUnit->LocalSymbols.contains(ident)) {
            Decl* d = m_Context->ActiveCompUnit->LocalSymbols.at(ident);
            ref.ReferencedDecl = d;
            expr->Type = getType(d);
            return;
        }

        if (mod->Symbols.contains(ident)) {
            Decl* d = mod->Symbols.at(ident);

            // if (d->Visibility == DeclVisibility::Private && ref.NameSpecifier) {
            //     m_Context->ReportCompilerDiagnostic(ref.NameSpecifier->Loc, ref.NameSpecifier->Range, fmt::format("Symbol '{}' is not accessible", ref.Identifier));
            //     expr->Type = &ErrorType;
            //     ref.ReferencedDecl = &g_ErrorDecl;
            //     return;
            // }

            ref.ReferencedDecl = d;
            expr->Type = getType(d);
            return;
        }

        for (Stmt* import : m_Context->ActiveCompUnit->Imports) {
            ARIA_ASSERT(import->Kind == StmtKind::Import, "Invalid import");

            if (import->Import.ResolvedModule->Symbols.contains(ident)) {
                m_Context->ReportCompilerDiagnostic(expr->Loc, expr->Range, fmt::format("Symbols from other modules must be prefixed with the module name ({}::{})", import->Import.Name, ref.Identifier));
                expr->Type = &ErrorType;
                ref.ReferencedDecl = &g_ErrorDecl;
                return;
            }
        }

        expr->Type = &ErrorType;
        ref.ReferencedDecl = &g_ErrorDecl;
        m_Context->ReportCompilerDiagnostic(expr->Loc, expr->Range, fmt::format("Unknown identifier '{}'", ref.Identifier));
    }

    void SemanticAnalyzer::ResolveMemberExpr(Expr* expr) {
        MemberExpr& mem = expr->Member;

        ResolveExpr(mem.Parent);
        TypeInfo* parentType = mem.Parent->Type;
        TypeInfo* memberType = nullptr;

        switch (parentType->Kind) {
            case TypeKind::Structure: {
                StructDeclaration& sd = parentType->Struct;

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

                break;
            }

            case TypeKind::Slice: {
                if (mem.Member == "mem") {
                    memberType = TypeInfo::Create(m_Context, TypeKind::Ptr, false);
                    memberType->Base = parentType->Base;
                    expr->Kind = ExprKind::BuiltinMember;
                } else if (mem.Member == "len") {
                    memberType = &ULongType;
                    expr->Kind = ExprKind::BuiltinMember;
                }

                break;
            }

            default: {
                m_Context->ReportCompilerDiagnostic(mem.Parent->Loc, mem.Parent->Range, fmt::format("Expression must be of slice or struct type but is '{}'", TypeInfoToString(parentType)));
                expr->Type = &ErrorType;
                return;
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

    void SemanticAnalyzer::ResolveBuiltinMemberExpr(Expr* expr) { ARIA_UNREACHABLE(); }
    void SemanticAnalyzer::ResolveTemporaryExpr(Expr* expr) { ARIA_UNREACHABLE(); }
    void SemanticAnalyzer::ResolveCopyExpr(Expr* expr) { ARIA_UNREACHABLE(); }

    void SemanticAnalyzer::ResolveCallExpr(Expr* expr) {
        CallExpr& call = expr->Call;

        ResolveExpr(call.Callee);
        TypeInfo* calleeType = call.Callee->Type;

        if (calleeType->Kind != TypeKind::Function && !calleeType->IsError()) {
            m_Context->ReportCompilerDiagnostic(expr->Loc, expr->Range, "Cannot call an object of non-function type");
            expr->Type = &ErrorType;
            return;
        }

        if (call.Callee->DeclRef.ReferencedDecl->Kind != DeclKind::Error) {
            if (call.Callee->Kind == ExprKind::DeclRef && call.Callee->DeclRef.ReferencedDecl->Kind == DeclKind::OverloadedFunction) { // Overloaded function
                ARIA_TODO("Overloaded function calls");
            } else if (!call.Callee->Type->IsError()) { // Normal function
                FunctionDeclaration& fnDecl = calleeType->Function;

                for (auto& attr : call.Callee->DeclRef.ReferencedDecl->Function.Attributes) {
                    if (attr.Kind == FunctionDecl::AttributeKind::Unsafe && !m_UnsafeContext) {
                        m_Context->ReportCompilerDiagnostic(expr->Loc, expr->Range, "Cannot call unsafe function in safe context");
                    }
                }

                if (fnDecl.ParamTypes.Size != call.Arguments.Size) {
                    m_Context->ReportCompilerDiagnostic(expr->Loc, expr->Range, fmt::format("Mismatched argument count, function expects {} but got {}", fnDecl.ParamTypes.Size, call.Arguments.Size));
                    for (size_t i = 0; i < call.Arguments.Size; i++) {
                        call.Arguments.Items[i]->Type = &ErrorType;
                    }
                } else {
                    for (size_t i = 0; i < fnDecl.ParamTypes.Size; i++) {
                        ResolveParamInitializer(fnDecl.ParamTypes.Items[i], call.Arguments.Items[i]);
                    }
                }

                expr->Type = fnDecl.ReturnType;
                expr->ValueKind = (fnDecl.ReturnType->IsReference()) ? ExprValueKind::LValue : ExprValueKind::RValue;

                // We may need to create a temporary if a function returns a non-trivial type and it is discarded
                if (expr->ResultDiscarded && !fnDecl.ReturnType->IsReference()) {
                    if (fnDecl.ReturnType->IsString()) {
                        ReplaceExpr(expr, Expr::Create(m_Context, expr->Loc, expr->Range, ExprKind::Temporary,
                            ExprValueKind::RValue, expr->Type,
                            TemporaryExpr(Expr::Dup(m_Context, expr), m_BuiltInStringDestructor)));
                    }
                }

                return;
            }
        } else {
            for (Expr* arg : call.Arguments) {
                ResolveExpr(arg);
            }
        }

        expr->Type = &ErrorType;
    }

    void SemanticAnalyzer::ResolveConstructExpr(Expr* expr) { ARIA_UNREACHABLE(); }

    void SemanticAnalyzer::ResolveMethodCallExpr(Expr* expr) {
        ARIA_ASSERT(false, "todo!");
    }

    void SemanticAnalyzer::ResolveArraySubscriptExpr(Expr* expr) {
        ArraySubscriptExpr& subs = expr->ArraySubscript;

        ResolveExpr(subs.Array);
        ResolveExpr(subs.Index);
        RequireRValue(subs.Index);

        if (subs.Array->Type->IsError()) { expr->Type = &ErrorType; return; }

        switch (subs.Array->Type->Kind) {
            case TypeKind::Ptr: {
                RequireRValue(subs.Array);
                expr->Type = subs.Array->Type->Base;
                break;
            }

            case TypeKind::Slice: {
                expr->Type = subs.Array->Type->Base;
                break;
            }

            case TypeKind::Array: {
                expr->Type = subs.Array->Type->Array.Type;
                break;
            }

            default: m_Context->ReportCompilerDiagnostic(subs.Array->Loc, subs.Array->Range, "'[' operator can only be used with a pointer/slice/array"); break;
        }

        ConversionCost cost = GetConversionCost(&ULongType, subs.Index->Type);
        if (cost.CastNeeded) {
            if (cost.ImplicitCastPossible) {
                InsertImplicitCast(&ULongType, subs.Index->Type, subs.Index, cost.Kind);
            } else {
                m_Context->ReportCompilerDiagnostic(subs.Index->Loc, subs.Index->Range, fmt::format("Array index cannot be implicitly converted from '{}' to 'ulong'", TypeInfoToString(subs.Index->Type)));
            }
        }
    }

    void SemanticAnalyzer::ResolveToSliceExpr(Expr* expr) {
        ToSliceExpr& tos = expr->ToSlice;

        ResolveExpr(tos.Source);
        ResolveExpr(tos.Len);
        RequireRValue(tos.Len);

        if (tos.Source->Type->IsError()) { expr->Type = &ErrorType; return; }

        switch (tos.Source->Type->Kind) {
            case TypeKind::Ptr: {
                RequireRValue(tos.Source);
                expr->Type = TypeInfo::Create(m_Context, TypeKind::Slice, false);
                expr->Type->Base = tos.Source->Type->Base;
                break;
            }

            case TypeKind::Slice: {
                ARIA_TODO("slice to slice");
                // RequireRValue(subs.Array);
                // expr->Type = subs.Array->Type->Base;
                // break;
            }

            case TypeKind::Array: {
                ARIA_TODO("array to slice");
                // expr->Type = subs.Array->Type->Array.Type;
                // break;
            }

            default: m_Context->ReportCompilerDiagnostic(tos.Source->Loc, tos.Source->Range, "Only a pointer/slice/array can be converted to a slice"); break;
        }

        ConversionCost cost = GetConversionCost(&ULongType, tos.Len->Type);
        if (cost.CastNeeded) {
            if (cost.ImplicitCastPossible) {
                InsertImplicitCast(&ULongType, tos.Len->Type, tos.Len, cost.Kind);
            } else {
                m_Context->ReportCompilerDiagnostic(tos.Len->Loc, tos.Len->Range, fmt::format("Slice length cannot be implicitly converted from '{}' to 'ulong'", TypeInfoToString(tos.Len->Type)));
            }
        }
    }

    void SemanticAnalyzer::ResolveNewExpr(Expr* expr) {
        NewExpr& n = expr->New;
        
        if (expr->ResultDiscarded) {
            m_Context->ReportCompilerDiagnostic(expr->Loc, expr->Range, "Discarding result of 'new' expression is not allowed");
        }

        if (!m_UnsafeContext) {
            m_Context->ReportCompilerDiagnostic(expr->Loc, expr->Range, "'new' is only allowed in unsafe context");
        }

        if (!n.Initializer) {
            CreateDefaultInitializer(&n.Initializer, expr->Type->Base, expr->Loc, expr->Range);
        } else {
            if (n.Array) {
                m_TemporaryContext = true;
                ResolveExpr(n.Initializer);
                RequireRValue(n.Initializer);
                m_TemporaryContext = false;

                ConversionCost cost = GetConversionCost(&ULongType, n.Initializer->Type);
                if (cost.CastNeeded) {
                    if (cost.ImplicitCastPossible) {
                        InsertImplicitCast(&ULongType, n.Initializer->Type, n.Initializer, cost.Kind);
                    } else {
                        m_Context->ReportCompilerDiagnostic(n.Initializer->Loc, n.Initializer->Range, fmt::format("Cannot implicitly convert from '{}' to '{}'", TypeInfoToString(n.Initializer->Type), TypeInfoToString(&ULongType)));
                    }
                }
            } else {
                m_TemporaryContext = true;
                ResolveExpr(n.Initializer);
                RequireRValue(n.Initializer);
                m_TemporaryContext = false;

                ConversionCost cost = GetConversionCost(expr->Type->Base, n.Initializer->Type);
                if (cost.CastNeeded) {
                    if (cost.ImplicitCastPossible) {
                        InsertImplicitCast(expr->Type->Base, n.Initializer->Type, n.Initializer, cost.Kind);
                    } else {
                        m_Context->ReportCompilerDiagnostic(n.Initializer->Loc, n.Initializer->Range, fmt::format("Cannot implicitly convert from '{}' to '{}'", TypeInfoToString(n.Initializer->Type), TypeInfoToString(expr->Type->Base)));
                    }
                }
            }
        }
    }

    void SemanticAnalyzer::ResolveDeleteExpr(Expr* expr) {
        DeleteExpr& d = expr->Delete;

        if (!m_UnsafeContext) {
            m_Context->ReportCompilerDiagnostic(expr->Loc, expr->Range, "'delete' is only allowed in unsafe context");
        }

        ResolveExpr(d.Expression);
        RequireRValue(d.Expression);

        if (!d.Expression->Type->IsPointer()) {
            m_Context->ReportCompilerDiagnostic(expr->Loc, expr->Range, "'delete' can only be used with pointer types");
        }
    }

    void SemanticAnalyzer::ResolveFormatExpr(Expr* expr) {
        FormatExpr& format = expr->Format;

        if (format.Args.Size == 0) {
            m_Context->ReportCompilerDiagnostic(expr->Loc, expr->Range, "Format expression must have a format string");
            return;
        }

        if (format.Args.Items[0]->Kind != ExprKind::StringConstant) {
            m_Context->ReportCompilerDiagnostic(format.Args.Items[0]->Loc, format.Args.Items[0]->Range, "Format string must be a string literal");
            return;
        }

        if (expr->ResultDiscarded) {
            m_Context->ReportCompilerDiagnostic(expr->Loc, expr->Range, "Discarding result of format is not allowed");
            return;
        }

        for (Expr* arg : format.Args) {
            m_TemporaryContext = true;
            ResolveExpr(arg);

            RequireRValue(arg);
            m_TemporaryContext = false;
        }

        TinyVector<FormatExpr::FormatArg> formattedArgs;
        StringView fmtStr = format.Args.Items[0]->Temporary.Expression->StringConstant.Value;
        StringBuilder buf;
        size_t idx = 0;
        bool needsClosing = false;

        for (size_t i = 0; i < fmtStr.Size(); i++) {
            if (needsClosing) {
                if (fmtStr.At(i) == '}') {
                    needsClosing = false;
                    continue;
                } else {
                    m_Context->ReportCompilerDiagnostic(expr->Loc, expr->Range, "Missing closing curly brace in format string");
                    return;
                }
            }

            if (fmtStr.At(i) == '{') {
                if (buf.Size() > 0) {
                    StringBuilder tmpB;
                    tmpB.Append(m_Context, buf);
                    buf.Clear();

                    Expr* str = Expr::Create(m_Context, expr->Loc, expr->Range, ExprKind::StringConstant,
                        ExprValueKind::RValue, &StringType,
                        StringConstantExpr(tmpB));

                    formattedArgs.Append(m_Context, { Expr::Create(m_Context, expr->Loc, expr->Range, ExprKind::Temporary,
                        ExprValueKind::RValue, &StringType,
                        TemporaryExpr(str, m_BuiltInStringDestructor)) });
                }
                
                if (idx + 1 >= format.Args.Size) {
                    m_Context->ReportCompilerDiagnostic(expr->Loc, expr->Range, "Format string specifies more arguments than are provided");
                    return;
                }

                formattedArgs.Append(m_Context, { format.Args.Items[idx + 1] });
                needsClosing = true;
                idx++;
            } else {
                buf.Append(m_Context, fmtStr.At(i));
            }
        }

        if (buf.Size() > 0) {
            StringBuilder tmpB;
            tmpB.Append(m_Context, buf);
            buf.Clear();

            Expr* str = Expr::Create(m_Context, expr->Loc, expr->Range, ExprKind::StringConstant,
                ExprValueKind::RValue, &StringType,
                StringConstantExpr(tmpB));

            formattedArgs.Append(m_Context, { Expr::Create(m_Context, expr->Loc, expr->Range, ExprKind::Temporary,
                ExprValueKind::RValue, &StringType,
                TemporaryExpr(str, m_BuiltInStringDestructor)) });
        }

        format.ResolvedArgs = formattedArgs;
        if (m_TemporaryContext) {
            ReplaceExpr(expr, Expr::Create(m_Context, expr->Loc, expr->Range, ExprKind::Temporary,
                ExprValueKind::RValue, expr->Type,
                TemporaryExpr(Expr::Dup(m_Context, expr), m_BuiltInStringDestructor)));
        }
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
        RequireRValue(cast.Expression);
        expr->Type = cast.Type;

        TypeInfo* srcType = cast.Expression->Type;
        TypeInfo* dstType = cast.Type;

        ConversionCost cost = GetConversionCost(dstType, srcType);

        if (cost.CastNeeded) {
            if (cost.ExplicitCastPossible) {
                InsertImplicitCast(dstType, srcType, cast.Expression, cost.Kind);
            } else {
                m_Context->ReportCompilerDiagnostic(expr->Loc, expr->Range, fmt::format("Cannot cast '{}' to '{}'", TypeInfoToString(srcType), TypeInfoToString(dstType)));
            }
        }

        if (expr->ResultDiscarded) {
            m_Context->ReportCompilerDiagnostic(expr->Loc, expr->Range, "Discarding result of explicit cast", CompilerDiagKind::Warning);
        }
    }

    void SemanticAnalyzer::ResolveImplicitCastExpr(Expr* expr) { ARIA_UNREACHABLE(); }

    void SemanticAnalyzer::ResolveUnaryOperatorExpr(Expr* expr) {
        UnaryOperatorExpr& unop = expr->UnaryOperator;
        
        ResolveExpr(unop.Expression);
        TypeInfo* type = unop.Expression->Type;
        
        switch (unop.Operator) {
            case UnaryOperatorKind::Negate: {
                RequireRValue(unop.Expression);
                ARIA_ASSERT(type->IsNumeric(), "todo: add error message");

                if (type->IsIntegral()) {
                    if (type->IsUnsigned()) {
                        m_Context->ReportCompilerDiagnostic(expr->Loc, expr->Range, fmt::format("Cannot negate expression of unsigned type '{}'", TypeInfoToString(type)));
                    }
                }

                expr->Type = type;
                break;
            }

            case UnaryOperatorKind::AddressOf: {
                if (type->IsError()) { expr->Type = type; break; }

                if (!m_UnsafeContext) {
                    m_Context->ReportCompilerDiagnostic(expr->Loc, expr->Range, "Address of operation ('&') must be in an unsafe context");
                }

                if (unop.Expression->ValueKind != ExprValueKind::LValue) {
                    m_Context->ReportCompilerDiagnostic(expr->Loc, expr->Range, "Address of operation ('&') requries an lvalue");
                }

                TypeInfo* newType = TypeInfo::Create(m_Context, TypeKind::Ptr, false);
                newType->Base = type;
                expr->Type = newType;
                break;
            }

            case UnaryOperatorKind::Dereference: {
                if (type->IsError()) { expr->Type = type; break; }

                if (!m_UnsafeContext) {
                    m_Context->ReportCompilerDiagnostic(expr->Loc, expr->Range, "Dereferencing of pointer must be in an unsafe context");
                }

                if (unop.Expression->ValueKind != ExprValueKind::LValue) {
                    m_Context->ReportCompilerDiagnostic(expr->Loc, expr->Range, "Dereferencing requires an lvalue");
                }

                RequireRValue(unop.Expression);

                if (type->IsPointer()) {
                    if (type->Base->IsVoid()) {
                        m_Context->ReportCompilerDiagnostic(expr->Loc, expr->Range, "Cannot dereference a void*");
                    }
                } else {
                    m_Context->ReportCompilerDiagnostic(expr->Loc, expr->Range, "Dereferencing requires a pointer type");
                    break;
                }

                expr->Type = type->Base;
                expr->ValueKind = ExprValueKind::LValue;
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

                RequireRValue(RHS);

                ConversionCost cost = GetConversionCost(LHS->Type, RHS->Type);

                if (cost.CastNeeded) {
                    if (cost.ImplicitCastPossible) {
                        InsertImplicitCast(LHS->Type, RHS->Type, RHS, cost.Kind);
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
                TypeInfo* boolType = TypeInfo::Create(m_Context, TypeKind::Bool, false);

                RequireRValue(LHS);
                RequireRValue(RHS);

                ConversionCost costLHS = GetConversionCost(boolType, LHS->Type);
                ConversionCost costRHS = GetConversionCost(boolType, RHS->Type);

                if (costLHS.CastNeeded) {
                    if (costLHS.ImplicitCastPossible) {
                        InsertImplicitCast(boolType, LHS->Type, LHS, costLHS.Kind);
                    } else {
                        m_Context->ReportCompilerDiagnostic(LHS->Loc, LHS->Range, fmt::format("Cannot implicitly convert from '{}' to 'bool'", TypeInfoToString(LHS->Type)));
                    }
                }

                if (costRHS.CastNeeded) {
                    if (costRHS.ImplicitCastPossible) {
                        InsertImplicitCast(boolType, RHS->Type, RHS, costRHS.Kind);
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

        RequireRValue(compAss.RHS);
        
        Expr* LHS = compAss.LHS;
        Expr* RHS = compAss.RHS;
        
        TypeInfo* LHSType = LHS->Type;
        TypeInfo* RHSType = RHS->Type;
        
        if (LHS->ValueKind != ExprValueKind::LValue) {
            m_Context->ReportCompilerDiagnostic(compAss.LHS->Loc, compAss.LHS->Range, "Expression must be a modifiable lvalue");
        }
        
        ConversionCost cost = GetConversionCost(LHSType, RHSType);
        
        if (cost.CastNeeded) {
            if (cost.ImplicitCastPossible) {
                InsertImplicitCast(LHSType, RHSType, RHS, cost.Kind);
                RHSType = LHSType;
            } else {
                m_Context->ReportCompilerDiagnostic(compAss.RHS->Loc, compAss.RHS->Range, fmt::format("Cannot implicitly convert from '{}' to '{}'", TypeInfoToString(RHSType), TypeInfoToString(LHSType)));
            }
        }
        
        expr->Type = LHSType;
        expr->ValueKind = ExprValueKind::LValue;
    }

    void SemanticAnalyzer::ResolveExpr(Expr* expr) {
        #define EXPR_CASE(kind) Resolve##kind##Expr(expr)
        #include "aria/internal/compiler/ast/expr_switch.hpp"
        #undef EXPR_CASE
    }

    void SemanticAnalyzer::InsertImplicitCast(TypeInfo* dstType, TypeInfo* srcType, Expr* srcExpr, CastKind castKind) {
        Expr* src = Expr::Dup(m_Context, srcExpr); // We must copy the original expression to avoid overwriting the same memory
        Expr* implicitCast = Expr::Create(m_Context, src->Loc, src->Range, ExprKind::ImplicitCast, ExprValueKind::RValue, dstType, ImplicitCastExpr(src, castKind));

        ReplaceExpr(srcExpr, implicitCast);
    }

    void SemanticAnalyzer::RequireRValue(Expr* expr) {
        if (expr->ValueKind == ExprValueKind::LValue) {
            if (expr->Type->Kind == TypeKind::String) {
                ReplaceExpr(expr, Expr::Create(m_Context, expr->Loc, expr->Range, 
                    ExprKind::Copy, ExprValueKind::RValue, expr->Type, 
                    CopyExpr(Expr::Dup(m_Context, expr), m_BuiltInStringCopyConstructor)));

                if (m_TemporaryContext) {
                    ReplaceExpr(expr, Expr::Create(m_Context, expr->Loc, expr->Range,
                        ExprKind::Temporary, ExprValueKind::RValue, expr->Type,
                        TemporaryExpr(Expr::Dup(m_Context, expr), m_BuiltInStringDestructor)));
                }
            } else {
                InsertImplicitCast(expr->Type, expr->Type, expr, CastKind::LValueToRValue);
            }
        }
    }

    void SemanticAnalyzer::InsertArithmeticPromotion(Expr* lhs, Expr* rhs) {
        TypeInfo* lhsType = lhs->Type;
        TypeInfo* rhsType = rhs->Type;

        if (lhsType->Kind == TypeKind::Error || rhsType->Kind == TypeKind::Error) {
            return;
        }

        RequireRValue(lhs);
        RequireRValue(rhs);

        if (TypeIsEqual(lhsType, rhsType)) {
            return;
        }

        if (lhsType->IsIntegral() && rhsType->IsIntegral()) {
            size_t lSize = TypeGetSize(lhsType);
            size_t rSize = TypeGetSize(rhsType);

            if (lSize > rSize) {
                InsertImplicitCast(lhsType, rhsType, rhs, CastKind::Integral);
            } else if (rSize > lSize) {
                InsertImplicitCast(rhsType, lhsType, lhs, CastKind::Integral);
            } else if (lSize == rSize) {
                // We know that the types are not equal so we likely have a signed/unsigned mismatch
                m_Context->ReportCompilerDiagnostic(lhs->Loc, SourceRange(lhs->Range.Start, rhs->Range.End), fmt::format("Mismatched types '{}' and '{}' (implicit signedness conversions are not allowed here)", TypeInfoToString(lhsType), TypeInfoToString(rhsType)));
            }

            return;
        }

        if (lhsType->IsIntegral() && rhsType->IsFloatingPoint()) {
            InsertImplicitCast(rhsType, lhsType, lhs, CastKind::IntegralToFloating);
            return;
        }

        if (lhsType->IsFloatingPoint() && rhsType->IsIntegral()) {
            InsertImplicitCast(lhsType, rhsType, rhs, CastKind::IntegralToFloating);
            return;
        }

        if (lhsType->IsFloatingPoint() && rhsType->IsFloatingPoint()) {
            size_t lSize = TypeGetSize(lhsType);
            size_t rSize = TypeGetSize(rhsType);

            if (lSize > rSize) {
                InsertImplicitCast(lhsType, rhsType, rhs, CastKind::Floating);
            } else if (rSize > lSize) {
                InsertImplicitCast(rhsType, lhsType, lhs, CastKind::Floating);
            }

            return;
        }

        ARIA_UNREACHABLE();
    }

} // namespace Aria::Internal