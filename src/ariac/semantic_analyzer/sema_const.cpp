#include "ariac/semantic_analyzer/semantic_analyzer.hpp"

namespace ariac {

    void SemanticAnalyzer::eval_const_func_decl(Decl* func) {
        FunctionDecl& f = func->function;
        eval_const_compound_stmt(f.body);
    }
    
    bool SemanticAnalyzer::is_const_expr(Expr* expr) {
        switch (expr->kind) {
            case ExprKind::Error:
            case ExprKind::BooleanLiteral:
            case ExprKind::CharacterLiteral:
            case ExprKind::IntegerLiteral:
            case ExprKind::FloatingLiteral:
            case ExprKind::StringLiteral:
            case ExprKind::Null:
            case ExprKind::TypeInfo:
                return true;
    
            case ExprKind::DeclRef: {
                switch (expr->decl_ref.referenced_decl->kind) {
                    case DeclKind::Var: return expr->decl_ref.referenced_decl->var.const_var;

                    case DeclKind::Param: {
                        auto ctx = m_inlines.get<ConstantEvaluationContext>();
                        if (!ctx) { return false; }

                        return ctx->parameters.contains(expr->decl_ref.referenced_decl);
                    }
    
                    default: return false;
                }
            }
    
            case ExprKind::TypeMember:
                return expr->type_member.type->is_enum();
    
            case ExprKind::BuiltinCall:
                return expr->builtin_call.kind == BuiltinCallKind::Defined;
    
            case ExprKind::Construct:
                return expr->construct.is_const;
    
            case ExprKind::Paren:
                return is_const_expr(expr->paren.expression);
    
            case ExprKind::ImplicitCast:
                return is_const_expr(expr->implicit_cast.expression);
    
            case ExprKind::UnaryOperator:
                return is_const_expr(expr->unary_operator.expression);
    
            case ExprKind::BinaryOperator:
                return is_const_expr(expr->binary_operator.lhs) && is_const_expr(expr->binary_operator.rhs);
    
            case ExprKind::Const: return true;
    
            default: return false;
        }
    }
    
    Expr* SemanticAnalyzer::eval_const_expr(Expr* expr) {
        ARIA_ASSERT(is_const_expr(expr), "Cannot evaulate a non-constant expression");
    
        switch (expr->kind) {
            // Already evaluated
            case ExprKind::Const: return expr;
    
            case ExprKind::Error: 
                return Expr::Create(expr->loc, ExprKind::Const, ExprValueKind::RValue, expr->type, ConstExpr(ConstExprKind::Error));
    
            case ExprKind::BooleanLiteral: 
                return Expr::Create(expr->loc, ExprKind::Const, ExprValueKind::RValue, expr->type, ConstExpr(ConstExprKind::Bool, expr->boolean_literal.value));
    
            case ExprKind::CharacterLiteral: 
                return Expr::Create(expr->loc, ExprKind::Const, ExprValueKind::RValue, expr->type, ConstExpr(ConstExprKind::Int, static_cast<u64>(expr->character_literal.value)));
    
            case ExprKind::IntegerLiteral: 
                return Expr::Create(expr->loc, ExprKind::Const, ExprValueKind::RValue, expr->type, ConstExpr(ConstExprKind::Int, expr->integer_literal.value));
    
            case ExprKind::FloatingLiteral: 
                return Expr::Create(expr->loc, ExprKind::Const, ExprValueKind::RValue, expr->type, ConstExpr(ConstExprKind::Float, expr->floating_literal.value));
    
            case ExprKind::StringLiteral: 
                return Expr::Create(expr->loc, ExprKind::Const, ExprValueKind::RValue, expr->type, ConstExpr(ConstExprKind::String, expr->string_literal.value));
    
            case ExprKind::DeclRef: {
                resolve_decl(expr->decl_ref.referenced_decl);
    
                switch (expr->decl_ref.referenced_decl->kind) {
                    case DeclKind::Var: {
                        ARIA_ASSERT(expr->decl_ref.referenced_decl->var.const_var, "Referenced decl must be const");
                        return expr->decl_ref.referenced_decl->var.initializer;
                    }

                    case DeclKind::Param: {
                        auto ctx = m_inlines.get<ConstantEvaluationContext>();
                        ARIA_ASSERT(ctx, "Must be in a constant context");

                        return ctx->parameters.at(expr->decl_ref.referenced_decl);
                    }
 
                    default: ARIA_UNREACHABLE("Invalid decl kind");
                }
            }
    
            case ExprKind::TypeInfo:
                return Expr::Create(expr->loc, ExprKind::Const, ExprValueKind::RValue, expr->type, ConstExpr(ConstExprKind::Typeid, expr->type_info.type));
    
            case ExprKind::TypeMember: {
                ARIA_ASSERT(expr->type_member.referenced_member, "Invalid type member expression");
                ARIA_ASSERT(expr->type_member.referenced_member->kind == DeclKind::EnumConstant, "Invalid type member expression");
                return Expr::Create(expr->loc, ExprKind::Const, ExprValueKind::RValue, expr->type, ConstExpr(ConstExprKind::Int, expr->type_member.referenced_member->enum_constant.resolved_value));
            }
    
            case ExprKind::Construct:
                for (Expr*& arg : expr->construct.arguments) {
                    arg = eval_const_expr(arg);
                }
    
                return Expr::Create(expr->loc, ExprKind::Const, ExprValueKind::RValue, expr->type, ConstExpr(ConstExprKind::Struct, expr->construct.arguments));
    
            case ExprKind::Paren:
                return eval_const_expr(expr->paren.expression);
    
            case ExprKind::UnaryOperator: {
                Expr* val = eval_const_expr(expr->unary_operator.expression);
    
                switch (expr->unary_operator.op) {
                    case UnaryOperatorKind::Negate: {
                        switch (val->const_.kind) {
                            case ConstExprKind::Int: {
                                val->const_.integer = static_cast<u64>(-static_cast<i64>(val->const_.integer));
                                return val;
                            }
    
                            case ConstExprKind::Float: {
                                val->const_.number = -val->const_.number;
                                return val;
                            }
    
                            default: ARIA_UNREACHABLE("Invalid const expr kind");
                        }
    
                        ARIA_UNREACHABLE("Should never be reached");
                    }
    
                    default: ARIA_UNREACHABLE("Invalid unary operator");
                }
    
                return nullptr;
            }
    
            case ExprKind::ImplicitCast: {
                #define CAST(t, e) static_cast<t>(e)
                #define INT(x) Expr::Create(expr->loc, ExprKind::Const, ExprValueKind::RValue, expr->type, ConstExpr(ConstExprKind::Int, x))
                #define FLOAT(x) Expr::Create(expr->loc, ExprKind::Const, ExprValueKind::RValue, expr->type, ConstExpr(ConstExprKind::Float, x))
    
                switch (expr->implicit_cast.kind) {
                    case CastKind::IntegralCast: {
                        if (expr->implicit_cast.expression->type->is_signed()) {
                            i64 val = eval_const_expr(expr->implicit_cast.expression)->const_.integer;
    
                            switch (expr->type->kind) {
                                case TypeKind::Char: return INT(CAST(u64, CAST(u8, val)));
                                case TypeKind::IChar: return INT(CAST(i64, CAST(i8, val)));
                                case TypeKind::Short: return INT(CAST(i64, CAST(i16, val)));
                                case TypeKind::UShort: return INT(CAST(u64, CAST(u16, val)));
                                case TypeKind::Int: return INT(CAST(i64, CAST(i32, val)));
                                case TypeKind::UInt: return INT(CAST(u64, CAST(u32, val)));
                                case TypeKind::Long: return INT(CAST(i64, val));
                                case TypeKind::ULong: return INT(CAST(u64, val));
    
                                default: ARIA_UNREACHABLE("Invalid type kind");
                            }
                        } else {
                            u64 val = eval_const_expr(expr->implicit_cast.expression)->const_.integer;
    
                            switch (expr->type->kind) {
                                case TypeKind::Char: return INT(CAST(u64, CAST(u8, val)));
                                case TypeKind::IChar: return INT(CAST(i64, CAST(i8, val)));
                                case TypeKind::Short: return INT(CAST(i64, CAST(i16, val)));
                                case TypeKind::UShort: return INT(CAST(u64, CAST(u16, val)));
                                case TypeKind::Int: return INT(CAST(i64, CAST(i32, val)));
                                case TypeKind::UInt: return INT(CAST(u64, CAST(u32, val)));
                                case TypeKind::Long: return INT(CAST(i64, val));
                                case TypeKind::ULong: return INT(CAST(u64, val));
    
                                default: ARIA_UNREACHABLE("Invalid type kind");
                            }
                        }
    
                        ARIA_UNREACHABLE("Should never be reached");
                        return nullptr;
                    }
    
                    case CastKind::IntegralToFloating: {
                        if (expr->implicit_cast.expression->type->is_signed()) {
                            i64 val = eval_const_expr(expr->implicit_cast.expression)->const_.integer;
    
                            switch (expr->type->kind) {
                                case TypeKind::Float: return FLOAT(CAST(double, CAST(float, val)));
                                case TypeKind::Double: return FLOAT(CAST(double, val));
    
                                default: ARIA_UNREACHABLE("Invalid type kind");
                            }
                        } else {
                            u64 val = eval_const_expr(expr->implicit_cast.expression)->const_.integer;
    
                            switch (expr->type->kind) {
                                case TypeKind::Float: return FLOAT(CAST(double, CAST(float, val)));
                                case TypeKind::Double: return FLOAT(CAST(double, val));
    
                                default: ARIA_UNREACHABLE("Invalid type kind");
                            }
                        }
    
                        ARIA_UNREACHABLE("Should never be reached");
                        return nullptr;
                    }
    
                    case CastKind::LValueToRValue: {
                        return eval_const_expr(expr->implicit_cast.expression);
                    }
    
                    default: ARIA_UNREACHABLE("Invalid cast kind");
                }
    
                #undef INT
                #undef CAST
    
                return nullptr;
            }
    
            case ExprKind::BinaryOperator: {
                Expr* lhs = eval_const_expr(expr->binary_operator.lhs);
                Expr* rhs = eval_const_expr(expr->binary_operator.rhs);
    
                switch (expr->binary_operator.op) {
                    case BinaryOperatorKind::Add: {
                        switch (lhs->const_.kind) {
                            case ConstExprKind::Int: {
                                return Expr::Create(expr->loc, ExprKind::Const, 
                                    ExprValueKind::RValue, lhs->type, 
                                    ConstExpr(ConstExprKind::Int, lhs->const_.integer + rhs->const_.integer));
                            }
    
                            case ConstExprKind::Float: {
                                return Expr::Create(expr->loc, ExprKind::Const, 
                                    ExprValueKind::RValue, lhs->type, 
                                    ConstExpr(ConstExprKind::Float, lhs->const_.number + rhs->const_.number));
                            }
    
                            default: ARIA_UNREACHABLE("Invalid const expr kind");
                        }
    
                        return nullptr;
                    }
    
                    case BinaryOperatorKind::Mul: {
                        switch (lhs->const_.kind) {
                            case ConstExprKind::Int: {
                                return Expr::Create(expr->loc, ExprKind::Const, 
                                    ExprValueKind::RValue, lhs->type, 
                                    ConstExpr(ConstExprKind::Int, lhs->const_.integer * rhs->const_.integer));
                            }
    
                            case ConstExprKind::Float: {
                                return Expr::Create(expr->loc, ExprKind::Const, 
                                    ExprValueKind::RValue, lhs->type, 
                                    ConstExpr(ConstExprKind::Float, lhs->const_.number * rhs->const_.number));
                            }
    
                            default: ARIA_UNREACHABLE("Invalid const expr kind");
                        }
    
                        return nullptr;
                    }
    
                    case BinaryOperatorKind::Div: {
                        switch (lhs->const_.kind) {
                            case ConstExprKind::Int: {
                                if (lhs->type->is_signed() && rhs->type->is_signed()) {
                                    return Expr::Create(expr->loc, ExprKind::Const, 
                                        ExprValueKind::RValue, lhs->type, 
                                        ConstExpr(ConstExprKind::Int, static_cast<i64>(lhs->const_.integer) / static_cast<i64>(rhs->const_.integer)));
                                } else if (lhs->type->is_unsigned() && rhs->type->is_unsigned()) {
                                    return Expr::Create(expr->loc, ExprKind::Const, 
                                        ExprValueKind::RValue, lhs->type, 
                                        ConstExpr(ConstExprKind::Int, lhs->const_.integer / rhs->const_.integer));
                                } else {
                                    ARIA_UNREACHABLE("Invalid type");
                                }
    
                                return nullptr;
                            }
    
                            case ConstExprKind::Float: {
                                return Expr::Create(expr->loc, ExprKind::Const, 
                                    ExprValueKind::RValue, lhs->type, 
                                    ConstExpr(ConstExprKind::Float, lhs->const_.number / rhs->const_.number));
                            }
    
                            default: ARIA_UNREACHABLE("Invalid const expr kind");
                        }
    
                        return nullptr;
                    }
    
                    default: ARIA_UNREACHABLE("Invalid binary operator");
                }
    
                return nullptr;
            }
    
            default: ARIA_UNREACHABLE("Should never be reached");
        }
    }
    
    void SemanticAnalyzer::eval_const_compound_stmt(Stmt* stmt) {
        CompoundStmt& co = stmt->compound;
        
        for (Stmt* s : co.stmts) {
            eval_const_stmt(s);
        }
    }
    
    void SemanticAnalyzer::eval_const_return_stmt(Stmt* stmt) {
        ReturnStmt& r = stmt->return_;
    
        if (!r.value) { return; }

        auto ctx = m_inlines.get<ConstantEvaluationContext>();
        if (!ctx) { return; }
    
        if (!is_const_expr(r.value)) {
            report_error(r.value->loc, "This expression must be constant");
            ctx->return_val = &error_expr;
            return;
        }
    
        ctx->return_val = eval_const_expr(r.value);
    }
    
    void SemanticAnalyzer::eval_const_stmt(Stmt* stmt) {
        switch (stmt->kind) {
            case StmtKind::Compound: return eval_const_compound_stmt(stmt);
            case StmtKind::Return: return eval_const_return_stmt(stmt);
            
            default: {
                report_error(stmt->loc, "This statement is not supported during constant evaluation");
                return;
            }
        }
    }

} // namespace ariac