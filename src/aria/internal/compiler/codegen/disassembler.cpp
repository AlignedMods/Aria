#include "aria/internal/compiler/codegen/disassembler.hpp"

#include <string>

namespace Aria::Internal {

    Disassembler::Disassembler(const std::vector<OpCode>* opcodes, bool verbose) {
        m_OpCodes = opcodes;
        m_Verbose = verbose;

        DisassembleImpl();
    }

    std::string& Disassembler::GetDisassembly() {
        return m_Output;
    }

    void Disassembler::DisassembleImpl() {
        for (const auto& op : *m_OpCodes) {
            m_Output += fmt::format("{:05d}: ", m_ProgramCounter);

            DisassembleOpCode(op);
            m_ProgramCounter++;
        }
    }

    void Disassembler::DisassembleOpCode(const OpCode& op) {
        #define CASE_LOAD(_enum, builtInType, str) case OpCodeType::_enum: { \
            OpCodeLoad l = std::get<OpCodeLoad>(op.Data); \
            m_Output += fmt::format("{}ld.{}    {}\n", m_Indentation, str, std::get<builtInType>(l.Data)); \
            break; \
        }

        #define CASE_UNARYEXPR(_enum, opStr, str) case OpCodeType::_enum: { \
            m_Output += fmt::format("{}{}.{}\n", m_Indentation, opStr, str); \
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
            m_Output += fmt::format("{}{}.{}\n", m_Indentation, opStr, str); \
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
            m_Output += fmt::format("{}cast.{}.{}\n", m_Indentation, opStr, str); \
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
                m_Output += "alloca    ";
                m_Output += std::to_string(a.Size);

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

            case OpCodeType::Store: {
                m_Output += fmt::format("{}store\n", m_Indentation);
                break;
            }

            case OpCodeType::Dup: {
                m_Output += fmt::format("{}dup\n", m_Indentation);
                break;
            }

            CASE_LOAD(LoadI8,  i8,  "i8 ")
            CASE_LOAD(LoadI16, i16, "i16")
            CASE_LOAD(LoadI32, i32, "i32")
            CASE_LOAD(LoadI64, i64, "i64")

            CASE_LOAD(LoadU8,  u8,  "u8 ")
            CASE_LOAD(LoadU16, u16, "u16")
            CASE_LOAD(LoadU32, u32, "u32")
            CASE_LOAD(LoadU64, u64, "u64")

            CASE_LOAD(LoadF32, f32, "f32");
            CASE_LOAD(LoadF64, f64, "f64");

            CASE_LOAD(LoadStr, StringView, "str")

            case OpCodeType::Deref: {
                size_t size = std::get<size_t>(op.Data);
                m_Output += fmt::format("{}deref     {}\n", m_Indentation, size);
                break;
            }

            case OpCodeType::DeclareGlobal: {
                const std::string& global = std::get<std::string>(op.Data);
                m_Output += fmt::format("{}decl.g    {}\n", m_Indentation, global);
                break;
            }

            case OpCodeType::DeclareLocal: {
                size_t index = std::get<size_t>(op.Data);
                m_Output += fmt::format("{}decl.l    {}\n", m_Indentation, index);
                break;
            }

            case OpCodeType::DeclareArg: {
                size_t index = std::get<size_t>(op.Data);
                m_Output += fmt::format("{}decl.arg  {}\n", m_Indentation, index);
                break;
            }

            case OpCodeType::LoadGlobal: {
                const std::string& global = std::get<std::string>(op.Data);
                m_Output += fmt::format("{}ld.g      {}\n", m_Indentation, global);
                break;
            }

            case OpCodeType::LoadLocal: {
                size_t index = std::get<size_t>(op.Data);
                m_Output += fmt::format("{}ld.l      {}\n", m_Indentation, index);
                break;
            }

            case OpCodeType::LoadOffset: {
                OpCodeOffset off = std::get<OpCodeOffset>(op.Data);
                m_Output += fmt::format("{}ld.off    {} {}\n", m_Indentation, off.Offset, off.Size);
                break;
            }

            case OpCodeType::LoadArg: {
                size_t index = std::get<size_t>(op.Data);
                m_Output += fmt::format("{}ld.arg    {}\n", m_Indentation, index);
                break;
            }

            case OpCodeType::LoadFunc: {
                const std::string& func = std::get<std::string>(op.Data);
                m_Output += fmt::format("{}ld.fn     {}\n", m_Indentation, func);
                break;
            }

            case OpCodeType::LoadPtrGlobal: {
                const std::string& global = std::get<std::string>(op.Data);
                m_Output += fmt::format("{}ldptr.g   {}\n", m_Indentation, global);
                break;
            }

            case OpCodeType::LoadPtrLocal: {
                size_t index = std::get<size_t>(op.Data);
                m_Output += fmt::format("{}ldptr.l   {}\n", m_Indentation, index);
                break;
            }

            case OpCodeType::LoadPtrOffset: {
                OpCodeOffset off = std::get<OpCodeOffset>(op.Data);
                m_Output += fmt::format("{}ldptr.off {} {}\n", m_Indentation, off.Offset, off.Size);
                break;
            }

            case OpCodeType::LoadPtrRet: {
                m_Output += fmt::format("{}ldptr.ret\n", m_Indentation);
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

                m_Output += fmt::format("{}jmp       {}\n", m_Indentation, name);
                break;
            }

            case OpCodeType::Jt: {
                const std::string& label = std::get<std::string>(op.Data);

                m_Output += fmt::format("{}jt        {}\n", m_Indentation, label);
                break;
            }

            case OpCodeType::Jf: {
                const std::string& label = std::get<std::string>(op.Data);

                m_Output += fmt::format("{}jf        {}\n", m_Indentation, label);
                break;
            }

            case OpCodeType::Call: {
                const OpCodeCall& call = std::get<OpCodeCall>(op.Data);

                m_Output += fmt::format("{}call      {}\n", m_Indentation, call.ArgCount);
                break;
            }

            case OpCodeType::Ret: m_Output += m_Indentation; m_Output += "ret\n"; break;

            CASE_UNARYEXPR_GROUP(Neg, "neg      ")

            CASE_BINEXPR_GROUP(Add, "add")
            CASE_BINEXPR_GROUP(Sub, "sub")
            CASE_BINEXPR_GROUP(Mul, "mul")
            CASE_BINEXPR_GROUP(Div, "div")
            CASE_BINEXPR_GROUP(Mod, "mod")

            CASE_BINEXPR_INTEGRAL_GROUP(And, "and")
            CASE_BINEXPR_INTEGRAL_GROUP(Or,  "or")
            CASE_BINEXPR_INTEGRAL_GROUP(Xor, "xor")

            CASE_BINEXPR_GROUP(Cmp,  "cmp")
            CASE_BINEXPR_GROUP(Ncmp, "ncmp")
            CASE_BINEXPR_GROUP(Lt,   "lt")
            CASE_BINEXPR_GROUP(Lte,  "lte")
            CASE_BINEXPR_GROUP(Gt,   "gt")
            CASE_BINEXPR_GROUP(Gte,  "gte")

            CASE_CAST_GROUP(I8,  "i8");
            CASE_CAST_GROUP(I16, "i16");
            CASE_CAST_GROUP(I32, "i32");
            CASE_CAST_GROUP(I64, "i64");
            CASE_CAST_GROUP(U8,  "u8");
            CASE_CAST_GROUP(U16, "u16");
            CASE_CAST_GROUP(U32, "u32");
            CASE_CAST_GROUP(U64, "u64");
            CASE_CAST_GROUP(F32, "f32");
            CASE_CAST_GROUP(F64, "f64");

            case OpCodeType::Comment: {
                const std::string& c = std::get<std::string>(op.Data);

                m_Output += fmt::format("{}// {}", m_Indentation, c);
                break;
            }
        }

        #undef CASE_UNARYEXPR
        #undef CASE_UNARYEXPR_GROUP
        #undef CASE_BINEXPR
        #undef CASE_BINEXPR_GROUP
        #undef CASE_CAST
        #undef CASE_CAST_GROUP
    }

} // namespace Aria::Internal
