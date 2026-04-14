#pragma once

#include "aria/internal/types.hpp"
#include "aria/core.hpp"

#include <string_view>
#include <array>
#include <vector>

namespace Aria::Internal {

    enum class VMTypeKind : u8 {
        Invalid = 0,
        Void,
        I1,
        I8, U8,
        I16, U16,
        I32, U32,
        I64, U64,
        F32, F64,
        Ptr,

        String,
        Struct
    };

    struct VMType {
        VMTypeKind Kind = VMTypeKind::Invalid;
        std::string_view Data;
        size_t Size = 0; // Size aligned to 8 bytes

        bool operator==(VMType& other) {
            if (Kind != VMTypeKind::Struct && other.Kind != VMTypeKind::Struct) {
                return Kind == other.Kind;
            }
        
            return Data == other.Data;
        }
    };

    enum class OpCodeKind : u16 {
        Nop,

        Alloca,
        Pop,
        Store,
        Dup,

        PushSF,
        PopSF,

        Ldc,
        LdStr,

        Deref,

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
        EndFunction,
        Label,
        Jmp,
        Jt,
        JtPop,
        Jf,
        JfPop,

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

        Shl,
        Shr,

        Cmp,
        Ncmp,
        Lt,
        Lte,
        Gt,
        Gte,

        Cast,

        Comment // Used only for debugging
    };

    union OpCodeArg {
        OpCodeArg()
            : U64(0) {}

        OpCodeArg(bool b)
            : Boolean(b) {}

        OpCodeArg(i8 i)
            : I8(i) {}

        OpCodeArg(i16 i)
            : I16(i) {}

        OpCodeArg(i32 i)
            : I32(i) {}

        OpCodeArg(i64 i)
            : I64(i) {}

        OpCodeArg(u8 u)
            : U8(u) {}

        OpCodeArg(u16 u)
            : U16(u) {}

        OpCodeArg(u32 u)
            : U32(u) {}

        OpCodeArg(u64 u)
            : U64(u) {}

        OpCodeArg(float f)
            : Float(f) {}

        OpCodeArg(double d)
            : Double(d) {}

        bool Boolean;
        i8 I8;
        u8 U8;
        i16 I16;
        u16 U16;
        i32 I32;
        u32 U32;
        i64 I64;
        u64 U64;

        float Float;
        double Double;

        size_t Index; // Used for types and strings
    };

    struct OpCode {
        OpCode() {}

        OpCode(OpCodeKind kind)
            : Kind(kind) {}

        OpCode(OpCodeKind kind, std::initializer_list<OpCodeArg> args) {
            ARIA_ASSERT(args.size() <= 3, "Cannot initialize op code with more than 3 arguments");

            for (size_t i = 0; i < args.size(); i++) {
                Args[i] = *(args.begin() + i);
            }

            Kind = kind;
        }

        OpCodeKind Kind = OpCodeKind::Add;
        std::array<OpCodeArg, 3> Args;
    };

    struct OpCodes {
        std::vector<std::string> StringTable;
        std::vector<VMType> TypeTable;

        std::vector<OpCode> OpCodeTable;
    };

} // namespace Aria::Internal
