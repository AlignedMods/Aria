#pragma once

#include "ariac/core/vector.hpp"
#include "ariac/types/type_info.hpp"
#include "ariac/core/source_location.hpp"
#include "ariac/ast/stmt.hpp"
#include "common/types.hpp"

#include <variant>

namespace Aria::Internal {

    enum class ExprKind {
        Invalid = 0,

        Error,
        BooleanLiteral,
        CharacterLiteral,
        IntegerLiteral,
        FloatingLiteral,
        StringLiteral,
        Null,
        DeclRef,
        Member,
        BuiltinMember,
        Self,
        Temporary,
        Copy,
        Call,
        Construct,
        MethodCall,
        ArraySubscript,
        ToSlice,
        New,
        Delete,
        Format,
        Paren,
        Cast,
        ImplicitCast,
        UnaryOperator,
        BinaryOperator,
        CompoundAssign
    };

    enum class CastKind {
        Invalid,
        Integral,
        Floating,
        IntegralToFloating,
        FloatingToIntegral,
        BitCast,
        ArrayToSlice,

        LValueToRValue
    };
    inline const char* cast_kind_to_string(CastKind kind) {
        switch (kind) {
            case CastKind::Invalid: return "Invalid";
            case CastKind::Integral: return "Integral";
            case CastKind::Floating: return "Floating";
            case CastKind::IntegralToFloating: return "IntegralToFloating";
            case CastKind::FloatingToIntegral: return "FloatingToIntegral";
            case CastKind::BitCast: return "BitCast";
            case CastKind::ArrayToSlice: return "ArrayToSlice";
            case CastKind::LValueToRValue: return "LValueToRValue";
            default: ARIA_ASSERT(false, "Unreachable");
        }
    }

    enum class UnaryOperatorKind {
        Invalid,
    
        Not, // "!"
        Negate, // "-8.7f"
        AddressOf, // "&x"
        Dereference // "*ptr"
    };
    inline const char* unary_op_kind_to_string(UnaryOperatorKind kind) {
        switch (kind) {
            case UnaryOperatorKind::Invalid: return "invalid";
            
            case UnaryOperatorKind::Not: return "!";
            case UnaryOperatorKind::Negate: return "-";
            case UnaryOperatorKind::AddressOf: return "&";
            case UnaryOperatorKind::Dereference: return "*";

            default: ARIA_UNREACHABLE();
        }
    }

    enum class BinaryOperatorKind {
        Invalid,
        Add, CompoundAdd,
        Sub, CompoundSub,
        Mul, CompoundMul,
        Div, CompoundDiv,
        Mod, CompoundMod,
    
        Less,
        LessOrEq,
        Greater,
        GreaterOrEq,

        BitAnd, CompoundBitAnd, LogAnd,
        BitOr,  CompoundBitOr,  LogOr,
        BitXor, CompoundBitXor,
        Shl, CompoundShl,
        Shr, CompoundShr,
    
        Eq,
        IsEq,
        IsNotEq,
    };
    inline const char* binary_op_kind_to_string(BinaryOperatorKind kind) {
        switch (kind) {
            case BinaryOperatorKind::Invalid: return "invalid";
            case BinaryOperatorKind::Add: return "+";
            case BinaryOperatorKind::CompoundAdd: return "+=";
            case BinaryOperatorKind::Sub: return "-";
            case BinaryOperatorKind::CompoundSub: return "-=";
            case BinaryOperatorKind::Mul: return "*";
            case BinaryOperatorKind::CompoundMul: return "*=";
            case BinaryOperatorKind::Div: return "/";
            case BinaryOperatorKind::CompoundDiv: return "/=";
            case BinaryOperatorKind::Mod: return "%";
            case BinaryOperatorKind::CompoundMod: return "%=";

            case BinaryOperatorKind::Less: return "<";
            case BinaryOperatorKind::LessOrEq: return "<=";
            case BinaryOperatorKind::Greater: return ">";
            case BinaryOperatorKind::GreaterOrEq: return ">=";

            case BinaryOperatorKind::BitAnd: return "&";
            case BinaryOperatorKind::CompoundBitAnd: return "&=";
            case BinaryOperatorKind::LogAnd: return "&&";
            case BinaryOperatorKind::BitOr: return "|";
            case BinaryOperatorKind::CompoundBitOr: return "|=";
            case BinaryOperatorKind::LogOr: return "||";
            case BinaryOperatorKind::BitXor: return "^";
            case BinaryOperatorKind::CompoundBitXor: return "^=";
            case BinaryOperatorKind::Shl: return "<<";
            case BinaryOperatorKind::CompoundShl: return "<<=";
            case BinaryOperatorKind::Shr: return ">>";
            case BinaryOperatorKind::CompoundShr: return ">>=";

            case BinaryOperatorKind::Eq: return "=";
            case BinaryOperatorKind::IsEq: return "==";
            case BinaryOperatorKind::IsNotEq: return "!=";

            default: ARIA_UNREACHABLE();
        }
    }

    enum class ExprValueKind {
        LValue,
        RValue
    };
    inline const char* expr_value_kind_to_string(ExprValueKind type) {
        switch (type) {
            case ExprValueKind::LValue: return "lvalue";
            case ExprValueKind::RValue: return "rvalue";

            default: ARIA_UNREACHABLE();
        }
    }

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
        FloatingLiteralExpr(f64 value)
            : value(value) {}

        f64 value = 0.0;
    };

    struct StringLiteralExpr {
        StringLiteralExpr(std::string_view value)
            : value(value) {}

        std::string_view value;
    };

    struct DeclRefExpr {
        DeclRefExpr(std::string_view identifier, Specifier* specifier)
            : identifier(identifier), name_specifier(specifier) {}

        std::string_view identifier;
        Specifier* name_specifier = nullptr;
        Decl* referenced_decl = nullptr;
    };

    struct MemberExpr {
        MemberExpr(std::string_view member, Expr* parent)
            : member(member), parent(parent) {}

        std::string_view member;
        Expr* parent = nullptr;
    };

    // TemporaryExpr
    // Represents a temporary expression
    // eg. print("Hello world");
    // Here "Hello world" will call a constructor that allocates memory and therefore its destructor needs to be called
    struct TemporaryExpr {
        TemporaryExpr(Expr* expr, Decl* destructor)
            : expression(expr), destructor(destructor) {}

        Expr* expression = nullptr;
        Decl* destructor = nullptr;
    };

    struct CopyExpr {
        CopyExpr(Expr* expr, Decl* constructor)
            : expression(expr), constructor(constructor) {}

        Expr* expression = nullptr;
        Decl* constructor = nullptr;
    };

    struct CallExpr {
        CallExpr(Expr* callee, TinyVector<Expr*> args)
            : callee(callee), arguments(args) {}

        Expr* callee;
        TinyVector<Expr*> arguments;
    };

    struct ConstructExpr {
        ConstructExpr(ConstructorDecl* ctor, TinyVector<Expr*> args)
            : ctor(ctor), arguments(args) {}

        ConstructorDecl* ctor = nullptr;
        TinyVector<Expr*> arguments;
    };

    struct MethodCallExpr {
        MethodCallExpr(Expr* callee, TinyVector<Expr*> args)
            : callee(callee), arguments(args) {}
    
        Expr* callee;
        TinyVector<Expr*> arguments;
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

    struct NewExpr {
        NewExpr(Expr* initializer, bool array)
            : initializer(initializer), array(array) {}

        Expr* initializer = nullptr;
        bool array = false;
    };

    struct DeleteExpr {
        DeleteExpr(Expr* expr)
            : expression(expr) {}

        Expr* expression = nullptr;
    };
    
    struct FormatExpr {
        struct FormatArg {
            Expr* arg = nullptr;
        };

        FormatExpr(TinyVector<Expr*> args)
            : args(args) {}

        TinyVector<Expr*> args;
        TinyVector<FormatArg> resolved_args; // Properly ordered args, ordered during semantic analysis
                                             // eg. $format("Hello, {}, Nice to meet you", name); ->
                                             // "Hello, " name, ", Nice to meet you"
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

    // CastExpr
    // Represents an explicit cast in the original source code
    // This node should never represent an implicit cast, for that use ImplicitCastExpr
    // eg. int a = <int>5.5;
    struct CastExpr {
        CastExpr(Expr* expr, TypeInfo* type)
            : expression(expr), type(type) {}

        Expr* expression = nullptr;
        TypeInfo* type = nullptr;
    };

    // ImplicitCastExpr
    // Represents an implicit cast automatically inserted by the type checker/semantic analyzer
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

    struct Expr {
        template <typename T>
        static inline Expr* Create(CompilationContext* ctx, 
            SourceLocation loc, SourceRange range,
            ExprKind kind, 
            ExprValueKind value_kind, TypeInfo* type, 
            T t = ErrorExpr()) { return ctx->allocate<Expr>(loc, range, kind, value_kind, type, t); }

        static inline Expr* Dup(CompilationContext* ctx, Expr* other) {
            Expr* newExpr = ctx->allocate<Expr>();
            memcpy(reinterpret_cast<void*>(newExpr), other, sizeof(Expr));
            return newExpr;
        }

        ExprKind kind = ExprKind::Invalid;
        ExprValueKind value_kind = ExprValueKind::RValue;
        TypeInfo* type = nullptr;

        bool result_discarded = false;

        SourceLocation loc;
        SourceRange range;

        union {
            ErrorExpr error;
            BooleanLiteralExpr boolean_literal;
            CharacterLiteralExpr character_literal;
            IntegerLiteralExpr integer_literal;
            FloatingLiteralExpr floating_literal;
            StringLiteralExpr string_literal;
            DeclRefExpr decl_ref;
            MemberExpr member;
            TemporaryExpr temporary;
            CopyExpr copy;
            CallExpr call;
            ConstructExpr construct;
            MethodCallExpr method_call;
            ArraySubscriptExpr array_subscript;
            ToSliceExpr to_slice;
            NewExpr new_;
            DeleteExpr delete_;
            FormatExpr format;
            ParenExpr paren;
            CastExpr cast;
            ImplicitCastExpr implicit_cast;
            UnaryOperatorExpr unary_operator;
            BinaryOperatorExpr binary_operator;
            CompoundAssignExpr compound_assign;
        };

        Expr()
            : boolean_literal(false) {}

        Expr(SourceLocation loc, SourceRange range, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, ErrorExpr error)
            : loc(loc), range(range), kind(kind), value_kind(value_kind), type(type), error(error) {}

        Expr(SourceLocation loc, SourceRange range, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, BooleanLiteralExpr boolean_literal)
            : loc(loc), range(range), kind(kind), value_kind(value_kind), type(type), boolean_literal(boolean_literal) {}

        Expr(SourceLocation loc, SourceRange range, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, CharacterLiteralExpr character_literal)
            : loc(loc), range(range), kind(kind), value_kind(value_kind), type(type), character_literal(character_literal) {}

        Expr(SourceLocation loc, SourceRange range, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, IntegerLiteralExpr integer_literal)
            : loc(loc), range(range), kind(kind), value_kind(value_kind), type(type), integer_literal(integer_literal) {}

        Expr(SourceLocation loc, SourceRange range, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, FloatingLiteralExpr floating_literal)
            : loc(loc), range(range), kind(kind), value_kind(value_kind), type(type), floating_literal(floating_literal) {}

        Expr(SourceLocation loc, SourceRange range, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, StringLiteralExpr string_literal)
            : loc(loc), range(range), kind(kind), value_kind(value_kind), type(type), string_literal(string_literal) {}

        Expr(SourceLocation loc, SourceRange range, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, DeclRefExpr decl_ref)
            : loc(loc), range(range), kind(kind), value_kind(value_kind), type(type), decl_ref(decl_ref) {}

        Expr(SourceLocation loc, SourceRange range, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, MemberExpr member)
            : loc(loc), range(range), kind(kind), value_kind(value_kind), type(type), member(member) {}

        Expr(SourceLocation loc, SourceRange range, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, TemporaryExpr temporary)
            : loc(loc), range(range), kind(kind), value_kind(value_kind), type(type), temporary(temporary) {}

        Expr(SourceLocation loc, SourceRange range, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, CopyExpr copy)
            : loc(loc), range(range), kind(kind), value_kind(value_kind), type(type), copy(copy) {}

        Expr(SourceLocation loc, SourceRange range, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, CallExpr call)
            : loc(loc), range(range), kind(kind), value_kind(value_kind), type(type), call(call) {}

        Expr(SourceLocation loc, SourceRange range, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, ConstructExpr construct)
            : loc(loc), range(range), kind(kind), value_kind(value_kind), type(type), construct(construct) {}

        Expr(SourceLocation loc, SourceRange range, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, MethodCallExpr method_call)
            : loc(loc), range(range), kind(kind), value_kind(value_kind), type(type), method_call(method_call) {}

        Expr(SourceLocation loc, SourceRange range, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, ArraySubscriptExpr arr_subs)
            : loc(loc), range(range), kind(kind), value_kind(value_kind), type(type), array_subscript(arr_subs) {}

        Expr(SourceLocation loc, SourceRange range, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, ToSliceExpr to_slice)
            : loc(loc), range(range), kind(kind), value_kind(value_kind), type(type), to_slice(to_slice) {}

        Expr(SourceLocation loc, SourceRange range, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, NewExpr new_)
            : loc(loc), range(range), kind(kind), value_kind(value_kind), type(type), new_(new_) {}

        Expr(SourceLocation loc, SourceRange range, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, DeleteExpr delete_)
            : loc(loc), range(range), kind(kind), value_kind(value_kind), type(type), delete_(delete_) {}

        Expr(SourceLocation loc, SourceRange range, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, FormatExpr format)
            : loc(loc), range(range), kind(kind), value_kind(value_kind), type(type), format(format) {}

        Expr(SourceLocation loc, SourceRange range, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, ParenExpr paren)
            : loc(loc), range(range), kind(kind), value_kind(value_kind), type(type), paren(paren) {}

        Expr(SourceLocation loc, SourceRange range, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, CastExpr cast)
            : loc(loc), range(range), kind(kind), value_kind(value_kind), type(type), cast(cast) {}

        Expr(SourceLocation loc, SourceRange range, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, ImplicitCastExpr implicit_cast)
            : loc(loc), range(range), kind(kind), value_kind(value_kind), type(type), implicit_cast(implicit_cast) {}

        Expr(SourceLocation loc, SourceRange range, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, UnaryOperatorExpr unary_operator)
            : loc(loc), range(range), kind(kind), value_kind(value_kind), type(type), unary_operator(unary_operator) {}

        Expr(SourceLocation loc, SourceRange range, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, BinaryOperatorExpr binary_operator)
            : loc(loc), range(range), kind(kind), value_kind(value_kind), type(type), binary_operator(binary_operator) {}

        Expr(SourceLocation loc, SourceRange range, ExprKind kind, ExprValueKind value_kind, TypeInfo* type, CompoundAssignExpr compound_assign)
            : loc(loc), range(range), kind(kind), value_kind(value_kind), type(type), compound_assign(compound_assign) {}
    };

    inline Expr error_expr = Expr(SourceLocation(), SourceRange(), ExprKind::Error, ExprValueKind::RValue, nullptr, ErrorExpr());

} // namespace Aria::Internal
