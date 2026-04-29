#ifndef STMT_CASE
    #error "Incorrect usage of stmt_switch.hpp, define STMT_CASE before including it"
#endif

#define X(kind) case StmtKind::kind: STMT_CASE(kind); break;
switch (stmt->Kind) {
    case StmtKind::Error: break;
    X(Nop)
    X(Import)
    X(Block)
    X(While)
    X(DoWhile)
    X(For)
    X(If)
    X(Break)
    X(Continue)
    X(Return)
    X(Expr)
    X(Decl)

    default: ARIA_UNREACHABLE();
}
#undef X