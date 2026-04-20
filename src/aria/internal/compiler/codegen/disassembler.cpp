#include "aria/internal/compiler/codegen/disassembler.hpp"

#include <string>

#define PROG_SIZE() (m_Program->OpCodeTable.size())
#define PROG_OP(idx) (m_Program->OpCodeTable.at(idx))
#define GET_STR(idx) (std::string_view(m_Program->StringTable[idx]))
#define GET_TYPE(idx) (m_Program->TypeTable[idx])

namespace Aria::Internal {

    Disassembler::Disassembler(const OpCodes& ops, bool verbose) {
        m_Program = &ops;
        m_Verbose = verbose;

        DisassembleImpl();
    }

    std::string& Disassembler::GetDisassembly() {
        return m_Output;
    }

    void Disassembler::DisassembleImpl() {
        for (const auto& op : m_Program->OpCodeTable) {
            m_Output += fmt::format("{:05d}: ", m_ProgramCounter);

            DisassembleOpCode(op);
            m_ProgramCounter++;
        }
    }

    void Disassembler::DisassembleOpCode(const OpCode& op) {
        #define CASE_BINEXPR(_enum, str) case OpCodeKind::_enum: { \
            const VMType& type = GET_TYPE(op.Args[0].Index); \
            m_Output += fmt::format("    {}      {}\n", str, VMTypeToString(type)); \
            break; \
        }

        switch (op.Kind) {
            case OpCodeKind::Nop: m_Output += "    nop\n"; break;
        
            case OpCodeKind::Alloca: {
                const VMType& type = GET_TYPE(op.Args[0].Index);
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
                const VMType& type = GET_TYPE(op.Args[0].Index);
        
                switch (type.Kind) {
                    case VMTypeKind::I1:  m_Output += fmt::format("    ld.c      {} {}\n", VMTypeToString(type), op.Args[1].Boolean); break;
                    case VMTypeKind::I8:  m_Output += fmt::format("    ld.c      {} {}\n", VMTypeToString(type), op.Args[1].I8);  break;
                    case VMTypeKind::U8:  m_Output += fmt::format("    ld.c      {} {}\n", VMTypeToString(type), op.Args[1].U8);  break;
                    case VMTypeKind::I16: m_Output += fmt::format("    ld.c      {} {}\n", VMTypeToString(type), op.Args[1].I16); break;
                    case VMTypeKind::U16: m_Output += fmt::format("    ld.c      {} {}\n", VMTypeToString(type), op.Args[1].U16); break;
                    case VMTypeKind::I32: m_Output += fmt::format("    ld.c      {} {}\n", VMTypeToString(type), op.Args[1].I32); break;
                    case VMTypeKind::U32: m_Output += fmt::format("    ld.c      {} {}\n", VMTypeToString(type), op.Args[1].U32); break;
                    case VMTypeKind::I64: m_Output += fmt::format("    ld.c      {} {}\n", VMTypeToString(type), op.Args[1].I64); break;
                    case VMTypeKind::U64: m_Output += fmt::format("    ld.c      {} {}\n", VMTypeToString(type), op.Args[1].U64); break;
                    case VMTypeKind::F32: m_Output += fmt::format("    ld.c      {} {}\n", VMTypeToString(type), op.Args[1].Float); break;
                    case VMTypeKind::F64: m_Output += fmt::format("    ld.c      {} {}\n", VMTypeToString(type), op.Args[1].Double); break;
                    default: ARIA_ASSERT(false, "Invalid 'ldc' type");
                }
        
                break;
            }
        
            case OpCodeKind::LdStr: {
                std::string_view str = GET_STR(op.Args[0].Index);
                m_Output += fmt::format("    ldstr     \"{}\"\n", str);
                break;
            }
        
            case OpCodeKind::Deref: {
                const VMType& type = GET_TYPE(op.Args[0].Index);
                m_Output += fmt::format("    deref     {}\n", VMTypeToString(type));
                break;
            }
        
            case OpCodeKind::DeclareGlobal: {
                std::string_view str = GET_STR(op.Args[0].Index);
                m_Output += fmt::format("    decl.g    {}\n", str);
                break;
            }
        
            case OpCodeKind::DeclareLocal: {
                size_t index = op.Args[0].Index;
                m_Output += fmt::format("    decl.l    {}\n", index);
                break;
            }
        
            case OpCodeKind::DeclareArg: {
                size_t index = op.Args[0].Index;
                m_Output += fmt::format("    decl.arg  {}\n", index);
                break;
            }
        
            case OpCodeKind::LdGlobal: {
                std::string_view str = GET_STR(op.Args[0].Index);
                m_Output += fmt::format("    ld.g      {}\n", str);
                break;
            }
        
            case OpCodeKind::LdLocal: {
                size_t index = op.Args[0].Index;
                m_Output += fmt::format("    ld.l      {}\n", index);
                break;
            }
        
            case OpCodeKind::LdMember: {
                ARIA_TODO("LdMember op code");
                // OpCodeMember mem = std::get<OpCodeMember>(op.Data);
                // m_Output += fmt::format("    ld.mem    {} {} {}\n", mem.Index, VMTypeToString(mem.MemberType), VMTypeToString(mem.StructType));
                // break;
            }
        
            case OpCodeKind::LdArg: {
                size_t index = op.Args[0].Index;
                m_Output += fmt::format("    ld.arg    {}\n", index);
                break;
            }
        
            case OpCodeKind::LdFunc: {
                std::string_view str = GET_STR(op.Args[0].Index);
                m_Output += fmt::format("    ld.fn     {}\n", str);
                break;
            }
        
            case OpCodeKind::LdPtrGlobal: {
                std::string_view str = GET_STR(op.Args[0].Index);
                m_Output += fmt::format("    ldptr.g   {}\n", str);
                break;
            }
        
            case OpCodeKind::LdPtrLocal: {
                size_t index = op.Args[0].Index;
                m_Output += fmt::format("    ldptr.l   {}\n", index);
                break;
            }
        
            case OpCodeKind::LdPtrMember: {
                ARIA_TODO("LdPtrMember op code");
                // OpCodeMember mem = std::get<OpCodeMember>(op.Data);
                // m_Output += fmt::format("    ldptr.mem {} {} {}\n", mem.Index, VMTypeToString(mem.MemberType), VMTypeToString(mem.StructType));
                // break;
            }
        
            case OpCodeKind::LdPtrArg: {
                size_t index = op.Args[0].Index;
                m_Output += fmt::format("    ldptr.arg {}\n", index);
                break;
            }
        
            case OpCodeKind::LdPtrRet: {
                m_Output += "    ldptr.ret\n";
                break;
            }
        
            case OpCodeKind::Function: {
                std::string_view str = GET_STR(op.Args[0].Index);
                m_Output += fmt::format(".function {}:\n", str);
                break;
            }
        
            case OpCodeKind::Label: {
                std::string_view str = GET_STR(op.Args[0].Index);
                m_Output += fmt::format("{}:\n", str);
                break;
            }
        
            case OpCodeKind::Jmp: {
                std::string_view str = GET_STR(op.Args[0].Index);
                m_Output += fmt::format("    jmp       {}\n", str);
                break;
            }
        
            case OpCodeKind::Jt: {
                std::string_view str = GET_STR(op.Args[0].Index);
                m_Output += fmt::format("    jt        {}\n", str);
                break;
            }
        
            case OpCodeKind::JtPop: {
                std::string_view str = GET_STR(op.Args[0].Index);
                m_Output += fmt::format("    jtpop     {}\n", str);
                break;
            }
        
            case OpCodeKind::Jf: {
                std::string_view str = GET_STR(op.Args[0].Index);
                m_Output += fmt::format("    jf        {}\n", str);
                break;
            }
        
            case OpCodeKind::JfPop: {
                std::string_view str = GET_STR(op.Args[0].Index);
                m_Output += fmt::format("    jfpop     {}\n", str);
                break;
            }
        
            case OpCodeKind::Call: {
                size_t argCount = op.Args[0].Index;
                const VMType& type = GET_TYPE(op.Args[1].Index);

                m_Output += fmt::format("    call      {} {}\n", VMTypeToString(type), argCount);
                break;
            }
        
            case OpCodeKind::Ret: m_Output += "    ret\n"; break;
        
            case OpCodeKind::Neg: {
                const VMType& type = GET_TYPE(op.Args[0].Index);
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
                const VMType& type = GET_TYPE(op.Args[0].Index);
                m_Output += fmt::format("    cast      {}\n", VMTypeToString(type));
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

            case VMTypeKind::Struct: return m_Program->StructTable[type.Data].Name;

            default: ARIA_UNREACHABLE();
        }
    }

} // namespace Aria::Internal
