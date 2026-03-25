#pragma once

#include "aria/internal/types.hpp"
#include "aria/internal/compiler/core/string_view.hpp"

#include <variant>
#include <vector>
#include <string_view>

namespace Aria::Internal {

    struct TypeInfo;

    enum class VMTypeKind {
        Invalid = 0,
        Void,
        I1,
        I8, U8,
        I16, U16,
        I32, U32,
        I64, U64,
        F32, F64,
        Ptr,

        Struct
    };

    struct VMType {
        VMTypeKind Kind = VMTypeKind::Invalid;
        std::string_view Data;

        bool operator==(VMType& other) {
            if (Kind != VMTypeKind::Struct && other.Kind != VMTypeKind::Struct) {
                return Kind == other.Kind;
            }

            return Data == other.Data;
        }
    };

    enum class OpCodeKind {
        Nop,

        Alloca,
        Pop,
        Store,
        Dup,

        PushSF,
        PopSF,

        Ldc,

        Deref,

        Struct,

        DeclareGlobal,
        DeclareLocal,
        DeclareArg,

        LdGlobal,
        LdLocal,
        LdMember,
        LdArg,
        LdFunc,

        LdPtrGlobal,
        LdPtrLocal,
        LdPtrMember,
        LdPtrArg,
        LdPtrRet,

        Function,
        Label,
        Jmp,
        Jt,
        Jf,

        Call,
        Ret,

        Neg,

        Add,
        Sub,
        Mul,
        Div,
        Mod,

        And,
        Or,
        Xor,

        Cmp,
        Ncmp,
        Lt,
        Lte,
        Gt,
        Gte,

        Cast,

        Comment // Used only for debugging
    };

    #undef TYPED_OP

    struct OpCodeLdc {
        std::variant<bool, i8, u8, i16, u16, i32, u32, i64, u64, f32, f64, std::string_view> Data;
        VMType Type;
    };

    struct OpCodeCall {
        size_t ArgCount = 0;
        VMType ReturnType;
    };

    struct OpCodeStruct {
        std::vector<VMType> Fields;
        std::string_view Identifier;
    };

    struct OpCodeMember {
        size_t Index = 0;
        VMType MemberType;
        VMType StructType;
    };

    struct OpCode {
        OpCodeKind Kind = OpCodeKind::Nop;
        std::variant<size_t, std::string, VMType, OpCodeLdc, OpCodeCall, OpCodeStruct, OpCodeMember> Data;
    };

} // namespace Aria::Internal
