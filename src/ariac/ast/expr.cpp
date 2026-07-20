#include "ariac/ast/expr.hpp"

namespace ariac {

    Expr* Expr::dup(Expr* e) {
        Expr* copy = Expr::Create(e->loc, e->kind, e->value_kind, TypeInfo::dup(e->type), ErrorExpr());

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

            case ExprKind::TypeInfo: break;

            case ExprKind::Member:
            case ExprKind::BuiltinMember: {
                MemberExpr& m = e->member;
                copy->member.member = m.member;
                copy->member.parent = Expr::dup(m.parent);
                copy->member.implicit_deref = m.implicit_deref;
                copy->member.referenced_member = m.referenced_member;
                break;
            }

            case ExprKind::Self: break;

            case ExprKind::Call: {
                CallExpr& c = e->call;
                copy->call.callee = Expr::dup(c.callee);

                for (Expr* arg : c.arguments) {
                    copy->call.arguments.append(Expr::dup(arg));
                }

                for (TypeInfo* arg : c.generic_arguments) {
                    copy->call.generic_arguments.append(TypeInfo::dup(arg));
                }
                break;
            }

            case ExprKind::BuiltinCall: {
                BuiltinCallExpr& b = e->builtin_call;
                copy->builtin_call.kind = b.kind;

                for (Expr* arg : b.arguments) {
                    copy->builtin_call.arguments.append(Expr::dup(arg));
                }
                break;
            }

            case ExprKind::ArrayLiteral: {
                ArrayLiteralExpr& lit = e->array_literal;

                for (Expr* arg : lit.arguments) {
                    copy->array_literal.arguments.append(Expr::dup(arg));
                }

                copy->array_literal.is_const = lit.is_const;

                break;
            }

            case ExprKind::MethodCall: {
                CallExpr& m = e->call;
                
                for (Expr* arg : m.arguments) {
                    copy->call.arguments.append(Expr::dup(arg));
                }

                for (TypeInfo* arg : m.generic_arguments) {
                    copy->call.generic_arguments.append(TypeInfo::dup(arg));
                }

                copy->call.callee = Expr::dup(m.callee);
                break;
            }

            case ExprKind::ArraySubscript: {
                ArraySubscriptExpr& a = e->array_subscript;
                copy->array_subscript.array = Expr::dup(a.array);
                copy->array_subscript.index = Expr::dup(a.index);
                break;
            }

            case ExprKind::ToSlice: {
                ToSliceExpr& t = e->to_slice;
                if (t.len) copy->to_slice.len = Expr::dup(t.len);
                copy->to_slice.source = Expr::dup(t.source);
                break;
            }

            case ExprKind::Paren: {
                ParenExpr& p = e->paren;
                copy->paren.expression = Expr::dup(p.expression);
                break;
            }

            case ExprKind::Cast: {
                CastExpr& c = e->cast;
                copy->cast.expression = Expr::dup(c.expression);
                copy->cast.type = TypeInfo::dup(c.type);
                break;
            }

            case ExprKind::ImplicitCast: {
                ImplicitCastExpr& i = e->implicit_cast;
                copy->implicit_cast.expression = Expr::dup(i.expression);
                copy->implicit_cast.kind = i.kind;
                break;
            }

            case ExprKind::UnaryOperator: {
                UnaryOperatorExpr& u = e->unary_operator;
                copy->unary_operator.expression = Expr::dup(u.expression);
                copy->unary_operator.infix = u.infix;
                copy->unary_operator.op = u.op;
                break;
            }

            case ExprKind::BinaryOperator: {
                BinaryOperatorExpr& b = e->binary_operator;
                copy->binary_operator.lhs = Expr::dup(b.lhs);
                copy->binary_operator.rhs = Expr::dup(b.rhs);
                copy->binary_operator.op = b.op;
                break;
            }

            case ExprKind::CompoundAssign: {
                CompoundAssignExpr& c = e->compound_assign;
                copy->compound_assign.lhs = Expr::dup(c.lhs);
                copy->compound_assign.rhs = Expr::dup(c.rhs);
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
                            copy->const_.values.append(Expr::dup(e));
                        }
                        break;
                    }

                    default: ARIA_UNREACHABLE("Invalid const expr kind");
                }

                break;
            }

            default: ARIA_UNREACHABLE("Invalid expr kind");
        }

        return copy;
    }

} // namespace ariac