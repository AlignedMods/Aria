#include "black_lua/internal/compiler/codegen/disassembler.hpp"

#include <string>

namespace BlackLua::Internal {

    Disassembler::Disassembler(const std::vector<OpCode>* opcodes) {
        m_OpCodes = opcodes;

        DisassembleImpl();
    }

    std::string& Disassembler::GetDisassembly() {
        return m_Output;
    }

    void Disassembler::DisassembleImpl() {
        for (const auto& op : *m_OpCodes) {
            DisassembleOpCode(op);
        }
    }

    void Disassembler::DisassembleOpCode(const OpCode& op) {
        #define CASE_LOAD(_enum, builtInType, str) case OpCodeType::_enum: { \
            OpCodeLoad l = std::get<OpCodeLoad>(op.Data); \
            m_Output += fmt::format("{}load {} {}\n", m_Indentation, str, std::get<builtInType>(l.Data)); \
            break; \
        }

        #define CASE_UNARYEXPR(_enum, opStr, str) case OpCodeType::_enum: { \
            StackSlotIndex v = std::get<StackSlotIndex>(op.Data); \
            m_Output += fmt::format("{}{} {} ", m_Indentation, opStr, str); \
            DisassembleStackSlotIndex(v); \
            m_Output += '\n'; \
            break; \
        }

        #define CASE_UNARYEXPR_GROUP(unaryop, str) \
            CASE_UNARYEXPR(unaryop##I8,  str, "i8") \
            CASE_UNARYEXPR(unaryop##I16, str, "i16") \
            CASE_UNARYEXPR(unaryop##I32, str, "i32") \
            CASE_UNARYEXPR(unaryop##I64, str, "i64") \
            CASE_UNARYEXPR(unaryop##U8,  str, "u8") \
            CASE_UNARYEXPR(unaryop##U16, str, "u16") \
            CASE_UNARYEXPR(unaryop##U32, str, "u32") \
            CASE_UNARYEXPR(unaryop##U64, str, "u64") \
            CASE_UNARYEXPR(unaryop##F32, str, "f32") \
            CASE_UNARYEXPR(unaryop##F64, str, "f64")

        #define CASE_BINEXPR(_enum, opStr, str) case OpCodeType::_enum: { \
            OpCodeMath m = std::get<OpCodeMath>(op.Data); \
            m_Output += fmt::format("{}{} {} ", m_Indentation, opStr, str); \
            DisassembleStackSlotIndex(m.LHSSlot); m_Output += ' '; DisassembleStackSlotIndex(m.RHSSlot); \
            m_Output += '\n'; \
            break; \
        }

        #define CASE_BINEXPR_INTEGRAL_GROUP(mathop, str) \
            CASE_BINEXPR(mathop##I8,  str, "i8") \
            CASE_BINEXPR(mathop##I16, str, "i16") \
            CASE_BINEXPR(mathop##I32, str, "i32") \
            CASE_BINEXPR(mathop##I64, str, "i64") \
            CASE_BINEXPR(mathop##U8,  str, "u8") \
            CASE_BINEXPR(mathop##U16, str, "u16") \
            CASE_BINEXPR(mathop##U32, str, "u32") \
            CASE_BINEXPR(mathop##U64, str, "u64")

        #define CASE_BINEXPR_GROUP(mathop, str) \
            CASE_BINEXPR(mathop##I8,  str, "i8") \
            CASE_BINEXPR(mathop##I16, str, "i16") \
            CASE_BINEXPR(mathop##I32, str, "i32") \
            CASE_BINEXPR(mathop##I64, str, "i64") \
            CASE_BINEXPR(mathop##U8,  str, "u8") \
            CASE_BINEXPR(mathop##U16, str, "u16") \
            CASE_BINEXPR(mathop##U32, str, "u32") \
            CASE_BINEXPR(mathop##U64, str, "u64") \
            CASE_BINEXPR(mathop##F32, str, "f32") \
            CASE_BINEXPR(mathop##F64, str, "f64")

        #define CASE_CAST(_enum, opStr, str) case OpCodeType::_enum: { \
            OpCodeCast c = std::get<OpCodeCast>(op.Data); \
            m_Output += fmt::format("{}cast {} {} ", m_Indentation, opStr, str); \
            DisassembleStackSlotIndex(c.Slot); \
            m_Output += '\n'; \
            break; \
        }

        #define CASE_CAST_GROUP(_cast, str) \
            CASE_CAST(Cast##_cast##ToI8,  str, "i8") \
            CASE_CAST(Cast##_cast##ToI16, str, "i16") \
            CASE_CAST(Cast##_cast##ToI32, str, "i32") \
            CASE_CAST(Cast##_cast##ToI64, str, "i64") \
            CASE_CAST(Cast##_cast##ToU8,  str, "u8") \
            CASE_CAST(Cast##_cast##ToU16, str, "u16") \
            CASE_CAST(Cast##_cast##ToU32, str, "u32") \
            CASE_CAST(Cast##_cast##ToU64, str, "u64") \
            CASE_CAST(Cast##_cast##ToF32, str, "f32") \
            CASE_CAST(Cast##_cast##ToF64, str, "f64")

        switch (op.Type) {
            case OpCodeType::Nop: m_Output += "nop\n"; break;

            case OpCodeType::Push: {
                OpCodePush p = std::get<OpCodePush>(op.Data);

                m_Output += m_Indentation;
                m_Output += "push ";
                m_Output += std::to_string(p.Count);

                if (!op.DebugData.empty()) {
                    m_Output += "    ; ";
                    m_Output += op.DebugData;
                }

                m_Output += '\n';

                break;
            }

            case OpCodeType::Pop: m_Output += m_Indentation; m_Output += "pop\n"; break;
            case OpCodeType::PushStackFrame: m_Output += m_Indentation; m_Output += "push stack frame\n"; break;
            case OpCodeType::PopStackFrame: m_Output += m_Indentation; m_Output += "pop stack frame\n"; break;

            case OpCodeType::Get: {
                StackSlotIndex i = std::get<StackSlotIndex>(op.Data);

                m_Output += m_Indentation;
                m_Output += "get ";
                DisassembleStackSlotIndex(i);
                m_Output += '\n';

                break;
            }

            case OpCodeType::Copy: {
                OpCodeCopy c = std::get<OpCodeCopy>(op.Data);

                m_Output += m_Indentation;
                m_Output += "copy dst ";
                DisassembleStackSlotIndex(c.DstSlot);
                m_Output += " src ";
                DisassembleStackSlotIndex(c.SrcSlot);
                m_Output += '\n';

                break;
            }

            case OpCodeType::Dup: {
                StackSlotIndex i = std::get<StackSlotIndex>(op.Data);

                m_Output += m_Indentation;
                m_Output += "dup ";
                DisassembleStackSlotIndex(i);
                m_Output += '\n';

                break;
            }

            case OpCodeType::Ref: {
                StackSlotIndex i = std::get<StackSlotIndex>(op.Data);

                m_Output += m_Indentation;
                m_Output += "ref ";
                DisassembleStackSlotIndex(i);
                m_Output += '\n';

                break;
            }

            case OpCodeType::Offset: {
                OpCodeOffset o = std::get<OpCodeOffset>(op.Data);

                m_Output += m_Indentation;
                m_Output += "offset ";
                DisassembleStackSlotIndex(o.Slot);
                DisassembleStackSlotIndex(o.Offset);
                m_Output += std::to_string(o.Size);
                m_Output += '\n';

                break;
            }

            CASE_LOAD(LoadI8,  i8,  "i8")
            CASE_LOAD(LoadI16, i16, "i16")
            CASE_LOAD(LoadI32, i32, "i32")
            CASE_LOAD(LoadI64, i64, "i64")

            CASE_LOAD(LoadU8,  u8,  "u8")
            CASE_LOAD(LoadU16, u16, "u16")
            CASE_LOAD(LoadU32, u32, "u32")
            CASE_LOAD(LoadU64, u64, "u64")

            CASE_LOAD(LoadF32, f32, "f32");
            CASE_LOAD(LoadF64, f64, "f64");

            CASE_LOAD(LoadStr, StringView, "str")

            case OpCodeType::Label: {
                StackSlotIndex i = std::get<StackSlotIndex>(op.Data);

                m_Output += fmt::format("\n{}:    {}\n", std::get<StackSlotIndex>(op.Data).Slot, op.DebugData);
                
                m_Indentation.clear();
                m_Indentation.append(4, ' ');

                break;
            }

            case OpCodeType::Jmp: {
                StackSlotIndex i = std::get<StackSlotIndex>(op.Data);

                m_Output += m_Indentation;
                m_Output += "jmp ";
                m_Output += std::to_string(i.Slot);
                m_Output += '\n';

                break;
            }

            case OpCodeType::Jt: {
                OpCodeJump j = std::get<OpCodeJump>(op.Data);

                m_Output += m_Indentation;
                m_Output += "jt ";
                DisassembleStackSlotIndex(j.Slot);
                m_Output += " ";
                m_Output += std::to_string(j.Label);
                m_Output += '\n';

                break;
            }

            case OpCodeType::Jf: {
                OpCodeJump j = std::get<OpCodeJump>(op.Data);

                m_Output += m_Indentation;
                m_Output += "jf ";
                DisassembleStackSlotIndex(j.Slot);
                m_Output += " ";
                m_Output += std::to_string(j.Label);
                m_Output += '\n';

                break;
            }

            case OpCodeType::Call: {
                StackSlotIndex i = std::get<StackSlotIndex>(op.Data);

                m_Output += m_Indentation;
                m_Output += "call ";
                m_Output += std::to_string(i.Slot);
                m_Output += '\n';

                break;
            }

            case OpCodeType::CallExtern: {
                std::string s = std::get<std::string>(op.Data);

                m_Output += m_Indentation;
                m_Output += "call extern ";
                m_Output += s;
                m_Output += '\n';

                break;
            }

            case OpCodeType::Ret: m_Output += m_Indentation; m_Output += "ret\n"; break;

            CASE_UNARYEXPR_GROUP(Negate, "neg")

            CASE_BINEXPR_GROUP(Add, "add")
            CASE_BINEXPR_GROUP(Sub, "sub")
            CASE_BINEXPR_GROUP(Mul, "mul")
            CASE_BINEXPR_GROUP(Div, "div")
            CASE_BINEXPR_GROUP(Mod, "mod")

            CASE_BINEXPR_INTEGRAL_GROUP(And, "and")
            CASE_BINEXPR_INTEGRAL_GROUP(Or, "or")
            CASE_BINEXPR_INTEGRAL_GROUP(Xor, "xor")

            CASE_BINEXPR_GROUP(Cmp, "cmp")
            CASE_BINEXPR_GROUP(Ncmp, "ncmp")
            CASE_BINEXPR_GROUP(Lt, "lt")
            CASE_BINEXPR_GROUP(Lte, "lte")
            CASE_BINEXPR_GROUP(Gt, "gt")
            CASE_BINEXPR_GROUP(Gte, "gte")

            CASE_CAST_GROUP(I8, "i8");
            CASE_CAST_GROUP(I16, "i16");
            CASE_CAST_GROUP(I32, "i32");
            CASE_CAST_GROUP(I64, "i64");
            CASE_CAST_GROUP(U8, "u8");
            CASE_CAST_GROUP(U16, "u16");
            CASE_CAST_GROUP(U32, "u32");
            CASE_CAST_GROUP(U64, "u64");
            CASE_CAST_GROUP(F32, "f32");
            CASE_CAST_GROUP(F64, "f64");
        }

        #undef CASE_UNARYEXPR
        #undef CASE_UNARYEXPR_GROUP
        #undef CASE_BINEXPR
        #undef CASE_BINEXPR_GROUP
        #undef CASE_CAST
        #undef CASE_CAST_GROUP
    }

    void Disassembler::DisassembleStackSlotIndex(const StackSlotIndex& i) {
        if (i.Offset == 0 && i.Size == 0) {
            m_Output += "%(";
            m_Output += std::to_string(i.Slot);
            m_Output += ")";
        } else {
            m_Output += "(";
            m_Output += std::to_string(i.Slot);
            m_Output += ", ";
            m_Output += std::to_string(i.Offset);
            m_Output += ", ";
            m_Output += std::to_string(i.Size);
            m_Output += ")";
        }
    }

} // namespace BlackLua::Internal
