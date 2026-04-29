#pragma once

#include "aria/internal/types.hpp"
#include "aria/core.hpp"

#include <string_view>
#include <array>
#include <vector>
#include <unordered_map>
#include <variant>

namespace Aria::Internal {

    enum class VMTypeKind {
        Void,
        I1,

        I8, U8,
        I16, U16,
        I32, U32,
        I64, U64,

        Float,
        Double,

        Ptr,
        Slice,

        Struct,
    };

    struct VMStruct {
        std::string_view Name;
        std::vector<size_t> Fields;
    };

    struct VMType {
        VMTypeKind Kind = VMTypeKind::Void;
        std::variant<bool, VMStruct> Data;
    };

    enum OpCode : u16 {
        OP_ALLOCA,

        OP_LD_CONST,
        OP_LD_STR,
        OP_LD_NULL,
        OP_LD,
        OP_LD_LOCAL,
        OP_LD_GLOBAL,

        OP_LD_PTR_LOCAL,
        OP_LD_PTR_GLOBAL,

        OP_ST_LOCAL,
        OP_ST_GLOBAL,
        OP_ST_ADDR,

        OP_POP,

        OP_DECL_LOCAL,
        OP_DECL_GLOBAL,

        OP_ADDI,
        OP_ADDU,
        OP_ADDF,
        OP_SUBI,
        OP_SUBU,
        OP_SUBF,
        OP_MULI,
        OP_MULU,
        OP_MULF,
        OP_DIVI,
        OP_DIVU,
        OP_DIVF,
        OP_MODI,
        OP_MODU,
        OP_MODF,

        OP_CMPI,
        OP_CMPU,
        OP_CMPF,
        OP_NCMPI,
        OP_NCMPU,
        OP_NCMPF,
        OP_LTI,
        OP_LTU,
        OP_LTF,
        OP_LTEI,
        OP_LTEU,
        OP_LTEF,
        OP_GTI,
        OP_GTU,
        OP_GTF,
        OP_GTEI,
        OP_GTEU,
        OP_GTEF,

        OP_SHLI,
        OP_SHLU,
        OP_SHRI,
        OP_SHRU,
        OP_ANDI,
        OP_ANDU,
        OP_ORI,
        OP_ORU,
        OP_XORI,
        OP_XORU,

        OP_NEGI,
        OP_NEGF,

        OP_LOGAND,
        OP_LOGOR,
        OP_LOGNOT,

        OP_JMP,
        OP_JT,
        OP_JF,
        OP_JT_POP,
        OP_JF_POP,

        OP_CONV_ITOI,
        OP_CONV_FTOF,
        OP_CONV_ITOF,
        OP_CONV_FTOI,

        OP_CALL,

        OP_RET,
        OP_RET_VAL,

        OP_FUNCTION,
        OP_ENDFUNCTION,
        OP_LABEL,
    };

    struct OpCodes {
        std::vector<std::string> StringTable;
        std::vector<VMType> TypeTable;
        std::vector<std::variant<u64, float, double>> ConstantTable;
        std::vector<OpCode> Program;
    };

} // namespace Aria::Internal
