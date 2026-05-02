#include "aria/internal/compiler/semantic_analyzer/semantic_analyzer.hpp"
#include "aria/internal/compiler/core/scratch_buffer.hpp"

namespace Aria::Internal {

    void SemanticAnalyzer::resolve_BooleanConstant_expr(Expr* expr) {
        if (expr->ResultDiscarded) {
            m_Context->report_compiler_diagnostic(expr->Loc, expr->Range, "Discarding result of boolean literal", CompilerDiagKind::Warning);
        }
    }

    void SemanticAnalyzer::resolve_CharacterConstant_expr(Expr* expr) {
        if (expr->ResultDiscarded) {
            m_Context->report_compiler_diagnostic(expr->Loc, expr->Range, "Discarding result of character literal", CompilerDiagKind::Warning);
        }
    }

    void SemanticAnalyzer::resolve_IntegerConstant_expr(Expr* expr) {
        if (expr->ResultDiscarded) {
            m_Context->report_compiler_diagnostic(expr->Loc, expr->Range, "Discarding result of integer literal", CompilerDiagKind::Warning);
        }
    }

    void SemanticAnalyzer::resolve_FloatingConstant_expr(Expr* expr) {
        if (expr->ResultDiscarded) {
            m_Context->report_compiler_diagnostic(expr->Loc, expr->Range, "Discarding result of floating point literal", CompilerDiagKind::Warning);
        }
    }

    void SemanticAnalyzer::resolve_StringConstant_expr(Expr* expr) {
        if (expr->ResultDiscarded) {
            m_Context->report_compiler_diagnostic(expr->Loc, expr->Range, "Discarding result of string literal is not allowed");
        }
    }

    void SemanticAnalyzer::resolve_Null_expr(Expr* expr) {
        if (expr->ResultDiscarded) {
            m_Context->report_compiler_diagnostic(expr->Loc, expr->Range, "Discarding result of null", CompilerDiagKind::Warning);
        }
    }

    void SemanticAnalyzer::resolve_DeclRef_expr(Expr* expr) {
        DeclRefExpr& ref = expr->DeclRef;
        std::string ident = fmt::format("{}", ref.Identifier);

        auto getType = [](Decl* d) -> TypeInfo* {
            switch (d->Kind) {
                case DeclKind::Var: { return d->Var.Type; }
                case DeclKind::Param: { return d->Param.Type; }
                case DeclKind::Function: { return d->Function.Type; }
                case DeclKind::OverloadedFunction: { return &error_type; }
                case DeclKind::Struct: { return &error_type; }

                default: ARIA_UNREACHABLE();
            }      
        };

        if (expr->ResultDiscarded) {
            m_Context->report_compiler_diagnostic(expr->Loc, expr->Range, "Discarding result of identifier", CompilerDiagKind::Warning);
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
                m_Context->report_compiler_diagnostic(ref.NameSpecifier->Loc, ref.NameSpecifier->Range, fmt::format("Could not find module '{}'", ref.NameSpecifier->Scope.Identifier));
                expr->Type = &error_type;
                ref.ReferencedDecl = &error_decl;
                return;
            } else {
                ref.NameSpecifier->Scope.ReferencedModule = mod;
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

            ref.ReferencedDecl = d;
            expr->Type = getType(d);
            return;
        }

        for (Stmt* import : m_Context->ActiveCompUnit->Imports) {
            ARIA_ASSERT(import->Kind == StmtKind::Import, "Invalid import");

            if (import->Import.ResolvedModule->Symbols.contains(ident)) {
                m_Context->report_compiler_diagnostic(expr->Loc, expr->Range, fmt::format("Symbols from other modules must be prefixed with the module name ({}::{})", import->Import.Name, ref.Identifier));
                expr->Type = &error_type;
                ref.ReferencedDecl = &error_decl;
                return;
            }
        }

        expr->Type = &error_type;
        ref.ReferencedDecl = &error_decl;
        m_Context->report_compiler_diagnostic(expr->Loc, expr->Range, fmt::format("Unknown identifier '{}'", ref.Identifier));
    }

    void SemanticAnalyzer::resolve_Member_expr(Expr* expr) {
        MemberExpr& mem = expr->Member;

        resolve_expr(mem.Parent);
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
                    memberType = &ulong_type;
                    expr->Kind = ExprKind::BuiltinMember;
                }

                break;
            }

            default: {
                m_Context->report_compiler_diagnostic(mem.Parent->Loc, mem.Parent->Range, fmt::format("Expression must be of slice or struct type but is '{}'", type_info_to_string(parentType)));
                expr->Type = &error_type;
                return;
            }
        }

        if (!memberType) {
            m_Context->report_compiler_diagnostic(expr->Loc, expr->Range, fmt::format("Unknown member \"{}\" in '{}'", mem.Member, type_info_to_string(parentType)));
            expr->Type = &error_type;
            return;
        }

        expr->Type = memberType;

        if (expr->ResultDiscarded) {
            m_Context->report_compiler_diagnostic(expr->Loc, expr->Range, "Discarding result of member access", CompilerDiagKind::Warning);
        }
    }

    void SemanticAnalyzer::resolve_BuiltinMember_expr(Expr* expr) { ARIA_UNREACHABLE(); }
    void SemanticAnalyzer::resolve_Temporary_expr(Expr* expr) { ARIA_UNREACHABLE(); }
    void SemanticAnalyzer::resolve_Copy_expr(Expr* expr) { ARIA_UNREACHABLE(); }

    void SemanticAnalyzer::resolve_Call_expr(Expr* expr) {
        CallExpr& call = expr->Call;

        resolve_expr(call.Callee);
        TypeInfo* calleeType = call.Callee->Type;

        if (calleeType->Kind != TypeKind::Function && !calleeType->is_error()) {
            m_Context->report_compiler_diagnostic(expr->Loc, expr->Range, "Cannot call an object of non-function type");
            expr->Type = &error_type;
            return;
        }

        if (call.Callee->DeclRef.ReferencedDecl->Kind != DeclKind::Error) {
            if (call.Callee->Kind == ExprKind::DeclRef && call.Callee->DeclRef.ReferencedDecl->Kind == DeclKind::OverloadedFunction) { // Overloaded function
                m_TemporaryContext = true;
                for (Expr* arg : call.Arguments) {
                    resolve_expr(arg);
                }
                m_TemporaryContext = false;

                Decl* resolved = nullptr;
                Module* mod = call.Callee->DeclRef.NameSpecifier ? call.Callee->DeclRef.NameSpecifier->Scope.ReferencedModule : m_Context->ActiveCompUnit->Parent;

                for (Decl* func : mod->OverloadedFuncs.at(fmt::format("{}", call.Callee->DeclRef.Identifier))) {
                    for (size_t i = 0; i < func->Function.Type->Function.ParamTypes.Size; i++) {
                        if (!type_is_equal(func->Function.Type->Function.ParamTypes.Items[i], call.Arguments.Items[i]->Type)) { goto again; }
                    }

                    goto done;

                done:
                    resolved = func;
                    break;

                again:
                    continue;
                }

                if (!resolved) {
                    m_Context->report_compiler_diagnostic(expr->Loc, expr->Range, fmt::format("No matching overloaded function '{}' to call", call.Callee->DeclRef.Identifier));
                    for (size_t i = 0; i < call.Arguments.Size; i++) {
                        call.Arguments.Items[i]->Type = &error_type;
                    }
                }

                call.Callee->DeclRef.ReferencedDecl = resolved;
                expr->Type = resolved->Function.Type->Function.ReturnType;
                return;
            } else if (!call.Callee->Type->is_error()) { // Normal function
                FunctionDeclaration& fnDecl = calleeType->Function;

                for (auto& attr : call.Callee->DeclRef.ReferencedDecl->Function.Attributes) {
                    if (attr.Kind == FunctionDecl::AttributeKind::Unsafe && !m_UnsafeContext) {
                        m_Context->report_compiler_diagnostic(expr->Loc, expr->Range, "Cannot call unsafe function in safe context");
                    }
                }

                if (fnDecl.ParamTypes.Size != call.Arguments.Size) {
                    m_Context->report_compiler_diagnostic(expr->Loc, expr->Range, fmt::format("Mismatched argument count, function expects {} but got {}", fnDecl.ParamTypes.Size, call.Arguments.Size));
                    for (size_t i = 0; i < call.Arguments.Size; i++) {
                        call.Arguments.Items[i]->Type = &error_type;
                    }
                } else {
                    for (size_t i = 0; i < fnDecl.ParamTypes.Size; i++) {
                        resolve_param_initializer(fnDecl.ParamTypes.Items[i], call.Arguments.Items[i]);
                    }
                }

                expr->Type = fnDecl.ReturnType;
                expr->ValueKind = (fnDecl.ReturnType->is_reference()) ? ExprValueKind::LValue : ExprValueKind::RValue;

                // We may need to create a temporary if a function returns a non-trivial type and it is discarded
                if (expr->ResultDiscarded && !fnDecl.ReturnType->is_reference()) {
                    if (fnDecl.ReturnType->is_string()) {
                        replace_expr(expr, Expr::Create(m_Context, expr->Loc, expr->Range, ExprKind::Temporary,
                            ExprValueKind::RValue, expr->Type,
                            TemporaryExpr(Expr::Dup(m_Context, expr), m_BuiltInStringDestructor)));
                    }
                }

                return;
            }
        } else {
            for (Expr* arg : call.Arguments) {
                resolve_expr(arg);
            }
        }

        expr->Type = &error_type;
    }

    void SemanticAnalyzer::resolve_Construct_expr(Expr* expr) { ARIA_UNREACHABLE(); }

    void SemanticAnalyzer::resolve_MethodCall_expr(Expr* expr) {
        ARIA_ASSERT(false, "todo!");
    }

    void SemanticAnalyzer::resolve_ArraySubscript_expr(Expr* expr) {
        ArraySubscriptExpr& subs = expr->ArraySubscript;

        resolve_expr(subs.Array);
        resolve_expr(subs.Index);
        require_rvalue(subs.Index);

        if (subs.Array->Type->is_error()) { expr->Type = &error_type; return; }

        switch (subs.Array->Type->Kind) {
            case TypeKind::Ptr: {
                require_rvalue(subs.Array);
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

            default: m_Context->report_compiler_diagnostic(subs.Array->Loc, subs.Array->Range, "'[' operator can only be used with a pointer/slice/array"); break;
        }

        ConversionCost cost = get_conversion_cost(&ulong_type, subs.Index->Type);
        if (cost.CastNeeded) {
            if (cost.ImplicitCastPossible) {
                insert_implicit_cast(&ulong_type, subs.Index->Type, subs.Index, cost.Kind);
            } else {
                m_Context->report_compiler_diagnostic(subs.Index->Loc, subs.Index->Range, fmt::format("Array index cannot be implicitly converted from '{}' to 'ulong'", type_info_to_string(subs.Index->Type)));
            }
        }
    }

    void SemanticAnalyzer::resolve_ToSlice_expr(Expr* expr) {
        ToSliceExpr& tos = expr->ToSlice;

        resolve_expr(tos.Source);
        resolve_expr(tos.Len);
        require_rvalue(tos.Len);

        if (tos.Source->Type->is_error()) { expr->Type = &error_type; return; }

        switch (tos.Source->Type->Kind) {
            case TypeKind::Ptr: {
                require_rvalue(tos.Source);
                expr->Type = TypeInfo::Create(m_Context, TypeKind::Slice, false);
                expr->Type->Base = tos.Source->Type->Base;
                break;
            }

            case TypeKind::Slice: {
                ARIA_TODO("slice to slice");
                // require_rvalue(subs.Array);
                // expr->Type = subs.Array->Type->Base;
                // break;
            }

            case TypeKind::Array: {
                ARIA_TODO("array to slice");
                // expr->Type = subs.Array->Type->Array.Type;
                // break;
            }

            default: m_Context->report_compiler_diagnostic(tos.Source->Loc, tos.Source->Range, "Only a pointer/slice/array can be converted to a slice"); expr->Type = &error_type; break;
        }

        ConversionCost cost = get_conversion_cost(&ulong_type, tos.Len->Type);
        if (cost.CastNeeded) {
            if (cost.ImplicitCastPossible) {
                insert_implicit_cast(&ulong_type, tos.Len->Type, tos.Len, cost.Kind);
            } else {
                m_Context->report_compiler_diagnostic(tos.Len->Loc, tos.Len->Range, fmt::format("Slice length cannot be implicitly converted from '{}' to 'ulong'", type_info_to_string(tos.Len->Type)));
            }
        }
    }

    void SemanticAnalyzer::resolve_New_expr(Expr* expr) {
        NewExpr& n = expr->New;
        
        if (expr->ResultDiscarded) {
            m_Context->report_compiler_diagnostic(expr->Loc, expr->Range, "Discarding result of 'new' expression is not allowed");
        }

        if (!m_UnsafeContext) {
            m_Context->report_compiler_diagnostic(expr->Loc, expr->Range, "'new' is only allowed in unsafe context");
        }

        if (!n.Initializer) {
            create_default_initializer(&n.Initializer, expr->Type->Base, expr->Loc, expr->Range);
        } else {
            if (n.Array) {
                m_TemporaryContext = true;
                resolve_expr(n.Initializer);
                require_rvalue(n.Initializer);
                m_TemporaryContext = false;

                ConversionCost cost = get_conversion_cost(&ulong_type, n.Initializer->Type);
                if (cost.CastNeeded) {
                    if (cost.ImplicitCastPossible) {
                        insert_implicit_cast(&ulong_type, n.Initializer->Type, n.Initializer, cost.Kind);
                    } else {
                        m_Context->report_compiler_diagnostic(n.Initializer->Loc, n.Initializer->Range, fmt::format("Cannot implicitly convert from '{}' to '{}'", type_info_to_string(n.Initializer->Type), type_info_to_string(&ulong_type)));
                    }
                }
            } else {
                m_TemporaryContext = true;
                resolve_expr(n.Initializer);
                require_rvalue(n.Initializer);
                m_TemporaryContext = false;

                ConversionCost cost = get_conversion_cost(expr->Type->Base, n.Initializer->Type);
                if (cost.CastNeeded) {
                    if (cost.ImplicitCastPossible) {
                        insert_implicit_cast(expr->Type->Base, n.Initializer->Type, n.Initializer, cost.Kind);
                    } else {
                        m_Context->report_compiler_diagnostic(n.Initializer->Loc, n.Initializer->Range, fmt::format("Cannot implicitly convert from '{}' to '{}'", type_info_to_string(n.Initializer->Type), type_info_to_string(expr->Type->Base)));
                    }
                }
            }
        }
    }

    void SemanticAnalyzer::resolve_Delete_expr(Expr* expr) {
        DeleteExpr& d = expr->Delete;

        if (!m_UnsafeContext) {
            m_Context->report_compiler_diagnostic(expr->Loc, expr->Range, "'delete' is only allowed in unsafe context");
        }

        resolve_expr(d.Expression);
        require_rvalue(d.Expression);

        if (!d.Expression->Type->is_pointer()) {
            m_Context->report_compiler_diagnostic(expr->Loc, expr->Range, "'delete' can only be used with pointer types");
        }
    }

    void SemanticAnalyzer::resolve_Format_expr(Expr* expr) {
        ARIA_TODO("SemanticAnalyzer::resolve_Format_expr()");
        // FormatExpr& format = expr->Format;
        // 
        // if (format.Args.Size == 0) {
        //     m_Context->report_compiler_diagnostic(expr->Loc, expr->Range, "Format expression must have a format string");
        //     return;
        // }
        // 
        // if (format.Args.Items[0]->Kind != ExprKind::StringConstant) {
        //     m_Context->report_compiler_diagnostic(format.Args.Items[0]->Loc, format.Args.Items[0]->Range, "Format string must be a string literal");
        //     return;
        // }
        // 
        // if (expr->ResultDiscarded) {
        //     m_Context->report_compiler_diagnostic(expr->Loc, expr->Range, "Discarding result of format is not allowed");
        //     return;
        // }
        // 
        // for (Expr* arg : format.Args) {
        //     m_TemporaryContext = true;
        //     resolve_expr(arg);
        // 
        //     require_rvalue(arg);
        //     m_TemporaryContext = false;
        // }
        // 
        // scratch_buffer_clear();
        // TinyVector<FormatExpr::FormatArg> formattedArgs;
        // std::string_view fmtStr = format.Args.Items[0]->Temporary.Expression->StringConstant.Value;
        // 
        // size_t idx = 0;
        // bool needsClosing = false;
        // 
        // for (size_t i = 0; i < fmtStr.length(); i++) {
        //     if (needsClosing) {
        //         if (fmtStr.at(i) == '}') {
        //             needsClosing = false;
        //             continue;
        //         } else {
        //             m_Context->report_compiler_diagnostic(expr->Loc, expr->Range, "Missing closing curly brace in format string");
        //             return;
        //         }
        //     }
        // 
        //     if (fmtStr.at(i) == '{') {
        //         if (scratch_buffer_size() > 0) {
        //             StringBuilder tmpB;
        //             tmpB.Append(m_Context, buf);
        //             buf.Clear();
        // 
        //             Expr* str = Expr::Create(m_Context, expr->Loc, expr->Range, ExprKind::StringConstant,
        //                 ExprValueKind::RValue, &StringType,
        //                 StringConstantExpr(tmpB));
        // 
        //             formattedArgs.Append(m_Context, { Expr::Create(m_Context, expr->Loc, expr->Range, ExprKind::Temporary,
        //                 ExprValueKind::RValue, &StringType,
        //                 TemporaryExpr(str, m_BuiltInStringDestructor)) });
        //         }
        //         
        //         if (idx + 1 >= format.Args.Size) {
        //             m_Context->report_compiler_diagnostic(expr->Loc, expr->Range, "Format string specifies more arguments than are provided");
        //             return;
        //         }
        // 
        //         formattedArgs.Append(m_Context, { format.Args.Items[idx + 1] });
        //         needsClosing = true;
        //         idx++;
        //     } else {
        //         buf.Append(m_Context, fmtStr.At(i));
        //     }
        // }
        // 
        // if (buf.Size() > 0) {
        //     StringBuilder tmpB;
        //     tmpB.Append(m_Context, buf);
        //     buf.Clear();
        // 
        //     Expr* str = Expr::Create(m_Context, expr->Loc, expr->Range, ExprKind::StringConstant,
        //         ExprValueKind::RValue, &StringType,
        //         StringConstantExpr(tmpB));
        // 
        //     formattedArgs.Append(m_Context, { Expr::Create(m_Context, expr->Loc, expr->Range, ExprKind::Temporary,
        //         ExprValueKind::RValue, &StringType,
        //         TemporaryExpr(str, m_BuiltInStringDestructor)) });
        // }
        // 
        // format.ResolvedArgs = formattedArgs;
        // if (m_TemporaryContext) {
        //     replace_expr(expr, Expr::Create(m_Context, expr->Loc, expr->Range, ExprKind::Temporary,
        //         ExprValueKind::RValue, expr->Type,
        //         TemporaryExpr(Expr::Dup(m_Context, expr), m_BuiltInStringDestructor)));
        // }
    }

    void SemanticAnalyzer::resolve_Paren_expr(Expr* expr) {
        ParenExpr& paren = expr->Paren;
        resolve_expr(paren.Expression);

        expr->Type = paren.Expression->Type;
        expr->ValueKind = paren.Expression->ValueKind;

        if (expr->ResultDiscarded) {
            m_Context->report_compiler_diagnostic(expr->Loc, expr->Range, "Discarding result of expression", CompilerDiagKind::Warning);
        }
    }

    void SemanticAnalyzer::resolve_Cast_expr(Expr* expr) {
        CastExpr& cast = expr->Cast;
        
        resolve_expr(cast.Expression);
        require_rvalue(cast.Expression);
        expr->Type = cast.Type;

        TypeInfo* srcType = cast.Expression->Type;
        TypeInfo* dstType = cast.Type;

        ConversionCost cost = get_conversion_cost(dstType, srcType);

        if (cost.CastNeeded) {
            if (cost.ExplicitCastPossible) {
                insert_implicit_cast(dstType, srcType, cast.Expression, cost.Kind);
            } else {
                m_Context->report_compiler_diagnostic(expr->Loc, expr->Range, fmt::format("Cannot cast '{}' to '{}'", type_info_to_string(srcType), type_info_to_string(dstType)));
            }
        }

        if (expr->ResultDiscarded) {
            m_Context->report_compiler_diagnostic(expr->Loc, expr->Range, "Discarding result of explicit cast", CompilerDiagKind::Warning);
        }
    }

    void SemanticAnalyzer::resolve_ImplicitCast_expr(Expr* expr) { ARIA_UNREACHABLE(); }

    void SemanticAnalyzer::resolve_UnaryOperator_expr(Expr* expr) {
        UnaryOperatorExpr& unop = expr->UnaryOperator;
        
        resolve_expr(unop.Expression);
        TypeInfo* type = unop.Expression->Type;
        
        switch (unop.Operator) {
            case UnaryOperatorKind::Negate: {
                require_rvalue(unop.Expression);
                ARIA_ASSERT(type->is_numeric(), "todo: add error message");

                if (type->is_integral()) {
                    if (type->is_unsigned()) {
                        m_Context->report_compiler_diagnostic(expr->Loc, expr->Range, fmt::format("Cannot negate expression of unsigned type '{}'", type_info_to_string(type)));
                    }
                }

                expr->Type = type;
                break;
            }

            case UnaryOperatorKind::AddressOf: {
                if (type->is_error()) { expr->Type = type; break; }

                if (!m_UnsafeContext) {
                    m_Context->report_compiler_diagnostic(expr->Loc, expr->Range, "Address of operation ('&') must be in an unsafe context");
                }

                if (unop.Expression->ValueKind != ExprValueKind::LValue) {
                    m_Context->report_compiler_diagnostic(expr->Loc, expr->Range, "Address of operation ('&') requries an lvalue");
                }

                TypeInfo* newType = TypeInfo::Create(m_Context, TypeKind::Ptr, false);
                newType->Base = type;
                expr->Type = newType;
                break;
            }

            case UnaryOperatorKind::Dereference: {
                if (type->is_error()) { expr->Type = type; break; }

                if (!m_UnsafeContext) {
                    m_Context->report_compiler_diagnostic(expr->Loc, expr->Range, "Dereferencing of pointer must be in an unsafe context");
                }

                if (unop.Expression->ValueKind != ExprValueKind::LValue) {
                    m_Context->report_compiler_diagnostic(expr->Loc, expr->Range, "Dereferencing requires an lvalue");
                }

                require_rvalue(unop.Expression);

                if (type->is_pointer()) {
                    if (type->Base->is_void()) {
                        m_Context->report_compiler_diagnostic(expr->Loc, expr->Range, "Cannot dereference a void*");
                    }
                } else {
                    m_Context->report_compiler_diagnostic(expr->Loc, expr->Range, "Dereferencing requires a pointer type");
                    break;
                }

                expr->Type = type->Base;
                expr->ValueKind = ExprValueKind::LValue;
                break;
            }

            default: ARIA_UNREACHABLE();
        }
    }

    void SemanticAnalyzer::resolve_BinaryOperator_expr(Expr* expr) {
        BinaryOperatorExpr& binop = expr->BinaryOperator;

        resolve_expr(binop.LHS);
        resolve_expr(binop.RHS);

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
                if (!LHS->Type->is_error()) {
                    if (!LHS->Type->is_numeric()) {
                        m_Context->report_compiler_diagnostic(LHS->Loc, LHS->Range, fmt::format("Expression must be of a numeric type but is of type '{}'", type_info_to_string(LHS->Type)));
                    }

                    if (!LHS->Type->is_numeric()) {
                        m_Context->report_compiler_diagnostic(RHS->Loc, RHS->Range, fmt::format("Expression must be of a numeric type but is of type '{}'", type_info_to_string(RHS->Type)));
                    }
                }

                insert_arithmetic_promotion(LHS, RHS);

                if (binop.Operator == BinaryOperatorKind::Less ||
                    binop.Operator == BinaryOperatorKind::LessOrEq ||
                    binop.Operator == BinaryOperatorKind::Greater ||
                    binop.Operator == BinaryOperatorKind::GreaterOrEq ||
                    binop.Operator == BinaryOperatorKind::IsEq ||
                    binop.Operator == BinaryOperatorKind::IsNotEq) 
                {
                    expr->Type = &bool_type;
                    expr->ValueKind = ExprValueKind::RValue;

                    if (expr->ResultDiscarded) {
                        m_Context->report_compiler_diagnostic(expr->Loc, expr->Range, "Discarding result of relational operator", CompilerDiagKind::Warning);
                    }
                    return;
                }

                expr->Type = LHS->Type;
                expr->ValueKind = ExprValueKind::RValue;

                if (expr->ResultDiscarded) {
                    m_Context->report_compiler_diagnostic(expr->Loc, expr->Range, "Discarding result of binary operator", CompilerDiagKind::Warning);
                }
                return;
            }

            case BinaryOperatorKind::BitAnd:
            case BinaryOperatorKind::BitOr:
            case BinaryOperatorKind::BitXor:
            case BinaryOperatorKind::Shl:
            case BinaryOperatorKind::Shr: {
                if (!LHS->Type->is_error()) {
                    if (!LHS->Type->is_integral() && !LHS->Type->is_string()) {
                        m_Context->report_compiler_diagnostic(LHS->Loc, LHS->Range, fmt::format("Expression must be of a integral type but is of type '{}'", type_info_to_string(LHS->Type)));
                    }

                    if (!LHS->Type->is_integral()) {
                        m_Context->report_compiler_diagnostic(RHS->Loc, RHS->Range, fmt::format("Expression must be of a integral type but is of type '{}'", type_info_to_string(RHS->Type)));
                    }
                }

                insert_arithmetic_promotion(LHS, RHS);

                expr->Type = LHS->Type;
                expr->ValueKind = ExprValueKind::RValue;

                if (expr->ResultDiscarded) {
                    m_Context->report_compiler_diagnostic(expr->Loc, expr->Range, "Discarding result of bitwise operator", CompilerDiagKind::Warning);
                }
                return;
            }

            case BinaryOperatorKind::Eq: {
                if (LHS->ValueKind != ExprValueKind::LValue) {
                    m_Context->report_compiler_diagnostic(LHS->Loc, LHS->Range, "Expression must be a modifiable lvalue");
                }

                require_rvalue(RHS);

                ConversionCost cost = get_conversion_cost(LHS->Type, RHS->Type);

                if (cost.CastNeeded) {
                    if (cost.ImplicitCastPossible) {
                        insert_implicit_cast(LHS->Type, RHS->Type, RHS, cost.Kind);
                    } else {
                        m_Context->report_compiler_diagnostic(expr->Loc, expr->Range, fmt::format("Cannot implicitly convert from '{}' to '{}'", type_info_to_string(RHS->Type), type_info_to_string(LHS->Type)));
                    }
                }

                expr->Type = LHS->Type;
                expr->ValueKind = ExprValueKind::LValue;
                return;
            }

            case BinaryOperatorKind::LogAnd:
            case BinaryOperatorKind::LogOr: {
                TypeInfo* boolType = TypeInfo::Create(m_Context, TypeKind::Bool, false);

                require_rvalue(LHS);
                require_rvalue(RHS);

                ConversionCost costLHS = get_conversion_cost(boolType, LHS->Type);
                ConversionCost costRHS = get_conversion_cost(boolType, RHS->Type);

                if (costLHS.CastNeeded) {
                    if (costLHS.ImplicitCastPossible) {
                        insert_implicit_cast(boolType, LHS->Type, LHS, costLHS.Kind);
                    } else {
                        m_Context->report_compiler_diagnostic(LHS->Loc, LHS->Range, fmt::format("Cannot implicitly convert from '{}' to 'bool'", type_info_to_string(LHS->Type)));
                    }
                }

                if (costRHS.CastNeeded) {
                    if (costRHS.ImplicitCastPossible) {
                        insert_implicit_cast(boolType, RHS->Type, RHS, costRHS.Kind);
                    } else {
                        m_Context->report_compiler_diagnostic(LHS->Loc, LHS->Range, fmt::format("Cannot implicitly convert from '{}' to 'bool'", type_info_to_string(RHS->Type)));
                    }
                }

                expr->Type = boolType;
                expr->ValueKind = ExprValueKind::RValue;

                if (expr->ResultDiscarded) {
                    m_Context->report_compiler_diagnostic(expr->Loc, expr->Range, "Discarding result of logical operator", CompilerDiagKind::Warning);
                }
                return;
            }

            default: ARIA_UNREACHABLE();
        }
    }

    void SemanticAnalyzer::resolve_CompoundAssign_expr(Expr* expr) {
        CompoundAssignExpr& compAss = expr->CompoundAssign;
        
        resolve_expr(compAss.LHS);
        resolve_expr(compAss.RHS);

        require_rvalue(compAss.RHS);
        
        Expr* LHS = compAss.LHS;
        Expr* RHS = compAss.RHS;
        
        TypeInfo* LHSType = LHS->Type;
        TypeInfo* RHSType = RHS->Type;
        
        if (LHS->ValueKind != ExprValueKind::LValue) {
            m_Context->report_compiler_diagnostic(compAss.LHS->Loc, compAss.LHS->Range, "Expression must be a modifiable lvalue");
        }
        
        ConversionCost cost = get_conversion_cost(LHSType, RHSType);
        
        if (cost.CastNeeded) {
            if (cost.ImplicitCastPossible) {
                insert_implicit_cast(LHSType, RHSType, RHS, cost.Kind);
                RHSType = LHSType;
            } else {
                m_Context->report_compiler_diagnostic(compAss.RHS->Loc, compAss.RHS->Range, fmt::format("Cannot implicitly convert from '{}' to '{}'", type_info_to_string(RHSType), type_info_to_string(LHSType)));
            }
        }
        
        expr->Type = LHSType;
        expr->ValueKind = ExprValueKind::LValue;
    }

    void SemanticAnalyzer::resolve_expr(Expr* expr) {
        #define EXPR_CASE(kind) resolve_##kind##_expr(expr)
        #include "aria/internal/compiler/ast/expr_switch.hpp"
        #undef EXPR_CASE
    }

    bool SemanticAnalyzer::is_const_expr(Expr* expr) {
        switch (expr->Kind) {
            case ExprKind::Error:
            case ExprKind::BooleanConstant:
            case ExprKind::CharacterConstant:
            case ExprKind::IntegerConstant:
            case ExprKind::FloatingConstant:
            case ExprKind::StringConstant:
            case ExprKind::Null:
                return true;

            case ExprKind::Paren:
                return is_const_expr(expr->Paren.Expression);

            case ExprKind::UnaryOperator:
                return is_const_expr(expr->UnaryOperator.Expression);

            case ExprKind::BinaryOperator:
                return is_const_expr(expr->BinaryOperator.LHS) && is_const_expr(expr->BinaryOperator.RHS);

            default: return false;
        }
    }

    bool SemanticAnalyzer::eval_expr_bool(Expr* expr) {
        ARIA_ASSERT(is_const_expr(expr), "Cannot evaulate a non-constant expression");
        ARIA_ASSERT(expr->Type->is_boolean(), "Type of expression must be bool");

        switch (expr->Kind) {
            case ExprKind::BooleanConstant: return expr->BooleanConstant.Value;

            case ExprKind::Paren: return eval_expr_bool(expr->Paren.Expression);

            case ExprKind::BinaryOperator: {
                bool lhs = eval_expr_bool(expr->BinaryOperator.LHS);
                bool rhs = eval_expr_bool(expr->BinaryOperator.RHS);

                switch (expr->BinaryOperator.Operator) {
                    case BinaryOperatorKind::Eq: return lhs == rhs;
                    case BinaryOperatorKind::IsNotEq: return lhs != rhs;
                    case BinaryOperatorKind::LogAnd: return lhs && rhs;
                    case BinaryOperatorKind::LogOr: return lhs || rhs;
                    default: ARIA_UNREACHABLE();
                }

                break;
            }

            default: ARIA_UNREACHABLE();
        }
    }

    void SemanticAnalyzer::insert_implicit_cast(TypeInfo* dstType, TypeInfo* srcType, Expr* srcExpr, CastKind castKind) {
        Expr* src = Expr::Dup(m_Context, srcExpr); // We must copy the original expression to avoid overwriting the same memory
        Expr* implicitCast = Expr::Create(m_Context, src->Loc, src->Range, ExprKind::ImplicitCast, ExprValueKind::RValue, dstType, ImplicitCastExpr(src, castKind));

        replace_expr(srcExpr, implicitCast);
    }

    void SemanticAnalyzer::require_rvalue(Expr* expr) {
        if (expr->ValueKind == ExprValueKind::LValue) {
            if (expr->Type->Kind == TypeKind::String) {
                replace_expr(expr, Expr::Create(m_Context, expr->Loc, expr->Range, 
                    ExprKind::Copy, ExprValueKind::RValue, expr->Type, 
                    CopyExpr(Expr::Dup(m_Context, expr), m_BuiltInStringCopyConstructor)));

                if (m_TemporaryContext) {
                    replace_expr(expr, Expr::Create(m_Context, expr->Loc, expr->Range,
                        ExprKind::Temporary, ExprValueKind::RValue, expr->Type,
                        TemporaryExpr(Expr::Dup(m_Context, expr), m_BuiltInStringDestructor)));
                }
            } else {
                insert_implicit_cast(expr->Type, expr->Type, expr, CastKind::LValueToRValue);
            }
        }
    }

    void SemanticAnalyzer::insert_arithmetic_promotion(Expr* lhs, Expr* rhs) {
        TypeInfo* lhsType = lhs->Type;
        TypeInfo* rhsType = rhs->Type;

        if (lhsType->Kind == TypeKind::Error || rhsType->Kind == TypeKind::Error) {
            return;
        }

        require_rvalue(lhs);
        require_rvalue(rhs);

        if (type_is_equal(lhsType, rhsType)) {
            return;
        }

        if (lhsType->is_integral() && rhsType->is_integral()) {
            size_t lSize = type_get_size(lhsType);
            size_t rSize = type_get_size(rhsType);

            if (lSize > rSize) {
                insert_implicit_cast(lhsType, rhsType, rhs, CastKind::Integral);
            } else if (rSize > lSize) {
                insert_implicit_cast(rhsType, lhsType, lhs, CastKind::Integral);
            } else if (lSize == rSize) {
                // We know that the types are not equal so we likely have a signed/unsigned mismatch
                m_Context->report_compiler_diagnostic(lhs->Loc, SourceRange(lhs->Range.Start, rhs->Range.End), fmt::format("Mismatched types '{}' and '{}' (implicit signedness conversions are not allowed here)", type_info_to_string(lhsType), type_info_to_string(rhsType)));
            }

            return;
        }

        if (lhsType->is_integral() && rhsType->is_floating_point()) {
            insert_implicit_cast(rhsType, lhsType, lhs, CastKind::IntegralToFloating);
            return;
        }

        if (lhsType->is_floating_point() && rhsType->is_integral()) {
            insert_implicit_cast(lhsType, rhsType, rhs, CastKind::IntegralToFloating);
            return;
        }

        if (lhsType->is_floating_point() && rhsType->is_floating_point()) {
            size_t lSize = type_get_size(lhsType);
            size_t rSize = type_get_size(rhsType);

            if (lSize > rSize) {
                insert_implicit_cast(lhsType, rhsType, rhs, CastKind::Floating);
            } else if (rSize > lSize) {
                insert_implicit_cast(rhsType, lhsType, lhs, CastKind::Floating);
            }

            return;
        }

        ARIA_UNREACHABLE();
    }

} // namespace Aria::Internal