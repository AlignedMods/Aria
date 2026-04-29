#pragma once

#include "aria/internal/compiler/core/vector.hpp"
#include "aria/internal/compiler/core/string_view.hpp"
#include "aria/internal/compiler/types/type_info.hpp"
#include "aria/internal/compiler/core/source_location.hpp"
#include "aria/internal/compiler/ast/stmt.hpp"
#include "aria/internal/types.hpp"

#include <variant>

namespace Aria::Internal {

    enum class ExprKind {
        Invalid = 0,

        Error,
        BooleanConstant,
        CharacterConstant,
        IntegerConstant,
        FloatingConstant,
        StringConstant,
        Null,
        DeclRef,
        Member,
        Self,
        Temporary,
        Copy,
        Call,
        Construct,
        MethodCall,
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
    inline const char* CastKindToString(CastKind kind) {
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
    inline const char* UnaryOperatorKindToString(UnaryOperatorKind kind) {
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
    inline const char* BinaryOperatorKindToString(BinaryOperatorKind kind) {
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
    inline const char* ExprValueKindToString(ExprValueKind type) {
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

    struct BooleanConstantExpr {
        BooleanConstantExpr(bool value)
            : Value(value) {}

        bool Value = false;
    };
    
    struct CharacterConstantExpr {
        CharacterConstantExpr(i8 value)
            : Value(value) {}

        i8 Value = 0;
    };
    
    struct IntegerConstantExpr {
        IntegerConstantExpr(u64 value)
            : Value(value) {}

        u64 Value = 0;
    };
    
    struct FloatingConstantExpr {
        FloatingConstantExpr(f64 value)
            : Value(value) {}

        f64 Value = 0.0;
    };

    struct StringConstantExpr {
        StringConstantExpr(StringView value)
            : Value(value) {}

        StringView Value;
    };

    struct DeclRefExpr {
        DeclRefExpr(StringView identifier, Specifier* specifier)
            : Identifier(identifier), NameSpecifier(specifier) {}

        StringView Identifier;
        Specifier* NameSpecifier = nullptr;
        Decl* ReferencedDecl = nullptr;
    };

    struct MemberExpr {
        MemberExpr(StringView member, Expr* parent)
            : Member(member), Parent(parent) {}

        StringView Member;
        Expr* Parent = nullptr;
    };

    // TemporaryExpr
    // Represents a temporary expression
    // eg. print("Hello world");
    // Here "Hello world" will call a constructor that allocates memory and therefore its destructor needs to be called
    struct TemporaryExpr {
        TemporaryExpr(Expr* expr, Decl* destructor)
            : Expression(expr), Destructor(destructor) {}

        Expr* Expression = nullptr;
        Decl* Destructor = nullptr;
    };

    struct CopyExpr {
        CopyExpr(Expr* expr, Decl* constructor)
            : Expression(expr), Constructor(constructor) {}

        Expr* Expression = nullptr;
        Decl* Constructor = nullptr;
    };

    struct CallExpr {
        CallExpr(Expr* callee, TinyVector<Expr*> args)
            : Callee(callee), Arguments(args) {}

        Expr* Callee;
        TinyVector<Expr*> Arguments;
    };

    struct ConstructExpr {
        ConstructExpr(ConstructorDecl* ctor, TinyVector<Expr*> args)
            : Ctor(ctor), Arguments(args) {}

        ConstructorDecl* Ctor = nullptr;
        TinyVector<Expr*> Arguments;
    };

    struct MethodCallExpr {
        MethodCallExpr(Expr* callee, TinyVector<Expr*> args)
            : Callee(callee), Arguments(args) {}
    
        Expr* Callee;
        TinyVector<Expr*> Arguments;
    };
    
    struct FormatExpr {
        struct FormatArg {
            Expr* Arg = nullptr;
        };

        FormatExpr(TinyVector<Expr*> args)
            : Args(args) {}

        TinyVector<Expr*> Args;
        TinyVector<FormatArg> ResolvedArgs; // Properly ordered args, ordered during semantic analysis
                                            // eg. $format("Hello, {}, Nice to meet you", name); ->
                                            // "Hello, " name, ", Nice to meet you"
    };

    // ParenExpr
    // At its core it just wraps an expression
    // These kinds of expressions are usually from the actual source code
    // eg. 1 + (2 - 3)
    struct ParenExpr {
        ParenExpr(Expr* expr)
            :  Expression(expr) {}

        Expr* Expression = nullptr;
    };

    // CastExpr
    // Represents an explicit cast in the original source code
    // This node should never represent an implicit cast, for that use ImplicitCastExpr
    // eg. int a = <int>5.5;
    struct CastExpr {
        CastExpr(Expr* expr, TypeInfo* type)
            : Expression(expr), Type(type) {}

        Expr* Expression = nullptr;
        TypeInfo* Type = nullptr;
    };

    // ImplicitCastExpr
    // Represents an implicit cast automatically inserted by the type checker/semantic analyzer
    // eg. float a = 5; -> here "5" is implicitly converted to a float
    struct ImplicitCastExpr {
        ImplicitCastExpr(Expr* expr, CastKind castKind)
            : Expression(expr), Kind(castKind) {}

        Expr* Expression = nullptr;
        CastKind Kind = CastKind::Invalid;
    };
    
    struct UnaryOperatorExpr {
        UnaryOperatorExpr(Expr* expr, UnaryOperatorKind op)
            : Expression(expr), Operator(op) {}

        Expr* Expression = nullptr;
        UnaryOperatorKind Operator = UnaryOperatorKind::Invalid;
    };
    
    struct BinaryOperatorExpr {
        BinaryOperatorExpr(Expr* lhs, Expr* rhs, BinaryOperatorKind op)
            : LHS(lhs), RHS(rhs), Operator(op) {}

        Expr* LHS = nullptr;
        Expr* RHS = nullptr;
        BinaryOperatorKind Operator = BinaryOperatorKind::Invalid;
    };

    // CompoundAssignExpr
    // Represents a binary operator which does both a normal binary operation (+, -, ..) and an assignment (=)
    // In code this looks like:
    // foo += bar;
    struct CompoundAssignExpr {
        CompoundAssignExpr(Expr* lhs, Expr* rhs, BinaryOperatorKind op)
            : LHS(lhs), RHS(rhs), Operator(op) {}

        Expr* LHS = nullptr;
        Expr* RHS = nullptr;
        BinaryOperatorKind Operator = BinaryOperatorKind::Invalid;
    };

    struct Expr {
        template <typename T>
        static inline Expr* Create(CompilationContext* ctx, 
            SourceLocation loc, SourceRange range,
            ExprKind kind, 
            ExprValueKind valueKind, TypeInfo* type, 
            T t = ErrorExpr()) { return ctx->Allocate<Expr>(loc, range, kind, valueKind, type, t); }

        static inline Expr* Dup(CompilationContext* ctx, Expr* other) {
            Expr* newExpr = ctx->Allocate<Expr>();
            memcpy(reinterpret_cast<void*>(newExpr), other, sizeof(Expr));
            return newExpr;
        }

        ExprKind Kind = ExprKind::Invalid;
        ExprValueKind ValueKind = ExprValueKind::RValue;
        TypeInfo* Type = nullptr;

        bool ResultDiscarded = false;

        SourceLocation Loc;
        SourceRange Range;

        union {
            ErrorExpr Error;
            BooleanConstantExpr BooleanConstant;
            CharacterConstantExpr CharacterConstant;
            IntegerConstantExpr IntegerConstant;
            FloatingConstantExpr FloatingConstant;
            StringConstantExpr StringConstant;
            DeclRefExpr DeclRef;
            MemberExpr Member;
            TemporaryExpr Temporary;
            CopyExpr Copy;
            CallExpr Call;
            ConstructExpr Construct;
            MethodCallExpr MethodCall;
            FormatExpr Format;
            ParenExpr Paren;
            CastExpr Cast;
            ImplicitCastExpr ImplicitCast;
            UnaryOperatorExpr UnaryOperator;
            BinaryOperatorExpr BinaryOperator;
            CompoundAssignExpr CompoundAssign;
        };

        Expr()
            : BooleanConstant(false) {}

        Expr(SourceLocation loc, SourceRange range, ExprKind kind, ExprValueKind valueKind, TypeInfo* type, ErrorExpr error)
            : Loc(loc), Range(range), Kind(kind), ValueKind(valueKind), Type(type), Error(error) {}

        Expr(SourceLocation loc, SourceRange range, ExprKind kind, ExprValueKind valueKind, TypeInfo* type, BooleanConstantExpr booleanConstant)
            : Loc(loc), Range(range), Kind(kind), ValueKind(valueKind), Type(type), BooleanConstant(booleanConstant) {}

        Expr(SourceLocation loc, SourceRange range, ExprKind kind, ExprValueKind valueKind, TypeInfo* type, CharacterConstantExpr characterConstant)
            : Loc(loc), Range(range), Kind(kind), ValueKind(valueKind), Type(type), CharacterConstant(characterConstant) {}

        Expr(SourceLocation loc, SourceRange range, ExprKind kind, ExprValueKind valueKind, TypeInfo* type, IntegerConstantExpr integerConstant)
            : Loc(loc), Range(range), Kind(kind), ValueKind(valueKind), Type(type), IntegerConstant(integerConstant) {}

        Expr(SourceLocation loc, SourceRange range, ExprKind kind, ExprValueKind valueKind, TypeInfo* type, FloatingConstantExpr floatingConstant)
            : Loc(loc), Range(range), Kind(kind), ValueKind(valueKind), Type(type), FloatingConstant(floatingConstant) {}

        Expr(SourceLocation loc, SourceRange range, ExprKind kind, ExprValueKind valueKind, TypeInfo* type, StringConstantExpr stringConstant)
            : Loc(loc), Range(range), Kind(kind), ValueKind(valueKind), Type(type), StringConstant(stringConstant) {}

        Expr(SourceLocation loc, SourceRange range, ExprKind kind, ExprValueKind valueKind, TypeInfo* type, DeclRefExpr declRef)
            : Loc(loc), Range(range), Kind(kind), ValueKind(valueKind), Type(type), DeclRef(declRef) {}

        Expr(SourceLocation loc, SourceRange range, ExprKind kind, ExprValueKind valueKind, TypeInfo* type, MemberExpr member)
            : Loc(loc), Range(range), Kind(kind), ValueKind(valueKind), Type(type), Member(member) {}

        Expr(SourceLocation loc, SourceRange range, ExprKind kind, ExprValueKind valueKind, TypeInfo* type, TemporaryExpr temporary)
            : Loc(loc), Range(range), Kind(kind), ValueKind(valueKind), Type(type), Temporary(temporary) {}

        Expr(SourceLocation loc, SourceRange range, ExprKind kind, ExprValueKind valueKind, TypeInfo* type, CopyExpr copy)
            : Loc(loc), Range(range), Kind(kind), ValueKind(valueKind), Type(type), Copy(copy) {}

        Expr(SourceLocation loc, SourceRange range, ExprKind kind, ExprValueKind valueKind, TypeInfo* type, CallExpr call)
            : Loc(loc), Range(range), Kind(kind), ValueKind(valueKind), Type(type), Call(call) {}

        Expr(SourceLocation loc, SourceRange range, ExprKind kind, ExprValueKind valueKind, TypeInfo* type, ConstructExpr construct)
            : Loc(loc), Range(range), Kind(kind), ValueKind(valueKind), Type(type), Construct(construct) {}

        Expr(SourceLocation loc, SourceRange range, ExprKind kind, ExprValueKind valueKind, TypeInfo* type, MethodCallExpr methodCall)
            : Loc(loc), Range(range), Kind(kind), ValueKind(valueKind), Type(type), MethodCall(methodCall) {}

        Expr(SourceLocation loc, SourceRange range, ExprKind kind, ExprValueKind valueKind, TypeInfo* type, FormatExpr format)
            : Loc(loc), Range(range), Kind(kind), ValueKind(valueKind), Type(type), Format(format) {}

        Expr(SourceLocation loc, SourceRange range, ExprKind kind, ExprValueKind valueKind, TypeInfo* type, ParenExpr paren)
            : Loc(loc), Range(range), Kind(kind), ValueKind(valueKind), Type(type), Paren(paren) {}

        Expr(SourceLocation loc, SourceRange range, ExprKind kind, ExprValueKind valueKind, TypeInfo* type, CastExpr cast)
            : Loc(loc), Range(range), Kind(kind), ValueKind(valueKind), Type(type), Cast(cast) {}

        Expr(SourceLocation loc, SourceRange range, ExprKind kind, ExprValueKind valueKind, TypeInfo* type, ImplicitCastExpr implicitCast)
            : Loc(loc), Range(range), Kind(kind), ValueKind(valueKind), Type(type), ImplicitCast(implicitCast) {}

        Expr(SourceLocation loc, SourceRange range, ExprKind kind, ExprValueKind valueKind, TypeInfo* type, UnaryOperatorExpr unaryOperator)
            : Loc(loc), Range(range), Kind(kind), ValueKind(valueKind), Type(type), UnaryOperator(unaryOperator) {}

        Expr(SourceLocation loc, SourceRange range, ExprKind kind, ExprValueKind valueKind, TypeInfo* type, BinaryOperatorExpr binaryOperator)
            : Loc(loc), Range(range), Kind(kind), ValueKind(valueKind), Type(type), BinaryOperator(binaryOperator) {}

        Expr(SourceLocation loc, SourceRange range, ExprKind kind, ExprValueKind valueKind, TypeInfo* type, CompoundAssignExpr compoundAssign)
            : Loc(loc), Range(range), Kind(kind), ValueKind(valueKind), Type(type), CompoundAssign(compoundAssign) {}
    };

    inline Expr g_ErrorExpr = Expr(SourceLocation(), SourceRange(), ExprKind::Error, ExprValueKind::RValue, nullptr, ErrorExpr());

} // namespace Aria::Internal
