#pragma once

#include "aria/internal/compiler/core/vector.hpp"
#include "aria/internal/compiler/core/string_view.hpp"
#include "aria/internal/compiler/types/type_info.hpp"
#include "aria/internal/compiler/core/source_location.hpp"
#include "aria/internal/compiler/ast/stmt.hpp"
#include "aria/internal/types.hpp"

#include <variant>

namespace Aria::Internal {

#pragma region DeclRefKind

    enum class DeclRefKind {
        LocalVar,
        ParamVar,
        GlobalVar,
        Function
    };

    inline const char* DeclRefKindToString(DeclRefKind kind) {
        switch (kind) {
            case DeclRefKind::LocalVar:  return "LocalVar";
            case DeclRefKind::ParamVar:  return "ParamVar";
            case DeclRefKind::GlobalVar: return "GlobalVar";
            case DeclRefKind::Function:  return "Function";
        }
    }

#pragma endregion

#pragma region CastKind

    enum class CastKind {
        Invalid,
        Integral,
        Floating,
        IntegralToFloating,
        FloatingToIntegral,

        LValueToRValue
    };

    inline const char* CastKindToString(CastKind kind) {
        switch (kind) {
            case CastKind::Invalid: return "Invalid";
            case CastKind::Integral: return "Integral";
            case CastKind::Floating: return "Floating";
            case CastKind::IntegralToFloating: return "IntegralToFloating";
            case CastKind::FloatingToIntegral: return "FloatingToIntegral";
            case CastKind::LValueToRValue: return "LValueToRValue";
            default: ARIA_ASSERT(false, "Unreachable");
        }
    }

#pragma endregion

#pragma region UnaryOperatorKind

    enum class UnaryOperatorKind {
        Invalid,
    
        Not, // "!"
        Negate // "-8.7f"
    };
    
    inline const char* UnaryOperatorKindToString(UnaryOperatorKind kind) {
        switch (kind) {
            case UnaryOperatorKind::Invalid: return "invalid";
            
            case UnaryOperatorKind::Not: return "!";
            case UnaryOperatorKind::Negate: return "-";

            default: ARIA_UNREACHABLE();
        }
    }

#pragma endregion

#pragma region BinaryOperatorKind

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

        And, CompoundAnd, BitAnd,
        Or,  CompoundOr,  BitOr,
        Xor, CompoundXor,
    
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

            case BinaryOperatorKind::And: return "&";
            case BinaryOperatorKind::CompoundAnd: return "&=";
            case BinaryOperatorKind::BitAnd: return "&&";
            case BinaryOperatorKind::Or: return "|";
            case BinaryOperatorKind::CompoundOr: return "|=";
            case BinaryOperatorKind::BitOr: return "||";
            case BinaryOperatorKind::Xor: return "^";
            case BinaryOperatorKind::CompoundXor: return "^=";

            case BinaryOperatorKind::Eq: return "=";
            case BinaryOperatorKind::IsEq: return "==";
            case BinaryOperatorKind::IsNotEq: return "!=";

            default: ARIA_UNREACHABLE();
        }
    }

#pragma endregion

#pragma region ExprValueKind

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

#pragma endregion

    struct Expr : public Stmt {
        Expr(CompilationContext* ctx, SourceLocation loc, SourceRange range)
            : Stmt(ctx), Loc(loc), Range(range) {}

        Expr(CompilationContext* ctx, SourceLocation loc, SourceRange range, TypeInfo* type)
            : Stmt(ctx), Loc(loc), Range(range), m_Type(type) {}

        inline TypeInfo* GetResolvedType() { return m_Type; }
        inline void SetResolvedType(TypeInfo* type) { m_Type = type; }

        inline ExprValueKind GetValueKind() const { return m_ValueKind; }
        inline void SetValueKind(ExprValueKind kind) { m_ValueKind = kind; }

        SourceLocation Loc;
        SourceRange Range;

        bool IsStmtExpr = false; // true is the expression is created as a statement (eg. x = 6 is a statement expression)

    protected:
        TypeInfo* m_Type = nullptr;
        ExprValueKind m_ValueKind{};
    };

    struct BooleanConstantExpr final : public Expr {
        BooleanConstantExpr(CompilationContext* ctx, SourceLocation loc, SourceRange range, bool value)
            : Expr(ctx, loc, range, TypeInfo::Create(ctx, PrimitiveType::Bool, false)), m_Value(value) {}

        inline bool GetValue() const { return m_Value; }

    private:
        bool m_Value = false;
    };
    
    struct CharacterConstantExpr final : public Expr {
        CharacterConstantExpr(CompilationContext* ctx, SourceLocation loc, SourceRange range, i8 value)
            : Expr(ctx, loc, range, TypeInfo::Create(ctx, PrimitiveType::Char, false)), m_Value(value) {}

        inline i8 GetValue() const { return m_Value; }

    private:
        i8 m_Value = 0;
    };
    
    struct IntegerConstantExpr final : public Expr {
        IntegerConstantExpr(CompilationContext* ctx, SourceLocation loc, SourceRange range, u64 value, TypeInfo* resolvedType)
            : Expr(ctx, loc, range, resolvedType), m_Value(value) {}

        inline u64 GetValue() const { return m_Value; }

    private:
        u64 m_Value = 0;
    };
    
    struct FloatingConstantExpr final : public Expr {
        FloatingConstantExpr(CompilationContext* ctx, SourceLocation loc, SourceRange range, f64 value)
            : Expr(ctx, loc, range, TypeInfo::Create(ctx, PrimitiveType::Double, false)), m_Value(value) {}

        inline f64 GetValue() const { return m_Value; }

    private:
        f64 m_Value = 0.0;
    };

    struct StringConstantExpr final : public Expr {
        StringConstantExpr(CompilationContext* ctx, SourceLocation loc, SourceRange range, StringView value)
            : Expr(ctx, loc, range, TypeInfo::Create(ctx, PrimitiveType::String, false)), m_Value(value) {}

        inline StringView GetValue() const { return m_Value; }

    private:
        StringView m_Value;
    };

    struct DeclRefExpr final : public Expr {
        DeclRefExpr(CompilationContext* ctx, SourceLocation loc, SourceRange range, StringView identifier)
            : Expr(ctx, loc, range), m_Identifier(identifier) {}

        inline std::string GetIdentifier() const { return fmt::format("{}", m_Identifier); }
        inline StringView GetRawIdentifier() const { return m_Identifier; }

        inline DeclRefKind GetKind() const { return m_Kind; }
        inline void SetKind(DeclRefKind kind) { m_Kind = kind; }

    private:
        StringView m_Identifier;
        DeclRefKind m_Kind = DeclRefKind::LocalVar;
    };

    struct MemberExpr final : public Expr {
        MemberExpr(CompilationContext* ctx, SourceLocation loc, SourceRange range, StringView member, Expr* parent)
            : Expr(ctx, loc, range), m_Member(member), m_Parent(parent) {}

        inline StringView GetMember() const { return m_Member; }

        inline Expr* GetParent() { return m_Parent; }
        inline void SetParent(Expr* parent) { m_Parent = parent; }

        inline TypeInfo* GetParentType() { return m_ParentType; }
        inline const TypeInfo* GetParentType() const { return m_ParentType; }
        inline void SetParentType(TypeInfo* type) { m_ParentType = type; }

    private:
        StringView m_Member;
        Expr* m_Parent = nullptr;

        TypeInfo* m_ParentType = nullptr;
    };

    struct SelfExpr final : public Expr {
        SelfExpr(CompilationContext* ctx, SourceLocation loc, SourceRange range)
            : Expr(ctx, loc, range) {}
    };

    // TemporaryExpr
    // Represents a temporary expression
    // eg. Print("Hello world");
    // Here "Hello world" will call a constructor that allocates memory and therefore its destructor needs to be called
    struct TemporaryExpr final : public Expr {
        TemporaryExpr(CompilationContext* ctx, SourceLocation loc, SourceRange range, Expr* expr, TypeInfo* type)
            : Expr(ctx, loc, range, type), m_Expression(expr) {}

        inline Expr* GetExpression() { return m_Expression; }

    private:
        Expr* m_Expression = nullptr;
    };

    // ExprWithCleanups
    // Wraps an entire expression which holds some temporaries
    // The purpose of this node is to let the codegen know when destructors should be called
    // eg. Print("Hello world");
    // Here the entire expression will be wrapped inside of a "ExprWithCleanups",
    // Because "Hello world" is a temporary expression
    struct ExprWithCleanups final : public Expr {
        ExprWithCleanups(CompilationContext* ctx, SourceLocation loc, SourceRange range, Expr* expr, TypeInfo* type)
            : Expr(ctx, loc, range, type), m_Expression(expr) {}

        inline Expr* GetExpression() { return m_Expression; }

    private:
        Expr* m_Expression = nullptr;
    };

    struct CallExpr final : public Expr {
        CallExpr(CompilationContext* ctx, SourceLocation loc, SourceRange range, DeclRefExpr* callee, TinyVector<Expr*> args)
            : Expr(ctx, loc, range), m_Callee(callee), m_Arguments(args) {}

        inline Expr* GetCallee() { return m_Callee; }

        inline TinyVector<Expr*> GetArguments() const { return m_Arguments; }
        inline void SetArgument(size_t index, Expr* expr) { m_Arguments.Items[index] = expr; }

    private:
        Expr* m_Callee;
        TinyVector<Expr*> m_Arguments;
    };

    struct MethodCallExpr final : public Expr {
        MethodCallExpr(CompilationContext* ctx, SourceLocation loc, SourceRange range, MemberExpr* callee, TinyVector<Expr*> args)
            : Expr(ctx, loc, range), m_Callee(callee), m_Arguments(args) {}
    
        inline MemberExpr* GetCallee() { return m_Callee; }
    
        inline TinyVector<Expr*> GetArguments() const { return m_Arguments; }
        inline void SetArgument(size_t index, Expr* expr) { m_Arguments.Items[index] = expr; }
    
    private:
        MemberExpr* m_Callee;
        TinyVector<Expr*> m_Arguments;
    };
    
    // ParenExpr
    // At its core it just wraps an expression
    // These kinds of expressions are usually from the actual source code
    // eg. 1 + (2 - 3)
    struct ParenExpr final : public Expr {
        ParenExpr(CompilationContext* ctx, SourceLocation loc, SourceRange range, Expr* expr)
            : Expr(ctx, loc, range), m_Expression(expr) {}

        inline Expr* GetChildExpr() { return m_Expression; }
        inline void SetChildExpr(Expr* newExpr) { m_Expression = newExpr; }

    private:
        Expr* m_Expression = nullptr;
    };
    
    // CastExpr
    // Represents an explicit cast in the original source code
    // This node should never represent an implicit cast, for that use ImplicitCastExpr
    // eg. int a = (int)5.5;
    struct CastExpr final : public Expr {
        CastExpr(CompilationContext* ctx, SourceLocation loc, SourceRange range, Expr* expr, StringView parsedType)
            : Expr(ctx, loc, range), m_Expression(expr), m_ParsedDestinationType(parsedType) {}

        inline Expr* GetChildExpr() { return m_Expression; }
        inline void SetChildExpr(Expr* newExpr) { m_Expression = newExpr; }

        inline StringView GetParsedType() const { return m_ParsedDestinationType; }

        inline CastKind GetCastKind() const { return m_CastKind; }
        inline void SetCastType(CastKind kind) { m_CastKind = kind; }

    private:
        Expr* m_Expression = nullptr;
        StringView m_ParsedDestinationType;

        CastKind m_CastKind = CastKind::Integral;
    };

    // ImplicitCastExpr
    // Represents an implicit cast automatically inserted by the type checker/semantic analyzer
    // eg. float a = 5; -> here "5" is implicitly converted to a float
    struct ImplicitCastExpr final : public Expr {
        ImplicitCastExpr(CompilationContext* ctx, SourceLocation loc, SourceRange range, Expr* expr, CastKind castKind)
            : Expr(ctx, loc, range), m_Expression(expr), m_CastKind(castKind) {}

        inline Expr* GetChildExpr() { return m_Expression; }

        inline CastKind GetCastKind() const { return m_CastKind; }
        inline void SetCastType(CastKind kind) { m_CastKind = kind; }

    private:
        Expr* m_Expression = nullptr;

        CastKind m_CastKind = CastKind::Integral;
    };
    
    struct UnaryOperatorExpr final : public Expr {
        UnaryOperatorExpr(CompilationContext* ctx, SourceLocation loc, SourceRange range, Expr* expr, UnaryOperatorKind op)
            : Expr(ctx, loc, range), m_Expression(expr), m_Operator(op) {}

        inline Expr* GetChildExpr() { return m_Expression; }
        inline void SetChildExpr(Expr* expr) { m_Expression = expr; }

        inline UnaryOperatorKind GetUnaryOperator() const { return m_Operator; }

    private:
        Expr* m_Expression = nullptr;
        UnaryOperatorKind m_Operator = UnaryOperatorKind::Invalid;
    };
    
    struct BinaryOperatorExpr final : public Expr {
        BinaryOperatorExpr(CompilationContext* ctx, SourceLocation loc, SourceRange range, Expr* lhs, Expr* rhs, BinaryOperatorKind op)
            : Expr(ctx, loc, range), m_LHS(lhs), m_RHS(rhs), m_Operator(op) {}

        inline Expr* GetLHS() { return m_LHS; }
        inline void SetLHS(Expr* expr) { m_LHS = expr; }

        inline Expr* GetRHS() { return m_RHS; }
        inline void SetRHS(Expr* expr) { m_RHS = expr; }

        inline BinaryOperatorKind GetBinaryOperator() const { return m_Operator; }

    private:
        Expr* m_LHS = nullptr;
        Expr* m_RHS = nullptr;
        BinaryOperatorKind m_Operator = BinaryOperatorKind::Invalid;
    };

    // CompoundAssignExpr
    // Represents a binary operator which does both a normal binary operation (+, -, ..) and an assignment (=)
    // In code this looks like:
    // foo += bar;
    struct CompoundAssignExpr final : public Expr {
        CompoundAssignExpr(CompilationContext* ctx, SourceLocation loc, SourceRange range, Expr* lhs, Expr* rhs, BinaryOperatorKind op)
            : Expr(ctx, loc, range), m_LHS(lhs), m_RHS(rhs), m_Operator(op) {}

        inline Expr* GetLHS() { return m_LHS; }
        inline void SetLHS(Expr* expr) { m_LHS = expr; }

        inline Expr* GetRHS() { return m_RHS; }
        inline void SetRHS(Expr* expr) { m_RHS = expr; }

        inline BinaryOperatorKind GetBinaryOperator() const { return m_Operator; }

    private:
        Expr* m_LHS = nullptr;
        Expr* m_RHS = nullptr;
        BinaryOperatorKind m_Operator = BinaryOperatorKind::Invalid;
    };

} // namespace Aria::Internal
