#ifndef EXPR_CASE
    #error "Incorrect usage of expr_switch.hpp, define EXPR_CASE before including it"
#endif

#define X(kind) case ExprKind::kind: EXPR_CASE(kind); break;
switch (expr->Kind) {
    case ExprKind::Error: break;
    X(BooleanConstant)
    X(CharacterConstant)
    X(IntegerConstant)
    X(FloatingConstant)
    X(StringConstant)
    X(Null)
    X(DeclRef)
    X(Member)
    X(Temporary)
    X(Copy)
    X(Call)
    X(Construct)
    X(MethodCall)
    X(ArraySubscript)
    X(ToSlice)
    X(New)
    X(Delete)
    X(Format)
    X(Paren)
    X(Cast)
    X(ImplicitCast)
    X(UnaryOperator)
    X(BinaryOperator)
    X(CompoundAssign)

    default: ARIA_UNREACHABLE();
}
#undef X