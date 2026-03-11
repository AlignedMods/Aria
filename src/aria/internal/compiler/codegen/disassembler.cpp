#include "aria/internal/compiler/codegen/disassembler.hpp"

#include <string>

namespace Aria::Internal {

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
            m_Output += fmt::format("{}load{} {}\n", m_Indentation, str, std::get<builtInType>(l.Data)); \
            break; \
        }

        #define CASE_UNARYEXPR(_enum, opStr, str) case OpCodeType::_enum: { \
            MemRef mem = std::get<MemRef>(op.Data); \
            m_Output += fmt::format("{}{}{} {}\n", m_Indentation, opStr, str, DisassembleMemRef(mem)); \
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
            m_Output += fmt::format("{}{}{} {} {}\n", m_Indentation, opStr, str, DisassembleMemRef(m.LHSMem), DisassembleMemRef(m.RHSMem)); \
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
            m_Output += fmt::format("{}cast {} {} {}\n", m_Indentation, opStr, str, DisassembleMemRef(c.Mem)); \
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

            case OpCodeType::Alloca: {
                OpCodeAlloca a = std::get<OpCodeAlloca>(op.Data);

                m_Output += m_Indentation;
                m_Output += "alloca ";
                m_Output += std::to_string(a.Size);

                if (!op.DebugData.empty()) {
                    m_Output += "    ; ";
                    m_Output += op.DebugData;
                }

                m_Output += '\n';
                break;
            }

            case OpCodeType::PushSF: {
                m_Output += fmt::format("{}pushsf\n", m_Indentation);
                break;
            }
            case OpCodeType::PopSF: {
                m_Output += fmt::format("{}popsf\n", m_Indentation);
                break;
            }

            case OpCodeType::Copy: {
                OpCodeCopy c = std::get<OpCodeCopy>(op.Data);

                m_Output += fmt::format("{}copy {} {}\n", m_Indentation, DisassembleMemRef(c.DstMem), DisassembleMemRef(c.SrcMem));
                break;
            }

            case OpCodeType::Dup: {
                MemRef mem = std::get<MemRef>(op.Data);

                m_Output += fmt::format("{}dup {}\n", m_Indentation, DisassembleMemRef(mem));
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

            case OpCodeType::SetGlobal: {
                const OpCodeSetGlobal& global = std::get<OpCodeSetGlobal>(op.Data);
                m_Output += fmt::format("{}setglobal {}\n", m_Indentation, global.Name);
                break;
            }

            case OpCodeType::Function: {
                const std::string& name = std::get<std::string>(op.Data);
                m_Output += fmt::format(".function {}:\n", name);
                m_Indentation.clear();
                m_Indentation.append(4, ' ');
                break;
            }

            case OpCodeType::Label: {
                const std::string& name = std::get<std::string>(op.Data);

                m_Output += fmt::format("{}:\n", name);
                m_Indentation.clear();
                m_Indentation.append(4, ' ');
                break;
            }

            case OpCodeType::Jmp: {
                const std::string& name = std::get<std::string>(op.Data);

                m_Output += fmt::format("{}jmp {}\n", m_Indentation, name);
                break;
            }

            case OpCodeType::Jt: {
                const OpCodeConditionalJump& jump = std::get<OpCodeConditionalJump>(op.Data);

                m_Output += fmt::format("{}jt {}\n", m_Indentation, DisassembleMemRef(jump.Mem), jump.Label);
                break;
            }

            case OpCodeType::Jf: {
                const OpCodeConditionalJump& jump = std::get<OpCodeConditionalJump>(op.Data);

                m_Output += fmt::format("{}jf {}\n", m_Indentation, DisassembleMemRef(jump.Mem), jump.Label);
                break;
            }

            case OpCodeType::Call: {
                const OpCodeCall& call = std::get<OpCodeCall>(op.Data);

                m_Output += fmt::format("{}call {}\n", m_Indentation, DisassembleMemRef(call.Function));
                break;
            }

            case OpCodeType::CallExtern: {
                const OpCodeCall& call = std::get<OpCodeCall>(op.Data);

                m_Output += fmt::format("{}call extern {}\n", m_Indentation, DisassembleMemRef(call.Function));
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

    std::string Disassembler::DisassembleMemRef(const MemRef& mem) {
        if (mem.ContainsStackSlot()) {
            StackSlotRef slot = mem.GetStackSlot();
            return fmt::format("ss({}, {}, {})", slot.Slot, slot.Size, slot.Offset);
        } else if (mem.ContainsGlobalVar()) {
            const GlobalVarRef& global = mem.GetGlobalVar();
            return fmt::format("g({})", global.Name);
        } else if (mem.ContainsFunction()) {
            const FunctionRef& func = mem.GetFunction();
            return fmt::format("fn({})", func.Signature);
        }

        ARIA_UNREACHABLE();
    }

} // namespace Aria::Internal
