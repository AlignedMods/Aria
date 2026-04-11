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
        #define CASE_BINEXPR(_enum, str) case OpCodeKind::_enum: { \
            const VMType& type = std::get<VMType>(op.Data); \
            m_Output += fmt::format("    {}      {}\n", str, VMTypeToString(type)); \
            break; \
        }

        switch (op.Kind) {
            case OpCodeKind::Nop: m_Output += "    nop\n"; break;

            case OpCodeKind::Alloca: {
                const VMType& type = std::get<VMType>(op.Data);
                m_Output += fmt::format("    alloca    {}\n", VMTypeToString(type));
                break;
            }

            case OpCodeKind::Pop: {
                m_Output += "    pop\n";
                break;
            }

            case OpCodeKind::PushSF: {
                m_Output += fmt::format("    pushsf\n");
                break;
            }
            case OpCodeKind::PopSF: {
                m_Output += fmt::format("    popsf\n");
                break;
            }

            case OpCodeKind::Store: {
                m_Output += fmt::format("    store\n");
                break;
            }

            case OpCodeKind::Dup: {
                m_Output += fmt::format("    dup\n");
                break;
            }

            case OpCodeKind::Ldc: {
                const OpCodeLdc& ldc = std::get<OpCodeLdc>(op.Data);

                switch (ldc.Type.Kind) {
                    case VMTypeKind::I1:  m_Output += fmt::format("    ld.c      {} {}\n", VMTypeToString(ldc.Type), std::get<bool>(ldc.Data)); break;
                    case VMTypeKind::I8:  m_Output += fmt::format("    ld.c      {} {}\n", VMTypeToString(ldc.Type), std::get<i8>(ldc.Data));  break;
                    case VMTypeKind::U8:  m_Output += fmt::format("    ld.c      {} {}\n", VMTypeToString(ldc.Type), std::get<u8>(ldc.Data));  break;
                    case VMTypeKind::I16: m_Output += fmt::format("    ld.c      {} {}\n", VMTypeToString(ldc.Type), std::get<i16>(ldc.Data)); break;
                    case VMTypeKind::U16: m_Output += fmt::format("    ld.c      {} {}\n", VMTypeToString(ldc.Type), std::get<u16>(ldc.Data)); break;
                    case VMTypeKind::I32: m_Output += fmt::format("    ld.c      {} {}\n", VMTypeToString(ldc.Type), std::get<i32>(ldc.Data)); break;
                    case VMTypeKind::U32: m_Output += fmt::format("    ld.c      {} {}\n", VMTypeToString(ldc.Type), std::get<u32>(ldc.Data)); break;
                    case VMTypeKind::I64: m_Output += fmt::format("    ld.c      {} {}\n", VMTypeToString(ldc.Type), std::get<i64>(ldc.Data)); break;
                    case VMTypeKind::U64: m_Output += fmt::format("    ld.c      {} {}\n", VMTypeToString(ldc.Type), std::get<u64>(ldc.Data)); break;
                    case VMTypeKind::F32: m_Output += fmt::format("    ld.c      {} {}\n", VMTypeToString(ldc.Type), std::get<f32>(ldc.Data)); break;
                    case VMTypeKind::F64: m_Output += fmt::format("    ld.c      {} {}\n", VMTypeToString(ldc.Type), std::get<f64>(ldc.Data)); break;
                    default: ARIA_ASSERT(false, "Invalid \"ldc\" type");
                }

                break;
            }

            case OpCodeKind::LdStr: {
                const std::string& str = std::get<std::string>(op.Data);
                m_Output += fmt::format("    ldstr     \"{}\"\n", str);

                break;
            }

            case OpCodeKind::Deref: {
                const VMType& type = std::get<VMType>(op.Data);
                m_Output += fmt::format("    deref     {}\n", VMTypeToString(type));
                break;
            }

            case OpCodeKind::Struct: {
                const OpCodeStruct& s = std::get<OpCodeStruct>(op.Data);
                m_Output += fmt::format("struct %{} [ ", s.Identifier);

                for (size_t i = 0; i < s.Fields.size(); i++) {
                    m_Output += VMTypeToString(s.Fields[i]);
                    if (i != s.Fields.size() - 1) { m_Output += ", "; }
                }

                m_Output += " ]\n";
                break;
            }

            case OpCodeKind::DeclareGlobal: {
                const std::string& global = std::get<std::string>(op.Data);
                m_Output += fmt::format("    decl.g    {}\n", global);
                break;
            }

            case OpCodeKind::DeclareLocal: {
                size_t index = std::get<size_t>(op.Data);
                m_Output += fmt::format("    decl.l    {}\n", index);
                break;
            }

            case OpCodeKind::DeclareArg: {
                size_t index = std::get<size_t>(op.Data);
                m_Output += fmt::format("    decl.arg  {}\n", index);
                break;
            }

            case OpCodeKind::LdGlobal: {
                const std::string& global = std::get<std::string>(op.Data);
                m_Output += fmt::format("    ld.g      {}\n", global);
                break;
            }

            case OpCodeKind::LdLocal: {
                size_t index = std::get<size_t>(op.Data);
                m_Output += fmt::format("    ld.l      {}\n", index);
                break;
            }

            case OpCodeKind::LdMember: {
                OpCodeMember mem = std::get<OpCodeMember>(op.Data);
                m_Output += fmt::format("    ld.mem    {} {} {}\n", mem.Index, VMTypeToString(mem.MemberType), VMTypeToString(mem.StructType));
                break;
            }

            case OpCodeKind::LdArg: {
                size_t index = std::get<size_t>(op.Data);
                m_Output += fmt::format("    ld.arg    {}\n", index);
                break;
            }

            case OpCodeKind::LdFunc: {
                const std::string& func = std::get<std::string>(op.Data);
                m_Output += fmt::format("    ld.fn     {}\n", func);
                break;
            }

            case OpCodeKind::LdPtrGlobal: {
                const std::string& global = std::get<std::string>(op.Data);
                m_Output += fmt::format("    ldptr.g   {}\n", global);
                break;
            }

            case OpCodeKind::LdPtrLocal: {
                size_t index = std::get<size_t>(op.Data);
                m_Output += fmt::format("    ldptr.l   {}\n", index);
                break;
            }

            case OpCodeKind::LdPtrMember: {
                OpCodeMember mem = std::get<OpCodeMember>(op.Data);
                m_Output += fmt::format("    ldptr.mem {} {} {}\n", mem.Index, VMTypeToString(mem.MemberType), VMTypeToString(mem.StructType));
                break;
            }

            case OpCodeKind::LdPtrArg: {
                size_t index = std::get<size_t>(op.Data);
                m_Output += fmt::format("    ldptr.arg {}\n", index);
                break;
            }

            case OpCodeKind::LdPtrRet: {
                m_Output += "    ldptr.ret\n";
                break;
            }

            case OpCodeKind::Function: {
                const std::string& name = std::get<std::string>(op.Data);
                m_Output += fmt::format(".function {}:\n", name);
                break;
            }

            case OpCodeKind::Label: {
                const std::string& name = std::get<std::string>(op.Data);

                m_Output += fmt::format("{}:\n", name);
                break;
            }

            case OpCodeKind::Jmp: {
                const std::string& name = std::get<std::string>(op.Data);

                m_Output += fmt::format("    jmp       {}\n", name);
                break;
            }

            case OpCodeKind::Jt: {
                const std::string& label = std::get<std::string>(op.Data);

                m_Output += fmt::format("    jt        {}\n", label);
                break;
            }

            case OpCodeKind::JtPop: {
                const std::string& label = std::get<std::string>(op.Data);

                m_Output += fmt::format("    jtpop     {}\n", label);
                break;
            }

            case OpCodeKind::Jf: {
                const std::string& label = std::get<std::string>(op.Data);

                m_Output += fmt::format("    jf        {}\n", label);
                break;
            }

            case OpCodeKind::JfPop: {
                const std::string& label = std::get<std::string>(op.Data);

                m_Output += fmt::format("    jfpop     {}\n", label);
                break;
            }

            case OpCodeKind::Call: {
                const OpCodeCall& call = std::get<OpCodeCall>(op.Data);

                m_Output += fmt::format("    call      {} {}\n", VMTypeToString(call.ReturnType), call.ArgCount);
                break;
            }

            case OpCodeKind::Ret: m_Output += "    ret\n"; break;

            case OpCodeKind::Neg: {
                const VMType& type = std::get<VMType>(op.Data);
                m_Output += fmt::format("    neg       {}\n", VMTypeToString(type));
                break;
            }

            CASE_BINEXPR(Add,  "add ");
            CASE_BINEXPR(Sub,  "sub ");
            CASE_BINEXPR(Mul,  "mul ");
            CASE_BINEXPR(Div,  "div ");
            CASE_BINEXPR(Mod,  "mod ");
                               
            CASE_BINEXPR(And,  "and ");
            CASE_BINEXPR(Or,   "or  ");
            CASE_BINEXPR(Xor,  "xor ");
            CASE_BINEXPR(Shl,  "shl ");
            CASE_BINEXPR(Shr,  "shr ");

            CASE_BINEXPR(Cmp,  "cmp ");
            CASE_BINEXPR(Ncmp, "ncmp");
            CASE_BINEXPR(Lt,   "lt  ");
            CASE_BINEXPR(Lte,  "lte ");
            CASE_BINEXPR(Gt,   "gt  ");
            CASE_BINEXPR(Gte,  "gte ");

            case OpCodeKind::Cast: {
                const VMType& type = std::get<VMType>(op.Data);
                m_Output += fmt::format("    cast      {}\n", VMTypeToString(type));
                break;
            }

            case OpCodeKind::Comment: {
                const std::string& c = std::get<std::string>(op.Data);

                m_Output += fmt::format("    // {}\n", c);
                break;
            }
        }
    }

    std::string Disassembler::VMTypeToString(const VMType& type) {
        switch (type.Kind) {
            case VMTypeKind::Void:   return "void";
                                     
            case VMTypeKind::I1:     return "i1";
            case VMTypeKind::I8:     return "i8";
            case VMTypeKind::U8:     return "u8";
            case VMTypeKind::I16:    return "i16";
            case VMTypeKind::U16:    return "u16";
            case VMTypeKind::I32:    return "i32";
            case VMTypeKind::U32:    return "u32";
            case VMTypeKind::I64:    return "i64";
            case VMTypeKind::U64:    return "u64";
                                     
            case VMTypeKind::F32:    return "f32";
            case VMTypeKind::F64:    return "f64";
                                     
            case VMTypeKind::Ptr:    return "ptr";

            case VMTypeKind::String: return "str";

            case VMTypeKind::Struct: return fmt::format("%{}", type.Data);

            default: ARIA_UNREACHABLE();
        }
    }

} // namespace Aria::Internal
