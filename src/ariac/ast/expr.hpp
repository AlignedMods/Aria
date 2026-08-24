#pragma once

#include "ariac/core/vector.hpp"
#include "ariac/types/type_info.hpp"
#include "ariac/core/source_location.hpp"
#include "ariac/ast/stmt.hpp"
#include "ariac/types.hpp"
#include "ariac/enums.hpp"

#include <variant>

namespace ariac {

    struct Decl;
    struct ConstructorDecl;
    struct Specifier;

    struct ErrorExpr {
        ErrorExpr() = default;
    };

    struct BooleanLiteralExpr {
        BooleanLiteralExpr(bool value)
            : value(value) {}

        bool value = false;
    };
    
    struct CharacterLiteralExpr {
        CharacterLiteralExpr(i8 value)
            : value(value) {}

        i8 value = 0;
    };
    
    struct IntegerLiteralExpr {
        IntegerLiteralExpr(u64 value)
            : value(value) {}

        u64 value = 0;
    };
    
    struct FloatingLiteralExpr {
        FloatingLiteralExpr(double value)
            : value(value) {}

        double value = 0.0;
    };

    struct StringLiteralExpr {
        StringLiteralExpr(std::string_view value)
            : value(value) {}

        std::string_view value;
    };

    struct ArrayFillerExpr {
        ArrayFillerExpr(Expr* initializer)
            : initializer(initializer) {}

        Expr* initializer = nullptr;
    };

    struct DeclRefExpr {
        DeclRefExpr(std::string_view identifier, Specifier* specifier, TinyVector<TypeInfo*> generic_args, bool provides_generic_args)
            : identifier(identifier), name_specifier(specifier), generic_arguments(generic_args), provides_generic_args(provides_generic_args) {}

        DeclRefExpr(std::string_view identifier, Specifier* specifier, Decl* rd)
            : identifier(identifier), name_specifier(specifier), referenced_decl(rd) {}

        std::string_view identifier;
        Specifier* name_specifier = nullptr;
        Decl* referenced_decl = nullptr;
        TinyVector<TypeInfo*> generic_arguments;
        bool provides_generic_args = false;
    };

    struct TypeInfoExpr {
        TypeInfoExpr(TypeInfo* t)
            : type(t) {}

        TypeInfo* type;
    };

    struct MemberExpr {
        MemberExpr(std::string_view member, Expr* parent)
            : member(member), parent(parent) {}

        MemberExpr(std::string_view member, Expr* parent, Decl* rm)
            : member(member), parent(parent), referenced_member(rm) {}

        std::string_view member;
        Expr* parent = nullptr;
        bool implicit_deref = false;
        Decl* referenced_member = nullptr;
    };

    struct TypeMemberExpr {
        TypeMemberExpr(std::string_view member, TypeInfo* t)
            : member(member), type(t) {}

        std::string_view member;
        TypeInfo* type;
        Decl* referenced_member = nullptr; // NOTE: This could be null
    };

    struct CallExpr {
        CallExpr(Expr* callee, TinyVector<Expr*> args)
            : callee(callee), arguments(args) {}

        Decl* get_callee_decl();

        Expr* callee;
        TinyVector<Expr*> arguments;
    };

    struct BuiltinCallExpr {
        BuiltinCallExpr(BuiltinCallKind kind, TinyVector<Expr*> args)
            : kind(kind), arguments(args) {}

        BuiltinCallKind kind;
        TinyVector<Expr*> arguments;
    };

    struct ConstructExpr {
        ConstructExpr(TinyVector<Expr*> args)
            : arguments(args) {}

        TinyVector<Expr*> arguments;
        bool is_const = true;
    };

    struct ArrayLiteralExpr {
        ArrayLiteralExpr(TinyVector<Expr*> args)
            : arguments(args) {}

        TinyVector<Expr*> arguments;
        bool is_const = true;
    };

    struct ArraySubscriptExpr {
        ArraySubscriptExpr(Expr* array, Expr* index)
            : array(array), index(index) {}

        Expr* array = nullptr;
        Expr* index = nullptr;
    };

    struct ToSliceExpr {
        ToSliceExpr(Expr* source, Expr* len)
            : source(source), len(len) {}

        Expr* source = nullptr;
        Expr* len = nullptr;
    };

    // MoveExpr
    // Represents a 'moved' struct
    // A move means that the old struct is invalidated (zero initialized) and the new one is the same as the old one
    // NOTE: The subexpression must be an lvalue
    // eg.
    // let s: SomeStruct;
    // foo(s); -> 's' becomes a MoveExpr
    struct MoveExpr {
        MoveExpr(Expr* expr)
            : expression(expr) {}

        Expr* expression = nullptr;
    };

    // MaterializeTemporaryExpr
    // Wraps an rvalue expression and turns it into an lvalue by creating a local variable
    // This is mainly needed in cases where lvalues are needed but rvalues may also be allowed
    // eg.
    // fn foo() -> Foo;
    // foo().x; -> '.x' requires an lvalue but 'foo()' creates an rvalue
    struct MaterializeTemporaryExpr {
        MaterializeTemporaryExpr(Expr* expr)
            : expression(expr) {}

        Expr* expression = nullptr;
    };

    // TemporaryExpr
    // Wraps an expression along with a destructor that needs to be called
    struct TemporaryExpr {
        TemporaryExpr(Expr* expr, Decl* dtor)
            : expression(expr), dtor(dtor) {}

        Expr* expression = nullptr;
        Decl* dtor = nullptr;
    };

    // ExprWithCleanups
    // Wraps an expression that contains one or more TemporaryExpr's
    // This expression will handle the cleanup of those temporaries
    struct ExprWithCleanups {
        ExprWithCleanups(Expr* expr)
            : expression(expr) {}

        Expr* expression = nullptr;
    };

    // DefaultArgExpr
    // Represents an argument to a function call which was not explicitly written in the source code
    // AKA a default argument
    // eg.
    // fn foo(x: int = 0) {}
    // foo(); -> DefaultArgExpr for 'x' in the function call
    struct DefaultArgExpr {
        DefaultArgExpr(Expr* arg, Decl* param)
            : argument(arg), parameter(param) {}

        Expr* argument;
        Decl* parameter;
    };

    // ParenExpr
    // At its core it just wraps an expression
    // These kinds of expressions are usually from the actual source code
    // eg. 1 + (2 - 3)
    struct ParenExpr {
        ParenExpr(Expr* expr)
            :  expression(expr) {}

        Expr* expression = nullptr;
    };

    // TernaryExpr
    // Represent a ternary expression
    // eg. true ? 1 : 0;
    struct TernaryExpr {
        TernaryExpr(Expr* cond, Expr* first, Expr* second)
            : condition(cond), first(first), second(second) {}

        Expr* condition = nullptr;
        Expr* first = nullptr;
        Expr* second = nullptr;
    };

    // CastExpr
    // Represents an explicit cast in the original source code
    // This node should never represent an implicit cast, for that use ImplicitCastExpr
    // eg. int a = (int)5.5;
    struct CastExpr {
        CastExpr(Expr* expr, TypeInfo* type)
            : expression(expr), type(type) {}

        Expr* expression = nullptr;
        TypeInfo* type = nullptr;
    };

    // ImplicitCastExpr
    // Represents an implicit cast automatically inserted by the semantic analyzer
    // eg. float a = 5; -> here "5" is implicitly converted to a float
    struct ImplicitCastExpr {
        ImplicitCastExpr(Expr* expr, CastKind castKind)
            : expression(expr), kind(castKind) {}

        Expr* expression = nullptr;
        CastKind kind = CastKind::Invalid;
    };
    
    struct UnaryOperatorExpr {
        UnaryOperatorExpr(Expr* expr, UnaryOperatorKind op)
            : expression(expr), op(op) {}

        Expr* expression = nullptr;
        UnaryOperatorKind op = UnaryOperatorKind::Invalid;
    };
    
    struct BinaryOperatorExpr {
        BinaryOperatorExpr(Expr* lhs, Expr* rhs, BinaryOperatorKind op)
            : lhs(lhs), rhs(rhs), op(op) {}

        Expr* lhs = nullptr;
        Expr* rhs = nullptr;
        BinaryOperatorKind op = BinaryOperatorKind::Invalid;
    };

    // CompoundAssignExpr
    // Represents a binary operator which does both a normal binary operation (+, -, ..) and an assignment (=)
    // In code this looks like:
    // foo += bar;
    struct CompoundAssignExpr {
        CompoundAssignExpr(Expr* lhs, Expr* rhs, BinaryOperatorKind op)
            : lhs(lhs), rhs(rhs), op(op) {}

        Expr* lhs = nullptr;
        Expr* rhs = nullptr;
        BinaryOperatorKind op = BinaryOperatorKind::Invalid;
    };

    struct ConstExpr {
        ConstExpr(ConstExprKind kind)
            : kind(kind), integer() {}

        ConstExpr(ConstExprKind kind, bool b)
            : kind(kind), boolean(b) {}

        ConstExpr(ConstExprKind kind, u64 i)
            : kind(kind), integer(i) {}

        ConstExpr(ConstExprKind kind, i64 i)
            : kind(kind), integer(static_cast<u64>(i)) {}

        ConstExpr(ConstExprKind kind, double f)
            : kind(kind), number(f) {}

        ConstExpr(ConstExprKind kind, std::string_view str)
            : kind(kind), string(str) {}

        ConstExpr(ConstExprKind kind, TinyVector<Expr*> vals)
            : kind(kind), values(vals) {}

        ConstExpr(ConstExprKind kind, Expr* e)
            : kind(kind), value(e) {}

        ConstExpr(ConstExprKind kind, TypeInfo* ty)
            : kind(kind), type(ty) {}

        ConstExprKind kind = ConstExprKind::Error;
        union {
            bool boolean;
            u64 integer;
            double number;
            std::string_view string;
            TinyVector<Expr*> values;
            Expr* value;
            TypeInfo* type;
        };
    };

    struct Expr {
        template <typename T>
        static inline Expr* Create(SourceLoc loc, 
            ExprKind kind, 
            ExprValueKind value_kind, TypeInfo* type, 
            T t = ErrorExpr()) { return context.allocate<Expr>(loc, kind, value_kind, type, t); }

        static Expr* dup(Expr* e);

        ExprKind kind = ExprKind::Invalid;
        ExprValueKind value_kind = ExprValueKind::RValue;
        TypeInfo* type = nullptr;

        bool result_discarded = false;

        SourceLoc loc;

        union {
            ErrorExpr error;
            BooleanLiteralExpr boolean_literal;
            CharacterLiteralExpr character_literal;
            IntegerLiteralExpr integer_literal;
            FloatingLiteralExpr floating_literal;
            StringLiteralExpr string_literal;
            ArrayFillerExpr array_filler;
            DeclRefExpr decl_ref;
            TypeInfoExpr type_info;
            MemberExpr member;
            TypeMemberExpr type_member;
            CallExpr call;
            BuiltinCallExpr builtin_call;
            ConstructExpr construct;
            ArrayLiteralExpr array_literal;
            ArraySubscriptExpr array_subscript;
            ToSliceExpr to_slice;
            MoveExpr move;
            MaterializeTemporaryExpr materialize_temporary;
            TemporaryExpr temporary;
            ExprWithCleanups expr_with_cleanups;
            DefaultArgExpr default_arg;
            ParenExpr paren;
            TernaryExpr ternary;
            CastExpr cast;
            ImplicitCastExpr implicit_cast;
            UnaryOperatorExpr unary_operator;
            BinaryOperatorExpr binary_operator;
            CompoundAssignExpr compound_assign;
            ConstExpr const_;
        };

        Expr()
            : boolean_literal(false) {}

        Expr(SourceLoc loc, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, ErrorExpr error)
            : loc(loc), kind(kind), value_kind(value_kind), type(type), error(error) {}

        Expr(SourceLoc loc, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, BooleanLiteralExpr boolean_literal)
            : loc(loc), kind(kind), value_kind(value_kind), type(type), boolean_literal(boolean_literal) {}

        Expr(SourceLoc loc, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, CharacterLiteralExpr character_literal)
            : loc(loc), kind(kind), value_kind(value_kind), type(type), character_literal(character_literal) {}

        Expr(SourceLoc loc, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, IntegerLiteralExpr integer_literal)
            : loc(loc), kind(kind), value_kind(value_kind), type(type), integer_literal(integer_literal) {}

        Expr(SourceLoc loc, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, FloatingLiteralExpr floating_literal)
            : loc(loc), kind(kind), value_kind(value_kind), type(type), floating_literal(floating_literal) {}

        Expr(SourceLoc loc, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, StringLiteralExpr string_literal)
            : loc(loc), kind(kind), value_kind(value_kind), type(type), string_literal(string_literal) {}

        Expr(SourceLoc loc, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, ArrayFillerExpr array_filler)
            : loc(loc), kind(kind), value_kind(value_kind), type(type), array_filler(array_filler) {}

        Expr(SourceLoc loc, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, DeclRefExpr decl_ref)
            : loc(loc), kind(kind), value_kind(value_kind), type(type), decl_ref(decl_ref) {}

        Expr(SourceLoc loc, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, TypeInfoExpr t)
            : loc(loc), kind(kind), value_kind(value_kind), type(type), type_info(t) {}

        Expr(SourceLoc loc, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, MemberExpr member)
            : loc(loc), kind(kind), value_kind(value_kind), type(type), member(member) {}

        Expr(SourceLoc loc, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, TypeMemberExpr member)
            : loc(loc), kind(kind), value_kind(value_kind), type(type), type_member(member) {}

        Expr(SourceLoc loc, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, CallExpr call)
            : loc(loc), kind(kind), value_kind(value_kind), type(type), call(call) {}

        Expr(SourceLoc loc, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, BuiltinCallExpr call)
            : loc(loc), kind(kind), value_kind(value_kind), type(type), builtin_call(call) {}

        Expr(SourceLoc loc, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, ConstructExpr construct)
            : loc(loc), kind(kind), value_kind(value_kind), type(type), construct(construct) {}

        Expr(SourceLoc loc, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, ArrayLiteralExpr lit)
            : loc(loc), kind(kind), value_kind(value_kind), type(type), array_literal(lit) {}

        Expr(SourceLoc loc, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, ArraySubscriptExpr arr_subs)
            : loc(loc), kind(kind), value_kind(value_kind), type(type), array_subscript(arr_subs) {}

        Expr(SourceLoc loc, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, ToSliceExpr to_slice)
            : loc(loc), kind(kind), value_kind(value_kind), type(type), to_slice(to_slice) {}

        Expr(SourceLoc loc, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, MoveExpr move)
            : loc(loc), kind(kind), value_kind(value_kind), type(type), move(move) {}

        Expr(SourceLoc loc, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, TemporaryExpr temp)
            : loc(loc), kind(kind), value_kind(value_kind), type(type), temporary(temp) {}

        Expr(SourceLoc loc, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, MaterializeTemporaryExpr temp)
            : loc(loc), kind(kind), value_kind(value_kind), type(type), materialize_temporary(temp) {}

        Expr(SourceLoc loc, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, ExprWithCleanups ewc)
            : loc(loc), kind(kind), value_kind(value_kind), type(type), expr_with_cleanups(ewc) {}

        Expr(SourceLoc loc, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, DefaultArgExpr da)
            : loc(loc), kind(kind), value_kind(value_kind), type(type), default_arg(da) {}

        Expr(SourceLoc loc, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, ParenExpr paren)
            : loc(loc), kind(kind), value_kind(value_kind), type(type), paren(paren) {}

        Expr(SourceLoc loc, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, TernaryExpr ternary)
            : loc(loc), kind(kind), value_kind(value_kind), type(type), ternary(ternary) {}

        Expr(SourceLoc loc, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, CastExpr cast)
            : loc(loc), kind(kind), value_kind(value_kind), type(type), cast(cast) {}

        Expr(SourceLoc loc, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, ImplicitCastExpr implicit_cast)
            : loc(loc), kind(kind), value_kind(value_kind), type(type), implicit_cast(implicit_cast) {}

        Expr(SourceLoc loc, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, UnaryOperatorExpr unary_operator)
            : loc(loc), kind(kind), value_kind(value_kind), type(type), unary_operator(unary_operator) {}

        Expr(SourceLoc loc, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, BinaryOperatorExpr binary_operator)
            : loc(loc), kind(kind), value_kind(value_kind), type(type), binary_operator(binary_operator) {}

        Expr(SourceLoc loc, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, CompoundAssignExpr compound_assign)
            : loc(loc), kind(kind), value_kind(value_kind), type(type), compound_assign(compound_assign) {}

        Expr(SourceLoc loc, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, ConstExpr const_)
            : loc(loc), kind(kind), value_kind(value_kind), type(type), const_(const_) {}

        inline bool is_lvalue() const { return value_kind == ExprValueKind::LValue; }
        inline bool is_rvalue() const { return value_kind == ExprValueKind::RValue; }
        inline bool is_xvalue() const { return value_kind == ExprValueKind::XValue; }
        inline bool is_lxvalue() const { return value_kind != ExprValueKind::RValue; }
    };

    inline Expr error_expr = Expr(SourceLoc(), ExprKind::Error, ExprValueKind::RValue, nullptr, ErrorExpr());

} // namespace ariac
