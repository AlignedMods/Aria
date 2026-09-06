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
                    case DeclKind::Var: {
                        if (expr->decl_ref.referenced_decl->var.const_var) { return true; }

                        auto ctx = m_inlines.get<ConstantEvaluationContext>();
                        if (!ctx) { return false; }
                        return ctx->vars.contains(expr->decl_ref.referenced_decl);
                    }

                    case DeclKind::Param: {
                        auto ctx = m_inlines.get<ConstantEvaluationContext>();
                        if (!ctx) { return false; }

                        return ctx->parameters.contains(expr->decl_ref.referenced_decl);
                    }
    
                    default: return false;
                }
            }

            case ExprKind::BuiltinMember:
                return is_const_expr(expr->member.parent);
    
            case ExprKind::TypeMember:
                return expr->type_member.type->is_enum();
    
            case ExprKind::BuiltinCall:
                return expr->builtin_call.kind == BuiltinCallKind::Defined;
    
            case ExprKind::Construct:{
                for (Expr* arg : expr->construct.arguments) {
                    if (!is_const_expr(arg)) { return false; }
                }

                return true;
            }

            case ExprKind::ArraySubscript: {
                if (!is_const_expr(expr->array_subscript.array)) { return false; }
                return is_const_expr(expr->array_subscript.index);
            }
    
            case ExprKind::Paren:
                return is_const_expr(expr->paren.expression);
    
            case ExprKind::ImplicitCast:
                return is_const_expr(expr->implicit_cast.expression);
    
            case ExprKind::UnaryOperator:
                return is_const_expr(expr->unary_operator.expression);
    
            case ExprKind::BinaryOperator:
                return is_const_expr(expr->binary_operator.lhs) && is_const_expr(expr->binary_operator.rhs);

            case ExprKind::CompoundAssign:
                return is_const_expr(expr->compound_assign.lhs) && is_const_expr(expr->compound_assign.rhs);
    
            case ExprKind::Const: return true;
    
            default: return false;
        }
    }

    Expr* SemanticAnalyzer::make_const_bool(SourceLoc loc, TypeInfo* type, bool val) {
        return Expr::Create(loc, ExprKind::Const, 
            ExprValueKind::RValue, type,
            ConstExpr(ConstExprKind::Bool, val));
    }

    Expr* SemanticAnalyzer::make_const_uint(SourceLoc loc, TypeInfo* type, u64 val) {
        return Expr::Create(loc, ExprKind::Const, 
            ExprValueKind::RValue, type, 
            ConstExpr(ConstExprKind::Int, val));
    }

    Expr* SemanticAnalyzer::make_const_int(SourceLoc loc, TypeInfo* type, i64 val) {
        return Expr::Create(loc, ExprKind::Const, 
            ExprValueKind::RValue, type, 
            ConstExpr(ConstExprKind::Int, val));
    }

    Expr* SemanticAnalyzer::make_const_float(SourceLoc loc, TypeInfo* type, double val) {
        return Expr::Create(loc, ExprKind::Const, 
            ExprValueKind::RValue, type, 
            ConstExpr(ConstExprKind::Float, val));
    }

    Expr* SemanticAnalyzer::eval_const_binary_operator(SourceLoc loc, Expr* lhs, Expr* rhs, BinaryOperatorKind op) {
        switch (op) {
            case BinaryOperatorKind::Add:
            case BinaryOperatorKind::CompoundAdd: {
                switch (lhs->const_.kind) {
                    case ConstExprKind::Int: return make_const_uint(loc, lhs->type, lhs->const_.integer + rhs->const_.integer);
                    case ConstExprKind::Float: return make_const_float(loc, lhs->type, lhs->const_.number + rhs->const_.number);
    
                    default: ARIA_UNREACHABLE("Invalid const expr kind");
                }
            }
    
            case BinaryOperatorKind::Mul: {
                switch (lhs->const_.kind) {
                    case ConstExprKind::Int: return make_const_uint(loc, lhs->type, lhs->const_.integer * rhs->const_.integer);
                    case ConstExprKind::Float: return make_const_float(loc, lhs->type, lhs->const_.number * rhs->const_.number);
    
                    default: ARIA_UNREACHABLE("Invalid const expr kind");
                }
            }
    
            case BinaryOperatorKind::Div: {
                switch (lhs->const_.kind) {
                    case ConstExprKind::Int: {
                        if (lhs->type->is_signed() && rhs->type->is_signed()) {
                            return make_const_int(loc, lhs->type, static_cast<i64>(lhs->const_.integer) / static_cast<i64>(rhs->const_.integer));
                        } else if (lhs->type->is_unsigned() && rhs->type->is_unsigned()) {
                            return make_const_uint(loc, lhs->type, lhs->const_.integer / rhs->const_.integer);
                        }

                        ARIA_UNREACHABLE("Invalid type");
                    }
    
                    case ConstExprKind::Float: return make_const_float(loc, lhs->type, lhs->const_.number / rhs->const_.number);
    
                    default: ARIA_UNREACHABLE("Invalid const expr kind");
                }
            }

            case BinaryOperatorKind::Less: {
                switch (lhs->const_.kind) {
                    case ConstExprKind::Int: {
                        if (lhs->type->is_signed() && rhs->type->is_signed()) {
                            return make_const_bool(loc, TypeInfo::get_bool(), static_cast<i64>(lhs->const_.integer) < static_cast<i64>(rhs->const_.integer));
                        } else if (lhs->type->is_unsigned() && rhs->type->is_unsigned()) {
                            return make_const_bool(loc, TypeInfo::get_bool(), lhs->const_.integer < rhs->const_.integer);
                        }

                        ARIA_UNREACHABLE("Invalid type");
                    }

                    default: ARIA_UNREACHABLE("Invalid const expr kind");
                }
            }
    
            default: ARIA_UNREACHABLE("Invalid binary operator");
        }
    }

    Expr* SemanticAnalyzer::eval_const_binary_expr(Expr* expr) {
        BinaryOperatorExpr& b = expr->binary_operator;

        Expr* lhs = eval_const_expr(b.lhs);
        Expr* rhs = eval_const_expr(b.rhs);
    
        return eval_const_binary_operator(expr->loc, lhs, rhs, b.op);
    }

    Expr* SemanticAnalyzer::eval_const_compound_assign_expr(Expr* expr) {
        CompoundAssignExpr& c = expr->compound_assign;

        Expr* lhs = eval_const_expr(c.lhs);
        Expr* rhs = eval_const_expr(c.rhs);

        Expr* result = eval_const_binary_operator(expr->loc, lhs, rhs, c.op);

        switch (lhs->const_.kind) {
            case ConstExprKind::Int: {
                lhs->const_.integer = result->const_.integer;
                break;
            }

            default: ARIA_UNREACHABLE("Invalid lhs kind");
        }

        return lhs;
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
                switch (expr->decl_ref.referenced_decl->kind) {
                    case DeclKind::Var: {
                        if (expr->decl_ref.referenced_decl->var.const_var) {
                            return expr->decl_ref.referenced_decl->var.initializer;
                        }

                        auto ctx = m_inlines.get<ConstantEvaluationContext>();
                        ARIA_ASSERT(ctx, "Must be in a constant context");

                        return ctx->vars.at(expr->decl_ref.referenced_decl);
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
    
            case ExprKind::BuiltinMember: {
                Expr* parent = eval_const_expr(expr->member.parent);

                if (expr->member.member == "len") {
                    switch (parent->const_.kind) {
                        case ConstExprKind::Array: return Expr::Create(expr->loc, ExprKind::Const, ExprValueKind::RValue, expr->type, ConstExpr(ConstExprKind::Int, parent->const_.values.size));
                        
                        default: ARIA_UNREACHABLE("Unsupported member expr");
                    }
                }

                ARIA_UNREACHABLE("Invalid member");
            }

            case ExprKind::TypeMember: {
                ARIA_ASSERT(expr->type_member.referenced_member, "Invalid type member expression");
                ARIA_ASSERT(expr->type_member.referenced_member->kind == DeclKind::EnumConstant, "Invalid type member expression");
                return Expr::Create(expr->loc, ExprKind::Const, ExprValueKind::RValue, expr->type, ConstExpr(ConstExprKind::Int, expr->type_member.referenced_member->enum_constant.resolved_value));
            }
    
            case ExprKind::Construct: {
                if (expr->type->is_array()) {
                    TinyVector<Expr*> const_args;

                    for (Expr* arg : expr->construct.arguments) {
                        const_args.append(eval_const_expr(arg));
                    }

                    Expr* default_arg = get_null_value(expr->type->array.base);
                    for (size_t i = expr->construct.arguments.size; i < expr->type->array.size; i++) {
                        const_args.append(default_arg);
                    }
    
                    return Expr::Create(expr->loc, ExprKind::Const, ExprValueKind::RValue, expr->type, ConstExpr(ConstExprKind::Array, const_args));
                } else if (expr->type->is_struct()) {
                    for (Expr*& arg : expr->construct.arguments) {
                        arg = eval_const_expr(arg);
                    }
    
                    return Expr::Create(expr->loc, ExprKind::Const, ExprValueKind::RValue, expr->type, ConstExpr(ConstExprKind::Struct, expr->construct.arguments));
                }

                return eval_const_expr(expr->construct.arguments[0]);
            } 

            case ExprKind::ArraySubscript: {
                Expr* arr = eval_const_expr(expr->array_subscript.array);
                Expr* idx = eval_const_expr(expr->array_subscript.index);

                if (arr->const_.kind == ConstExprKind::Error) {
                    return Expr::Create(expr->loc, ExprKind::Const, ExprValueKind::RValue, TypeInfo::get_error(), ConstExpr(ConstExprKind::Error));
                }

                if (idx->const_.integer >= arr->const_.values.size) {
                    report_error(expr->loc, "Array subscript out of range");
                    report_note(arr->loc, fmt::format("The length of the array is {} but the index is {}", arr->const_.values.size, idx->const_.integer));

                    return Expr::Create(expr->loc, ExprKind::Const, ExprValueKind::RValue, TypeInfo::get_error(), ConstExpr(ConstExprKind::Error));
                }

                return arr->const_.values[idx->const_.integer];
            }
    
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

                    case UnaryOperatorKind::PostIncrement: {
                        Expr* prev_val = Expr::dup(val);

                        switch (val->const_.kind) {
                            case ConstExprKind::Int: {
                                val->const_.integer++;
                                return prev_val;
                            }
    
                            case ConstExprKind::Float: {
                                val->const_.number++;
                                return prev_val;
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
                                case TypeKind::Sz: return INT(CAST(size_t, val));
    
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

                    case CastKind::AnyCast: {
                        Expr* any = eval_const_expr(expr->implicit_cast.expression);

                        if (any->const_.kind == ConstExprKind::Error) {
                            return Expr::Create(expr->loc, ExprKind::Const, ExprValueKind::RValue, TypeInfo::get_error(), ConstExpr(ConstExprKind::Error));
                        }

                        if (!type_is_equal(any->const_.value->type, expr->type)) {
                            report_error(expr->loc, "Invalid any cast");
                            report_note(any->loc, fmt::format("This expression has type '{}'", any->const_.value->type->to_string()));

                            return Expr::Create(expr->loc, ExprKind::Const, ExprValueKind::RValue, TypeInfo::get_error(), ConstExpr(ConstExprKind::Error));
                        }

                        return any->const_.value;
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
    
            case ExprKind::BinaryOperator: return eval_const_binary_expr(expr);
            case ExprKind::CompoundAssign: return eval_const_compound_assign_expr(expr);
    
            default: ARIA_UNREACHABLE("Should never be reached");
        }
    }

    Expr* SemanticAnalyzer::get_null_value(TypeInfo* type) {
        switch (type->kind) {
            case TypeKind::Char:
            case TypeKind::IChar:
            case TypeKind::Short:
            case TypeKind::UShort:
            case TypeKind::Int:
            case TypeKind::UInt:
            case TypeKind::Long:
            case TypeKind::ULong:
            case TypeKind::Sz:
            case TypeKind::Isz: return Expr::Create({}, ExprKind::Const, ExprValueKind::RValue, type, ConstExpr(ConstExprKind::Int, static_cast<u64>(0)));

            default: ARIA_UNREACHABLE("Unsupported type");
        }
    }

    void SemanticAnalyzer::eval_const_var_decl(Decl* decl) {
        VarDecl& var = decl->var;

        Expr* init = nullptr;

        if (var.initializer) {
            if (!is_const_expr(var.initializer)) {
                report_error(var.initializer->loc, "This expression must be constant");
                return;
            }

            init = var.initializer;
        } else {
            init = get_null_value(var.type);
        }

        auto ctx = m_inlines.get<ConstantEvaluationContext>();
        if (!ctx) { return; }

        ctx->vars[decl] = eval_const_expr(init);
    }

    void SemanticAnalyzer::eval_const_decl(Decl* decl) {
        switch (decl->kind) {
            case DeclKind::Var: return eval_const_var_decl(decl);

            default: {
                report_error(decl->loc, "This declaration is not supported during constant evaluation");
                return;
            }
        }
    }
    
    bool SemanticAnalyzer::eval_const_compound_stmt(Stmt* stmt) {
        CompoundStmt& co = stmt->compound;
        
        bool valid = true;
        for (Stmt* s : co.stmts) {
            if (!eval_const_stmt(s)) { valid = false; }
        }

        return valid;
    }

    bool SemanticAnalyzer::eval_const_for_stmt(Stmt* stmt) {
        ForStmt& f = stmt->for_;

        if (f.prologue) {
            eval_const_decl(f.prologue);
        }

        if (f.condition && !is_const_expr(f.condition)) {
            report_error(f.condition->loc, "This expression must be constant");
            return false;
        }

        if (f.step && !is_const_expr(f.step)) {
            report_error(f.step->loc, "This expression must be constant");
            return false;
        }

        while (true) {
            if (!eval_const_compound_stmt(f.body)) {
                return false;
            }

            if (f.condition) {
                Expr* cond = eval_const_expr(f.condition);

                if (!cond->const_.boolean) {
                    break;
                }
            }

            if (f.step) { eval_const_expr(f.step); }
        }

        return true;
    }
    
    bool SemanticAnalyzer::eval_const_return_stmt(Stmt* stmt) {
        ReturnStmt& r = stmt->return_;
    
        if (!r.value) { return true; }

        auto ctx = m_inlines.get<ConstantEvaluationContext>();
        if (!ctx) { return true; }
    
        if (!is_const_expr(r.value)) {
            report_error(r.value->loc, "This expression must be constant");
            ctx->return_val = &error_expr;
            return false;
        }
    
        ctx->return_val = eval_const_expr(Expr::dup(r.value));
        return true;
    }
    
    bool SemanticAnalyzer::eval_const_stmt(Stmt* stmt) {
        switch (stmt->kind) {
            case StmtKind::Compound: return eval_const_compound_stmt(stmt);
            case StmtKind::For: return eval_const_for_stmt(stmt);
            case StmtKind::Return: return eval_const_return_stmt(stmt);
            case StmtKind::Expr: eval_const_expr(stmt->expr); return true;
            case StmtKind::Decl: eval_const_decl(stmt->decl); return true;
            
            default: {
                report_error(stmt->loc, "This statement is not supported during constant evaluation");
                return false;
            }
        }
    }

} // namespace ariac