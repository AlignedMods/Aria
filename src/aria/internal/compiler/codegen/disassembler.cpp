#include "aria/internal/compiler/codegen/disassembler.hpp"

#include <string>

#define PROG_SIZE() (m_OpCodes->Program.size())
#define GET_STR() (std::string_view(m_OpCodes->StringTable[static_cast<size_t>(*(++m_ProgramCounter))]))
#define GET_TYPE() (m_OpCodes->TypeTable[static_cast<size_t>(*(++m_ProgramCounter))])

namespace Aria::Internal {

    Disassembler::Disassembler(const OpCodes& ops) {
        m_OpCodes = &ops;
        DisassembleImpl();
    }

    std::string& Disassembler::GetDisassembly() {
        return m_Output;
    }

    void Disassembler::DisassembleImpl() {
        m_ProgramCounter = &m_OpCodes->Program.front();

        while (m_ProgramCounter < &m_OpCodes->Program.back()) {
            m_Output += fmt::format("{:05d}: ", m_ProgramCounter - &m_OpCodes->Program.front());

            switch (*m_ProgramCounter) {
                case OP_ALLOCA: {
                    auto& type = GET_TYPE();
                    m_Output += fmt::format("    alloca {}", VMTypeToString(type));
                    break;
                }

                case OP_LD_CONST: {
                    auto& type = GET_TYPE();
                    size_t idx = static_cast<size_t>(*(++m_ProgramCounter));
                    
                    switch (type.Kind) {
                        case VMTypeKind::I1: {
                            bool val = static_cast<bool>(std::get<u64>(m_OpCodes->ConstantTable[idx]));
                            m_Output += fmt::format("    ldc {} {}", VMTypeToString(type), val);
                            break;
                        }

                        case VMTypeKind::I8:
                        case VMTypeKind::U8: {
                            u8 val = static_cast<u8>((std::get<u64>(m_OpCodes->ConstantTable[idx])));
                            m_Output += fmt::format("    ldc {} {}", VMTypeToString(type), val);
                            break;
                        }

                        case VMTypeKind::I16:
                        case VMTypeKind::U16: {
                            u16 val = static_cast<u16>((std::get<u64>(m_OpCodes->ConstantTable[idx])));
                            m_Output += fmt::format("    ldc {} {}", VMTypeToString(type), val);
                            break;
                        }

                        case VMTypeKind::I32:
                        case VMTypeKind::U32: {
                            u32 val = static_cast<u32>((std::get<u64>(m_OpCodes->ConstantTable[idx])));
                            m_Output += fmt::format("    ldc {} {}", VMTypeToString(type), val);
                            break;
                        }

                        case VMTypeKind::I64:
                        case VMTypeKind::U64: {
                            u64 val = static_cast<u64>((std::get<u64>(m_OpCodes->ConstantTable[idx])));
                            m_Output += fmt::format("    ldc {} {}", VMTypeToString(type), val);
                            break;
                        }

                        case VMTypeKind::Float: {
                            float val = std::get<float>(m_OpCodes->ConstantTable[idx]);
                            m_Output += fmt::format("    ldc {} {}", VMTypeToString(type), val);
                            break;
                        }

                        case VMTypeKind::Double: {
                            double val = std::get<double>(m_OpCodes->ConstantTable[idx]);
                            m_Output += fmt::format("    ldc {} {}", VMTypeToString(type), val);
                            break;
                        }
                    }

                    break;
                }

                case OP_LD_STR: {
                    std::string_view str = GET_STR();
                    m_Output += fmt::format("    ldstr {:?}", str);
                    break;
                }

                case OP_LD_LOCAL: {
                    size_t idx = static_cast<size_t>(*(++m_ProgramCounter));
                    m_Output += fmt::format("    ldloc {}", idx);
                    break;
                }

                case OP_LD_GLOBAL: {
                    std::string_view str = GET_STR();
                    m_Output += fmt::format("    ldglob {}", str);
                    break;
                }

                case OP_LD_PTR_LOCAL: {
                    size_t idx = static_cast<size_t>(*(++m_ProgramCounter));
                    m_Output += fmt::format("    ldptrloc {}", idx);
                    break;
                }

                case OP_LD_PTR_GLOBAL: {
                    std::string_view str = GET_STR();
                    m_Output += fmt::format("    ldptrglob {}", str);
                    break;
                }

                case OP_ST_LOCAL: {
                    size_t idx = static_cast<size_t>(*(++m_ProgramCounter));
                    m_Output += fmt::format("    stloc {}", idx);
                    break;
                }

                case OP_ST_GLOBAL: {
                    std::string_view str = GET_STR();
                    m_Output += fmt::format("    stglob {}", str);
                    break;
                }

                case OP_ST_ADDR: {
                    m_Output += "    staddr";
                    break;
                }

                case OP_POP: {
                    m_Output += "    pop";
                    break;
                }

                case OP_DECL_LOCAL: {
                    size_t idx = static_cast<size_t>(*(++m_ProgramCounter));
                    m_Output += fmt::format("    decl.loc {}", idx);
                    break;
                }

                case OP_DECL_GLOBAL: {
                    std::string_view str = GET_STR();
                    m_Output += fmt::format("    decl.glob {}", str);
                    break;
                }

                case OP_ADDI: {
                    m_Output += "    addi";
                    break;
                }

                case OP_ADDU: {
                    m_Output += "    addu";
                    break;
                }

                case OP_ADDF: {
                    m_Output += "    addf";
                    break;
                }

                case OP_SUBI: {
                    m_Output += "    subi";
                    break;
                }

                case OP_SUBU: {
                    m_Output += "    subu";
                    break;
                }

                case OP_SUBF: {
                    m_Output += "    subf";
                    break;
                }

                case OP_MULI: {
                    m_Output += "    muli";
                    break;
                }

                case OP_MULU: {
                    m_Output += "    mulu";
                    break;
                }

                case OP_MULF: {
                    m_Output += "    mulf";
                    break;
                }

                case OP_DIVI: {
                    m_Output += "    divi";
                    break;
                }

                case OP_DIVU: {
                    m_Output += "    divu";
                    break;
                }

                case OP_DIVF: {
                    m_Output += "    divf";
                    break;
                }

                case OP_MODI: {
                    m_Output += "    modi";
                    break;
                }

                case OP_MODU: {
                    m_Output += "    modu";
                    break;
                }

                case OP_MODF: {
                    m_Output += "    modf";
                    break;
                }

                case OP_CMPI: {
                    m_Output += "    cmpi";
                    break;
                }

                case OP_CMPU: {
                    m_Output += "    cmpu";
                    break;
                }

                case OP_CMPF: {
                    m_Output += "    cmpf";
                    break;
                }

                case OP_NCMPI: {
                    m_Output += "    ncmpi";
                    break;
                }

                case OP_NCMPU: {
                    m_Output += "    ncmpu";
                    break;
                }

                case OP_NCMPF: {
                    m_Output += "    ncmpf";
                    break;
                }

                case OP_LTI: {
                    m_Output += "    lti";
                    break;
                }

                case OP_LTU: {
                    m_Output += "    ltu";
                    break;
                }

                case OP_LTF: {
                    m_Output += "    ltf";
                    break;
                }

                case OP_LTEI: {
                    m_Output += "    ltei";
                    break;
                }

                case OP_LTEU: {
                    m_Output += "    lteu";
                    break;
                }

                case OP_LTEF: {
                    m_Output += "    ltef";
                    break;
                }

                case OP_GTI: {
                    m_Output += "    gti";
                    break;
                }

                case OP_GTU: {
                    m_Output += "    gtu";
                    break;
                }

                case OP_GTF: {
                    m_Output += "    gtf";
                    break;
                }

                case OP_GTEI: {
                    m_Output += "    gtei";
                    break;
                }

                case OP_GTEU: {
                    m_Output += "    gteu";
                    break;
                }

                case OP_GTEF: {
                    m_Output += "    gtef";
                    break;
                }

                case OP_SHLI: {
                    m_Output += "    shli";
                    break;
                }

                case OP_SHLU: {
                    m_Output += "    shlu";
                    break;
                }

                case OP_SHRI: {
                    m_Output += "    shri";
                    break;
                }

                case OP_SHRU: {
                    m_Output += "    shru";
                    break;
                }

                case OP_ANDI: {
                    m_Output += "    andi";
                    break;
                }

                case OP_ANDU: {
                    m_Output += "    andu";
                    break;
                }

                case OP_ORI: {
                    m_Output += "    ori";
                    break;
                }

                case OP_ORU: {
                    m_Output += "    oru";
                    break;
                }

                case OP_XORI: {
                    m_Output += "    xori";
                    break;
                }

                case OP_XORU: {
                    m_Output += "    xoru";
                    break;
                }

                case OP_NEGI: {
                    m_Output += "    negi";
                    break;
                }

                case OP_NEGF: {
                    m_Output += "    negf";
                    break;
                }

                case OP_LOGAND: {
                    m_Output += "    logand";
                    break;
                }

                case OP_LOGOR: {
                    m_Output += "    logor";
                    break;
                }

                case OP_LOGNOT: {
                    m_Output += "    lognot";
                    break;
                }
              
                case OP_JMP: {
                    std::string_view label = GET_STR();
                    m_Output += fmt::format("    jmp {}", label);
                    break;
                }

                case OP_JT: {
                    std::string_view label = GET_STR();
                    m_Output += fmt::format("    jt {}", label);
                    break;
                }

                case OP_JF: {
                    std::string_view label = GET_STR();
                    m_Output += fmt::format("    jf {}", label);
                    break;
                }

                case OP_JT_POP: {
                    std::string_view label = GET_STR();
                    m_Output += fmt::format("    jtpop {}", label);
                    break;
                }

                case OP_JF_POP: {
                    std::string_view label = GET_STR();
                    m_Output += fmt::format("    jfpop {}", label);
                    break;
                }

                case OP_CONV_ITOI: {
                    m_Output += "    conv.itoi";
                    break;
                }

                case OP_CONV_FTOF: {
                    m_Output += "    conv.ftof";
                    break;
                }

                case OP_CONV_ITOF: {
                    m_Output += "    conv.itof";
                    break;
                }

                case OP_CONV_FTOI: {
                    m_Output += "    conv.ftoi";
                    break;
                }

                case OP_CALL: {
                    std::string_view sig = GET_STR();
                    m_Output += fmt::format("    call '{}'", sig);
                    break;
                }

                case OP_RET: {
                    m_Output += "    ret";
                    break;
                }

                case OP_RET_VAL: {
                    m_Output += "    retval";
                    break;
                }

                case OP_FUNCTION: {
                    std::string_view sig = GET_STR();
                    m_Output += fmt::format(".function '{}' {{", sig);
                    break;
                }

                case OP_ENDFUNCTION: {
                    m_Output += "}";
                    break;
                }

                case OP_LABEL: {
                    std::string_view name = GET_STR();
                    m_Output += fmt::format("{}:", name);
                    break;
                }
            }

            m_Output += '\n';
            m_ProgramCounter++;
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
                                     
            case VMTypeKind::Float:  return "float";
            case VMTypeKind::Double: return "double";
                                     
            case VMTypeKind::Ptr:    return "ptr";
        
            case VMTypeKind::String: return "str";
        
            default: ARIA_UNREACHABLE();
        }
    }

} // namespace Aria::Internal
