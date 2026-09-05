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
        Copy,
        Move,
        MaterializeTemporary,
        Temporary,
        ExprWithCleanups,
        DefaultArg,
        Paren,
        Ternary,
        Cast,
        ImplicitCast,
        UnaryOperator,
        BinaryOperator,
        CompoundAssign,
        Const
    };

    enum class BuiltinCallKind {
        Sizeof,
        Defined
    };

    inline const char* builtin_call_kind_to_string(BuiltinCallKind kind) {
        switch (kind) {
            case BuiltinCallKind::Sizeof: return "sizeof";
            case BuiltinCallKind::Defined: return "defined";

            default: ARIA_UNREACHABLE("Invalid built in call");
        }
    }

    enum class CastKind {
        Invalid,

        IntegralCast,
        IntegralToBoolean,
        IntegralToFloating,
        IntegralToPointer,

        FloatingCast,
        FloatingToIntegral,
        FloatingToBoolean,

        BitCast,
        SliceToPointer,

        PointerToAny,
        AnyCast,

        LValueToRValue
    };

    inline const char* cast_kind_to_string(CastKind kind) {
        switch (kind) {
            case CastKind::Invalid: return "Invalid";

            case CastKind::IntegralCast: return "IntegralCast";
            case CastKind::IntegralToBoolean: return "IntegralToBoolean";
            case CastKind::IntegralToFloating: return "IntegralToFloating";
            case CastKind::IntegralToPointer: return "IntegralToPointer";

            case CastKind::FloatingCast: return "FloatingCast";
            case CastKind::FloatingToIntegral: return "FloatingToIntegral";
            case CastKind::FloatingToBoolean: return "FloatingToBoolean";

            case CastKind::BitCast: return "BitCast";
            case CastKind::SliceToPointer: return "SliceToPointer";

            case CastKind::PointerToAny: return "PointerToAny";
            case CastKind::AnyCast: return "AnyCast";

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
        PreIncrement, // ++i
        PreDecrement, // --i
        PostIncrement, // i++
        PostDecrement // i++
    };

    inline const char* unary_op_kind_to_string(UnaryOperatorKind kind) {
        switch (kind) {
            case UnaryOperatorKind::Invalid: return "invalid";
            
            case UnaryOperatorKind::Not: return "!";
            case UnaryOperatorKind::Negate: return "-";
            case UnaryOperatorKind::AddressOf: return "&";
            case UnaryOperatorKind::RValueAddressOf: return "&&";
            case UnaryOperatorKind::Dereference: return "*";
            case UnaryOperatorKind::PreIncrement: return "prefix ++";
            case UnaryOperatorKind::PreDecrement: return "prefix --";
            case UnaryOperatorKind::PostIncrement: return "postfix ++";
            case UnaryOperatorKind::PostDecrement: return "postfix --";

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
        Bool,
        Int,
        Float,
        String,
        Any,
        Struct,
        Array,
        Typeid
    };

    inline const char* const_expr_kind_to_string(ConstExprKind kind) {
        switch (kind) {
            case ConstExprKind::Error: return "Error";
            case ConstExprKind::Bool: return "Bool";
            case ConstExprKind::Int: return "Int";
            case ConstExprKind::Float: return "Float";
            case ConstExprKind::String: return "String";
            case ConstExprKind::Any: return "Any";
            case ConstExprKind::Struct: return "Struct";
            case ConstExprKind::Array: return "Array";
            case ConstExprKind::Typeid: return "Typeid";

            default: ARIA_UNREACHABLE("Invalid const expr kind");
        }
    }

    enum class ExprValueKind {
        LValue,
        RValue,
        XValue
    };

    inline const char* expr_value_kind_to_string(ExprValueKind type) {
        switch (type) {
            case ExprValueKind::LValue: return "lvalue";
            case ExprValueKind::RValue: return "rvalue";
            case ExprValueKind::XValue: return "xvalue";

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
        FunctionOverloadSet,
        Struct,
        StructSpecilization,
        Typedef,
        Enum,
        EnumConstant,
        Field,
        Method,
        Destructor,
        Template,
        TemplateParam
    };

    inline const char* decl_kind_to_string(DeclKind kind) {
        switch (kind) {
            case DeclKind::Error: return "Error";

            case DeclKind::Var: return "Var";
            case DeclKind::Param: return "Param";
            case DeclKind::Function: return "Function";
            case DeclKind::FunctionOverloadSet: return "FunctionOverloadSet";
            case DeclKind::Struct: return "Struct";
            case DeclKind::StructSpecilization: return "StructSpecilization";
            case DeclKind::Typedef: return "Typedef";
            case DeclKind::Enum: return "Enum";
            case DeclKind::EnumConstant: return "EnumConstant";
            case DeclKind::Field: return "Field";
            case DeclKind::Method: return "Method";
            case DeclKind::Destructor: return "Destructor";
            case DeclKind::Template: return "Template";
            case DeclKind::TemplateParam: return "TemplateParam";

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
        Init,
        Set
    };

    enum class ResolveStatus {
        NotStarted,
        InProgress,
        Done
    };

    enum class LinkageKind {
        None,
        Extern,
        Static
    };

    inline const char* linkage_kind_to_string(LinkageKind kind) {
        switch (kind) {
            case LinkageKind::None: return "none";
            case LinkageKind::Extern: return "extern";
            case LinkageKind::Static: return "static";

            default: ARIA_UNREACHABLE("Invalid linkage kind");
        }
    }

    enum class BuiltinFuncKind {
        None = 0,
        Memcpy,
        Memset
    };

    enum class StmtKind {
        Invalid = 0,

        Error,
        Nop,
        Compound,
        While,
        DoWhile,
        For,
        If,
        Switch,
        Case,
        Break,
        Continue,
        Nextcase,
        Return,
        Defer,
        CompileIf,
        Assert,
        Unreachable,
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

        Template,
        StructSpecilization,

        Unresolved,
        Dependent,
        DeducableTemplate,
        OverloadedFunction,
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