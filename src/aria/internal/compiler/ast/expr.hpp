#pragma once

#include "aria/internal/compiler/core/vector.hpp"
#include "aria/internal/compiler/core/string_view.hpp"
#include "aria/internal/compiler/types/type_info.hpp"
#include "aria/internal/compiler/core/source_location.hpp"
#include "aria/internal/compiler/ast/stmt.hpp"
#include "aria/internal/types.hpp"

#include <variant>

namespace Aria::Internal {

#pragma region DeclRefType

    enum class DeclRefType {
        LocalVar,
        ParamVar,
        GlobalVar,
        Function
    };

    inline const char* DeclRefTypeToString(DeclRefType type) {
        switch (type) {
            case DeclRefType::LocalVar:    return "LocalVar";
            case DeclRefType::ParamVar:    return "ParamVar";
            case DeclRefType::GlobalVar:   return "GlobalVar";
            case DeclRefType::Function:    return "Function";
        }
    }

#pragma endregion

#pragma region CastType

    enum class CastType {
        Invalid,
        Integral,
        Floating,
        IntegralToFloating,
        FloatingToIntegral,

        LValueToRValue
    };

    inline const char* CastTypeToString(CastType type) {
        switch (type) {
        case CastType::Invalid: return "Invalid";
            case CastType::Integral: return "Integral";
            case CastType::Floating: return "Floating";
            case CastType::IntegralToFloating: return "IntegralToFloating";
            case CastType::FloatingToIntegral: return "FloatingToIntegral";
            case CastType::LValueToRValue: return "LValueToRValue";
            default: ARIA_ASSERT(false, "Unreachable");
        }
    }

#pragma endregion

#pragma region UnaryOperatorType

    enum class UnaryOperatorType {
        Invalid,
    
        Not, // "!"
        Negate // "-8.7f"
    };
    
    inline const char* UnaryOperatorTypeToString(UnaryOperatorType type) {
        switch (type) {
            case UnaryOperatorType::Invalid: return "invalid";
            
            case UnaryOperatorType::Not: return "!";
            case UnaryOperatorType::Negate: return "-";
        }
    
        ARIA_ASSERT(false, "Unreachable!");
        return "invalid";
    }

#pragma endregion

#pragma region BinaryOperatorType

    enum class BinaryOperatorType {
        Invalid,
        Add, AddInPlace,
        Sub, SubInPlace,
        Mul, MulInPlace,
        Div, DivInPlace,
        Mod, ModInPlace,
    
        Less,
        LessOrEq,
        Greater,
        GreaterOrEq,

        And, AndInPlace, BitAnd,
        Or, OrInPlace, BitOr,
        Xor, XorInPlace,
    
        Eq,
        IsEq,
        IsNotEq,
    };
    
    inline const char* BinaryOperatorTypeToString(BinaryOperatorType type) {
        switch (type) {
            case BinaryOperatorType::Invalid: return "invalid";
            case BinaryOperatorType::Add: return "+";
            case BinaryOperatorType::AddInPlace: return "+=";
            case BinaryOperatorType::Sub: return "-";
            case BinaryOperatorType::SubInPlace: return "-=";
            case BinaryOperatorType::Mul: return "*";
            case BinaryOperatorType::MulInPlace: return "*=";
            case BinaryOperatorType::Div: return "/";
            case BinaryOperatorType::DivInPlace: return "/=";
            case BinaryOperatorType::Mod: return "%";
            case BinaryOperatorType::ModInPlace: return "%=";
    
            case BinaryOperatorType::Less: return "<";
            case BinaryOperatorType::LessOrEq: return "<=";
            case BinaryOperatorType::Greater: return ">";
            case BinaryOperatorType::GreaterOrEq: return ">=";

            case BinaryOperatorType::And: return "&";
            case BinaryOperatorType::AndInPlace: return "&=";
            case BinaryOperatorType::BitAnd: return "&&";
            case BinaryOperatorType::Or: return "|";
            case BinaryOperatorType::OrInPlace: return "|=";
            case BinaryOperatorType::BitOr: return "||";
            case BinaryOperatorType::Xor: return "^";
            case BinaryOperatorType::XorInPlace: return "^=";
    
            case BinaryOperatorType::Eq: return "=";
            case BinaryOperatorType::IsEq: return "==";
            case BinaryOperatorType::IsNotEq: return "!=";
        }
    
        ARIA_ASSERT(false, "Unreachable!");
        return "invalid";
    }

#pragma endregion

#pragma region ExprValueType

    enum class ExprValueType {
        LValue,
        RValue
    };

    inline const char* ExprValueTypeToString(ExprValueType type) {
        switch (type) {
            case ExprValueType::LValue: return "lvalue";
            case ExprValueType::RValue: return "rvalue";
        }

        ARIA_UNREACHABLE();
    }

#pragma endregion
    
    struct Expr : public Stmt {
        Expr(CompilationContext* ctx, SourceLocation loc, SourceRange range)
            : Stmt(ctx), Loc(loc), Range(range) {}

        virtual TypeInfo* GetResolvedType() = 0;
        virtual const TypeInfo* GetResolvedType() const = 0;

        virtual ExprValueType GetValueType() const = 0;

        SourceLocation Loc;
        SourceRange Range;
    };

    using IntegerStorage = std::variant<i8, u8, i16, u16, i32, u32, i64, u64>;
    using FloatingStorage = std::variant<f32, f64>;

    struct BooleanConstantExpr final : public Expr {
        BooleanConstantExpr(CompilationContext* ctx, SourceLocation loc, SourceRange range, bool value)
            : Expr(ctx, loc, range), m_Value(value) {}

        inline bool GetValue() const { return m_Value; }

        inline virtual TypeInfo* GetResolvedType() override { return TypeInfo::Create(m_Context, PrimitiveType::Bool, true); }
        inline virtual const TypeInfo* GetResolvedType() const override { return TypeInfo::Create(m_Context, PrimitiveType::Bool, true); }

        inline virtual ExprValueType GetValueType() const override { return ExprValueType::RValue; }

    private:
        bool m_Value = false;
    };
    
    struct CharacterConstantExpr final : public Expr {
        CharacterConstantExpr(CompilationContext* ctx, SourceLocation loc, SourceRange range, i8 value)
            : Expr(ctx, loc, range), m_Value(value) {}

        inline i8 GetValue() const { return m_Value; }

        inline virtual TypeInfo* GetResolvedType() override { return TypeInfo::Create(m_Context, PrimitiveType::Char, true); }
        inline virtual const TypeInfo* GetResolvedType() const override { return TypeInfo::Create(m_Context, PrimitiveType::Char, true); }

        inline virtual ExprValueType GetValueType() const override { return ExprValueType::RValue; }

    private:
        i8 m_Value = 0;
    };
    
    struct IntegerConstantExpr final : public Expr {
        IntegerConstantExpr(CompilationContext* ctx, SourceLocation loc, SourceRange range, IntegerStorage value, TypeInfo* resolvedType)
            : Expr(ctx, loc, range), m_Value(value), m_ResolvedType(resolvedType) {}

        inline IntegerStorage GetValue() const { return m_Value; }

        inline virtual TypeInfo* GetResolvedType() override { return m_ResolvedType; }
        inline virtual const TypeInfo* GetResolvedType() const override { return m_ResolvedType; }

        inline virtual ExprValueType GetValueType() const override { return ExprValueType::RValue; }

    private:
        IntegerStorage m_Value;
        TypeInfo* m_ResolvedType = nullptr;
    };
    
    struct FloatingConstantExpr final : public Expr {
        FloatingConstantExpr(CompilationContext* ctx, SourceLocation loc, SourceRange range, FloatingStorage value, TypeInfo* resolvedType)
            : Expr(ctx, loc, range), m_Value(value), m_ResolvedType(resolvedType) {}

        inline FloatingStorage GetValue() const { return m_Value; }

        inline virtual TypeInfo* GetResolvedType() override { return m_ResolvedType; }
        inline virtual const TypeInfo* GetResolvedType() const override { return m_ResolvedType; }

        inline virtual ExprValueType GetValueType() const override { return ExprValueType::RValue; }

    private:
        FloatingStorage m_Value;
        TypeInfo* m_ResolvedType = nullptr;
    };

    struct StringConstantExpr final : public Expr {
        StringConstantExpr(CompilationContext* ctx, SourceLocation loc, SourceRange range, StringView value)
            : Expr(ctx, loc, range), m_Value(value) {}

        inline StringView GetValue() const { return m_Value; }

        inline virtual TypeInfo* GetResolvedType() override { return TypeInfo::Create(m_Context, PrimitiveType::StringLiteral, m_Value.Size()); }
        inline virtual const TypeInfo* GetResolvedType() const override { return TypeInfo::Create(m_Context, PrimitiveType::StringLiteral, m_Value.Size()); }

        inline virtual ExprValueType GetValueType() const override { return ExprValueType::RValue; }

    private:
        StringView m_Value;
    };

    struct DeclRefExpr final : public Expr {
        DeclRefExpr(CompilationContext* ctx, SourceLocation loc, SourceRange range, StringView identifier)
            : Expr(ctx, loc, range), m_Identifier(identifier) {}

        inline std::string GetIdentifier() const { return fmt::format("{}", m_Identifier); }
        inline StringView GetRawIdentifier() const { return m_Identifier; }

        inline DeclRefType GetType() const { return m_Type; }
        inline void SetType(DeclRefType type) { m_Type = type; }

        inline virtual TypeInfo* GetResolvedType() override { return m_ResolvedType; }
        inline virtual const TypeInfo* GetResolvedType() const override { return m_ResolvedType; }
        inline void SetResolvedType(TypeInfo* type) { m_ResolvedType = type; }

        inline virtual ExprValueType GetValueType() const override { return ExprValueType::LValue; }

    private:
        StringView m_Identifier;

        DeclRefType m_Type = DeclRefType::LocalVar;
        TypeInfo* m_ResolvedType = nullptr;
    };

    struct MemberExpr final : public Expr {
        MemberExpr(CompilationContext* ctx, SourceLocation loc, SourceRange range, StringView member, Expr* parent)
            : Expr(ctx, loc, range), m_Member(member), m_Parent(parent) {}

        inline StringView GetMember() const { return m_Member; }

        inline Expr* GetParent() { return m_Parent; }
        inline const Expr* GetParent() const { return m_Parent; }

        inline TypeInfo* GetParentType() { return m_ParentType; }
        inline const TypeInfo* GetParentType() const { return m_ParentType; }
        inline void SetParentType(TypeInfo* type) { m_ParentType = type; }

        inline virtual TypeInfo* GetResolvedType() override { return m_ResolvedType; }
        inline virtual const TypeInfo* GetResolvedType() const override { return m_ResolvedType; }
        inline void SetResolvedType(TypeInfo* type) { m_ResolvedType = type; }

        inline virtual ExprValueType GetValueType() const override { return ExprValueType::LValue; }

    private:
        StringView m_Member;
        Expr* m_Parent = nullptr;

        TypeInfo* m_ParentType = nullptr;
        TypeInfo* m_ResolvedType = nullptr;
    };

    struct SelfExpr final : public Expr {
        SelfExpr(CompilationContext* ctx, SourceLocation loc, SourceRange range)
            : Expr(ctx, loc, range) {}

        inline virtual TypeInfo* GetResolvedType() override { return m_ResolvedType; }
        inline virtual const TypeInfo* GetResolvedType() const override { return m_ResolvedType; }
        inline void SetResolvedType(TypeInfo* type) { m_ResolvedType = type; }

        inline virtual ExprValueType GetValueType() const override { return ExprValueType::LValue; }

    private:
        TypeInfo* m_ResolvedType = nullptr;
    };

    struct CallExpr final : public Expr {
        CallExpr(CompilationContext* ctx, SourceLocation loc, SourceRange range, DeclRefExpr* callee, TinyVector<Expr*> args)
            : Expr(ctx, loc, range), m_Callee(callee), m_Arguments(args) {}

        inline DeclRefExpr* GetCallee() { return m_Callee; }
        inline const DeclRefExpr* GetCallee() const { return m_Callee; }

        inline TinyVector<Expr*> GetArguments() const { return m_Arguments; }
        inline void SetArgument(size_t index, Expr* expr) { m_Arguments.Items[index] = expr; }

        inline virtual TypeInfo* GetResolvedType() override { return m_ResolvedType; }
        inline virtual const TypeInfo* GetResolvedType() const override { return m_ResolvedType; }
        inline void SetResolvedType(TypeInfo* type) { m_ResolvedType = type; }

        inline virtual ExprValueType GetValueType() const override { return ExprValueType::RValue; }

    private:
        DeclRefExpr* m_Callee;
        TinyVector<Expr*> m_Arguments;

        TypeInfo* m_ResolvedType = nullptr;
    };

    struct MethodCallExpr final : public Expr {
        MethodCallExpr(CompilationContext* ctx, SourceLocation loc, SourceRange range, MemberExpr* callee, TinyVector<Expr*> args)
            : Expr(ctx, loc, range), m_Callee(callee), m_Arguments(args) {}
    
        inline MemberExpr* GetCallee() { return m_Callee; }
        inline const MemberExpr* GetCallee() const { return m_Callee; }
    
        inline TinyVector<Expr*> GetArguments() const { return m_Arguments; }
        inline void SetArgument(size_t index, Expr* expr) { m_Arguments.Items[index] = expr; }
    
    
        inline virtual TypeInfo* GetResolvedType() override { return m_ResolvedType; }
        inline virtual const TypeInfo* GetResolvedType() const override { return m_ResolvedType; }
        inline void SetResolvedType(TypeInfo* type) { m_ResolvedType = type; }
    
        inline virtual ExprValueType GetValueType() const override { return ExprValueType::RValue; }
    
    private:
        MemberExpr* m_Callee;
        TinyVector<Expr*> m_Arguments;
    
        TypeInfo* m_ResolvedType = nullptr;
    };
    
    // ParenExpr
    // At its core it just wraps an expression
    // These kinds of expressions are usually from the actual source code
    // eg. 1 + (2 - 3)
    struct ParenExpr final : public Expr {
        ParenExpr(CompilationContext* ctx, SourceLocation loc, SourceRange range, Expr* expr)
            : Expr(ctx, loc, range), m_Expression(expr) {}

        inline Expr* GetChildExpr() { return m_Expression; }
        inline const Expr* GetChildExpr() const { return m_Expression; }

        inline virtual TypeInfo* GetResolvedType() override { return m_Expression->GetResolvedType(); }
        inline virtual const TypeInfo* GetResolvedType() const override { return m_Expression->GetResolvedType(); }

        inline virtual ExprValueType GetValueType() const override { return m_Expression->GetValueType(); }

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
        inline const Expr* GetChildExpr() const { return m_Expression; }

        inline StringView GetParsedType() const { return m_ParsedDestinationType; }

        inline CastType GetCastType() const { return m_ResolvedCastType; }
        inline void SetCastType(CastType type) { m_ResolvedCastType = type; }

        inline virtual TypeInfo* GetResolvedType() override { return m_ResolvedType; }
        inline virtual const TypeInfo* GetResolvedType() const override { return m_ResolvedType; }
        inline void SetResolvedType(TypeInfo* type) { m_ResolvedType = type; }

        inline virtual ExprValueType GetValueType() const override { return ExprValueType::RValue; }

    private:
        Expr* m_Expression = nullptr;
        StringView m_ParsedDestinationType;

        CastType m_ResolvedCastType = CastType::Integral;
        TypeInfo* m_ResolvedType = nullptr;
    };

    // ImplicitCastExpr
    // Represents an implicit cast automatically inserted by the type checker/semantic analyzer
    // eg. float a = 5; -> here "5" is implicitly converted to a float
    struct ImplicitCastExpr final : public Expr {
        ImplicitCastExpr(CompilationContext* ctx, SourceLocation loc, SourceRange range, Expr* expr, CastType castType, TypeInfo* destType)
            : Expr(ctx, loc, range), m_Expression(expr), m_ResolvedCastType(castType), m_ResolvedType(destType) {}

        inline Expr* GetChildExpr() { return m_Expression; }
        inline const Expr* GetChildExpr() const { return m_Expression; }

        inline CastType GetCastType() const { return m_ResolvedCastType; }
        inline void SetCastType(CastType type) { m_ResolvedCastType = type; }

        inline virtual TypeInfo* GetResolvedType() override { return m_ResolvedType; }
        inline virtual const TypeInfo* GetResolvedType() const override { return m_ResolvedType; }

        inline virtual ExprValueType GetValueType() const override { return ExprValueType::RValue; }

    private:
        Expr* m_Expression = nullptr;

        CastType m_ResolvedCastType = CastType::Integral;
        TypeInfo* m_ResolvedType = nullptr;
    };
    
    struct UnaryOperatorExpr final : public Expr {
        UnaryOperatorExpr(CompilationContext* ctx, SourceLocation loc, SourceRange range, Expr* expr, UnaryOperatorType op)
            : Expr(ctx, loc, range), m_Expression(expr), m_Operator(op) {}

        inline Expr* GetChildExpr() { return m_Expression; }
        inline const Expr* GetChildExpr() const { return m_Expression; }
        inline void SetChildExpr(Expr* expr) { m_Expression = expr; }

        inline UnaryOperatorType GetUnaryOperator() const { return m_Operator; }

        inline virtual TypeInfo* GetResolvedType() override { return m_ResolvedType; }
        inline virtual const TypeInfo* GetResolvedType() const override { return m_ResolvedType; }
        inline void SetResolvedType(TypeInfo* type) { m_ResolvedType = type; }

        inline virtual ExprValueType GetValueType() const override { return ExprValueType::RValue; }

    private:
        Expr* m_Expression = nullptr;
        UnaryOperatorType m_Operator = UnaryOperatorType::Invalid;
    
        TypeInfo* m_ResolvedType = nullptr;
    };
    
    struct BinaryOperatorExpr final : public Expr {
        BinaryOperatorExpr(CompilationContext* ctx, SourceLocation loc, SourceRange range, Expr* lhs, Expr* rhs, BinaryOperatorType op)
            : Expr(ctx, loc, range), m_LHS(lhs), m_RHS(rhs), m_Operator(op) {}

        inline Expr* GetLHS() { return m_LHS; }
        inline const Expr* GetLHS() const { return m_LHS; }
        inline void SetLHS(Expr* expr) { m_LHS = expr; }

        inline Expr* GetRHS() { return m_RHS; }
        inline const Expr* GetRHS() const { return m_RHS; }
        inline void SetRHS(Expr* expr) { m_RHS = expr; }

        inline BinaryOperatorType GetBinaryOperator() const { return m_Operator; }

        inline virtual TypeInfo* GetResolvedType() override { return m_ResolvedType; }
        inline virtual const TypeInfo* GetResolvedType() const override { return m_ResolvedType; }
        inline void SetResolvedType(TypeInfo* type) { m_ResolvedType = type; }

        inline virtual ExprValueType GetValueType() const override { return ExprValueType::RValue; }

    private:
        Expr* m_LHS = nullptr;
        Expr* m_RHS = nullptr;
        BinaryOperatorType m_Operator = BinaryOperatorType::Invalid;
    
        TypeInfo* m_ResolvedType = nullptr;
    };

} // namespace Aria::Internal
