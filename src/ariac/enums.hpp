#pragma once

namespace ariac {

    enum class CompilerDiagKind {
        Note,
        Warning,
        Error
    };

    enum class ExprKind {
        Invalid = 0,

        Error,
        BooleanLiteral,
        CharacterLiteral,
        IntegerLiteral,
        FloatingLiteral,
        StringLiteral,
        ArrayFiller,
        Null,
        DeclRef,
        TypeInfo,
        Member,
        BuiltinMember,
        DependentMember,
        TypeMember,
        Self,
        Call,
        BuiltinCall,
        Construct,
        ArrayLiteral,
        MethodCall,
        ArraySubscript,
        ToSlice,
        Move,
        Temporary,
        ExprWithCleanups,
        Paren,
        Cast,
        ImplicitCast,
        UnaryOperator,
        BinaryOperator,
        CompoundAssign,
        Const
    };

    enum class BuiltinCallKind {
        Sizeof,
        Memcpy,
        Memset,
    };

    inline const char* builtin_call_kind_to_string(BuiltinCallKind kind) {
        switch (kind) {
            case BuiltinCallKind::Sizeof: return "sizeof";
            case BuiltinCallKind::Memcpy: return "memcpy";
            case BuiltinCallKind::Memset: return "memset";

            default: ARIA_UNREACHABLE("Invalid built in call");
        }
    }

    enum class CastKind {
        Invalid,
        Integral,
        Floating,
        IntegralToFloating,
        FloatingToIntegral,
        IntegerToPointer,
        BitCast,
        SliceToPointer,
        PointerToAny,

        LValueToRValue
    };

    inline const char* cast_kind_to_string(CastKind kind) {
        switch (kind) {
            case CastKind::Invalid: return "Invalid";
            case CastKind::Integral: return "Integral";
            case CastKind::Floating: return "Floating";
            case CastKind::IntegralToFloating: return "IntegralToFloating";
            case CastKind::FloatingToIntegral: return "FloatingToIntegral";
            case CastKind::IntegerToPointer: return "IntegerToPointer";
            case CastKind::BitCast: return "BitCast";
            case CastKind::SliceToPointer: return "SliceToPointer";
            case CastKind::PointerToAny: return "PointerToAny";
            case CastKind::LValueToRValue: return "LValueToRValue";

            default: ARIA_UNREACHABLE("Invalid cast kind");
        }
    }

    enum class UnaryOperatorKind {
        Invalid,
    
        Not, // "!"
        Negate, // "-8.7f"
        AddressOf, // "&x"
        RValueAddressOf, // "&&x"
        Dereference, // "*ptr"
        Increment, // ++i, i++
        Decrement // --i, i--
    };

    inline const char* unary_op_kind_to_string(UnaryOperatorKind kind) {
        switch (kind) {
            case UnaryOperatorKind::Invalid: return "invalid";
            
            case UnaryOperatorKind::Not: return "!";
            case UnaryOperatorKind::Negate: return "-";
            case UnaryOperatorKind::AddressOf: return "&";
            case UnaryOperatorKind::RValueAddressOf: return "&&";
            case UnaryOperatorKind::Dereference: return "*";
            case UnaryOperatorKind::Increment: return "++";
            case UnaryOperatorKind::Decrement: return "--";

            default: ARIA_UNREACHABLE("Invalid unary operator");
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

            default: ARIA_UNREACHABLE("Invalid binary operator");
        }
    }

    inline bool is_binary_operator_bit(BinaryOperatorKind kind) {
        switch (kind) {
            case BinaryOperatorKind::BitAnd:
            case BinaryOperatorKind::CompoundBitAnd:
            case BinaryOperatorKind::BitOr:
            case BinaryOperatorKind::CompoundBitOr:
            case BinaryOperatorKind::BitXor:
            case BinaryOperatorKind::CompoundBitXor:
            case BinaryOperatorKind::Shl:
            case BinaryOperatorKind::CompoundShl:
            case BinaryOperatorKind::Shr:
            case BinaryOperatorKind::CompoundShr: return true;

            default: return false;
        }
    }

    enum class ConstExprKind {
        Error = 0,
        Boolean,
        Integer,
        Floating,
        String,
        Struct,
        Typeid
    };

    inline const char* const_expr_kind_to_string(ConstExprKind kind) {
        switch (kind) {
            case ConstExprKind::Error: return "Error";
            case ConstExprKind::Boolean: return "Boolean";
            case ConstExprKind::Integer: return "Integer";
            case ConstExprKind::Floating: return "Floating";
            case ConstExprKind::String: return "String";
            case ConstExprKind::Struct: return "Struct";
            case ConstExprKind::Typeid: return "Typeid";

            default: ARIA_UNREACHABLE("Invalid const expr kind");
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

            default: ARIA_UNREACHABLE("Invalid expr value kind");
        }
    }

    enum class DeclKind {
        Invalid = 0,

        Error,
        Module,
        Import,
        Var,
        Param,
        Function,
        FunctionSpecilization,
        Struct,
        StructSpecilization,
        Impl,
        Typedef,
        Enum,
        EnumConstant,
        Field,
        Method,
        Destructor,
        Generic,
        GenericParameter
    };

    inline const char* decl_kind_to_string(DeclKind kind) {
        switch (kind) {
            case DeclKind::Error: return "Error";

            case DeclKind::Var: return "Var";
            case DeclKind::Param: return "Param";
            case DeclKind::Function: return "Function";
            case DeclKind::FunctionSpecilization: return "FunctionSpecilization";
            case DeclKind::Struct: return "Struct";
            case DeclKind::Typedef: return "Typedef";
            case DeclKind::Enum: return "Enum";
            case DeclKind::EnumConstant: return "EnumConstant";
            case DeclKind::Field: return "Field";
            case DeclKind::Method: return "Method";
            case DeclKind::Destructor: return "Destructor";
            case DeclKind::Generic: return "Generic";

            case DeclKind::GenericParameter: return "GenericParameter";

            default: ARIA_UNREACHABLE("Invalid decl kind");
        }
    }

    enum class DeclVisibility {
        Public,
        Private
    };

    inline const char* decl_visibility_to_string(DeclVisibility visibility) {
        switch (visibility) {
            case DeclVisibility::Public: return "public";
            case DeclVisibility::Private: return "private";

            default: ARIA_UNREACHABLE("Invalid decl visibility");
        }
    }

    enum class DeclAttributeKind {
        None,
        If,
        Builtin,
        Init
    };

    enum class StmtKind {
        Invalid = 0,

        Error,
        Nop,
        Block,
        While,
        DoWhile,
        For,
        If,
        Switch,
        Case,
        Break,
        Continue,
        Return,
        Defer,
        Expr,
        Decl
    };

    enum class SpecifierKind {
        Invalid = 0,

        Name
    };

    enum class TypeKind {
        Error = 0,
        Void,

        Bool,

        Char, IChar,
        Short, UShort,
        Int, UInt,
        Long, ULong,
        Sz, Isz,

        Float,
        Double,

        String,

        // Typeid is a built in type that is the same size as a 'sz'
        // Internally it is a pointer pointing to a struct of the following runtime layout:
        // name: String;
        // kind: TypeKind ('char' if TypeKind is not available);
        // size: sz;
        // inner: typeid;
        // len: sz;
        Typeid,

        // Any is a built in type that has the following runtime layout:
        // type: typeid;
        // value: *void;
        Any,

        Pointer,
        Array,
        Slice,

        Function,
        Method,

        Struct,
        Typedef,
        Enum,

        Generic,
        GenericDecl,
        StructSpecilization,

        Unresolved,
        Typeof,

        // Never is a special type which is only available for function return types
        // In code it is represented as: -> !
        // The underlying type for Never is void
        Never
    };

    enum class VariadicKind {
        None,

        Unnamed, // foo(int a, int b, ...);
                 // Supported only on functions marked 'extern'

        Named // foo(int a, int b, args...);
              // Supported on all functions
    };

    // WARNING: This enum must match exactly with TypeKind
    enum class RuntimeTypeKind : u8 {
        Void = 0,
        Bool,
        SignedInt,
        UnsignedInt,
        Float,
        String,
        Typeid,
        Any,
        Pointer,
        Array,
        Slice,
        Function,
        Struct,
        Union, // Not currently used, only exists to preserve backward compatibility when added
        Typedef,
        Enum
    };

} // namespace ariac