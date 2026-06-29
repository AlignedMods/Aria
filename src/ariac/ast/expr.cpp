#include "ariac/ast/expr.hpp"

namespace ariac {

    Expr* Expr::dup(CompilationContext* ctx, Expr* e) {
        Expr* copy = Expr::Create(ctx, e->loc, e->kind, e->value_kind, TypeInfo::dup(ctx, e->type), ErrorExpr());

        switch (e->kind) {
            case ExprKind::Error:

            case ExprKind::BooleanLiteral: {
                BooleanLiteralExpr& b = e->boolean_literal;
                copy->boolean_literal.value = b.value;
                break;
            }

            case ExprKind::CharacterLiteral: {
                CharacterLiteralExpr& c = e->character_literal;
                copy->character_literal.value = c.value;
                break;
            }

            case ExprKind::IntegerLiteral: {
                IntegerLiteralExpr& i = e->integer_literal;
                copy->integer_literal.value = i.value;
                break;
            }

            case ExprKind::FloatingLiteral: {
                FloatingLiteralExpr& f = e->floating_literal;
                copy->floating_literal.value = f.value;
                break;
            }

            case ExprKind::StringLiteral: {
                StringLiteralExpr& s = e->string_literal;
                copy->string_literal.value = s.value;
                break;
            }

            case ExprKind::Null: break;

            case ExprKind::DeclRef: {
                DeclRefExpr& d = e->decl_ref;
                copy->decl_ref.identifier = d.identifier;
                copy->decl_ref.name_specifier = d.name_specifier;
                copy->decl_ref.referenced_decl = d.referenced_decl;
                break;
            }

            case ExprKind::Member:
            case ExprKind::BuiltinMember: {
                MemberExpr& m = e->member;
                copy->member.member = m.member;
                copy->member.parent = Expr::dup(ctx, m.parent);
                copy->member.implicit_deref = m.implicit_deref;
                copy->member.referenced_member = m.referenced_member;
                break;
            }

            case ExprKind::Self: break;

            case ExprKind::Call: {
                CallExpr& c = e->call;
                
                for (Expr* arg : c.arguments) {
                    copy->call.arguments.append(ctx, Expr::dup(ctx, arg));
                }

                copy->call.callee = Expr::dup(ctx, c.callee);

                break;
            }

            case ExprKind::MethodCall: {
                MethodCallExpr& m = e->method_call;
                
                for (Expr* arg : m.arguments) {
                    copy->method_call.arguments.append(ctx, Expr::dup(ctx, arg));
                }

                copy->method_call.callee = Expr::dup(ctx, m.callee);

                break;
            }

            case ExprKind::ArraySubscript: {
                ArraySubscriptExpr& a = e->array_subscript;
                copy->array_subscript.array = Expr::dup(ctx, a.array);
                copy->array_subscript.index = Expr::dup(ctx, a.index);
                break;
            }

            case ExprKind::New: {
                NewExpr& n = e->new_;
                copy->new_.initializer = Expr::dup(ctx, n.initializer);
                copy->new_.array = n.array;
                break;
            }

            case ExprKind::Delete: {
                DeleteExpr& d = e->delete_;
                copy->delete_.expression = Expr::dup(ctx, d.expression);
                break;
            }

            case ExprKind::Sizeof: {
                SizeofExpr& s = e->sizeof_;
                if (s.expression) { copy->sizeof_.expression = Expr::dup(ctx, s.expression); }
                if (s.type) { copy->sizeof_.type = TypeInfo::dup(ctx, s.type); }
                break;
            }

            case ExprKind::Paren: {
                ParenExpr& p = e->paren;
                copy->paren.expression = Expr::dup(ctx, p.expression);
                break;
            }

            case ExprKind::Cast: {
                CastExpr& c = e->cast;
                copy->cast.expression = Expr::dup(ctx, c.expression);
                copy->type = TypeInfo::dup(ctx, c.type);
                break;
            }

            case ExprKind::ImplicitCast: {
                ImplicitCastExpr& i = e->implicit_cast;
                copy->implicit_cast.expression = Expr::dup(ctx, i.expression);
                copy->implicit_cast.kind = i.kind;
                break;
            }

            case ExprKind::UnaryOperator: {
                UnaryOperatorExpr& u = e->unary_operator;
                copy->unary_operator.expression = Expr::dup(ctx, u.expression);
                copy->unary_operator.infix = u.infix;
                copy->unary_operator.op = u.op;
                break;
            }

            case ExprKind::BinaryOperator: {
                BinaryOperatorExpr& b = e->binary_operator;
                copy->binary_operator.lhs = Expr::dup(ctx, b.lhs);
                copy->binary_operator.rhs = Expr::dup(ctx, b.rhs);
                copy->binary_operator.op = b.op;
                break;
            }

            case ExprKind::CompoundAssign: {
                CompoundAssignExpr& c = e->compound_assign;
                copy->compound_assign.lhs = Expr::dup(ctx, c.lhs);
                copy->compound_assign.rhs = Expr::dup(ctx, c.rhs);
                copy->compound_assign.op = c.op;
                break;
            }

            case ExprKind::Const: {
                ConstExpr& c = e->const_;
                copy->const_.kind = c.kind;

                switch (c.kind) {
                    case ConstExprKind::Error: break;

                    case ConstExprKind::Boolean: {
                        copy->const_.boolean = c.boolean;
                        break;
                    }

                    case ConstExprKind::Integer: {
                        copy->const_.integer = c.integer;
                        break;
                    }

                    case ConstExprKind::Floating: {
                        copy->const_.number = c.number;
                        break;
                    }

                    case ConstExprKind::String: {
                        copy->const_.string = c.string;
                        break;
                    }

                    case ConstExprKind::Struct: {
                        for (Expr* e : c.values) {
                            copy->const_.values.append(ctx, Expr::dup(ctx, e));
                        }
                        break;
                    }

                    default: ARIA_UNREACHABLE();
                }

                break;
            }

            default: ARIA_UNREACHABLE();
        }

        return copy;
    }

} // namespace ariac