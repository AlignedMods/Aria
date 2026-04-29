#ifndef DECL_CASE
    #error "Incorrect usage of decl_switch.hpp, define DECL_CASE before including it"
#endif

#define X(kind) case DeclKind::kind: DECL_CASE(kind); break;
switch (decl->Kind) {
    case DeclKind::Error: break;
    X(TranslationUnit)
    X(Module)
    X(Var)
    X(Param)
    X(Function)
    X(OverloadedFunction)
    X(Struct)
    X(Field)
    X(Constructor)
    X(Destructor)
    X(Method)
    X(BuiltinCopyConstructor)
    X(BuiltinDestructor)

    default: ARIA_UNREACHABLE();
}
#undef X