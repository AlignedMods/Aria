#include "aria/internal/compiler/codegen/disassembler.hpp"

#include <string>

#define GET_STR() (std::string_view(m_op_codes->string_table[static_cast<size_t>(*(++m_program_counter))]))
#define GET_TYPE() (m_op_codes->type_table[static_cast<size_t>(*(++m_program_counter))])

namespace Aria::Internal {

    Disassembler::Disassembler(const OpCodes& ops) {
        m_op_codes = &ops;
        dissasemble_impl();
    }

    std::string& Disassembler::get_dissasembly() {
        return m_output;
    }

    void Disassembler::dissasemble_impl() {
        if (m_op_codes->program.size() == 0) { return; }
        m_program_counter = &m_op_codes->program.front();

        while (m_program_counter < &m_op_codes->program.back()) {
            m_output += fmt::format("{:05d}: ", m_program_counter - &m_op_codes->program.front());

            switch (*m_program_counter) {
                case OP_ALLOCA: {
                    auto& type = GET_TYPE();
                    m_output += fmt::format("    alloca {}", vm_type_to_string(type));
                    break;
                }

                case OP_NEW: {
                    auto& type = GET_TYPE();
                    m_output += fmt::format("    new {}", vm_type_to_string(type));
                    break;
                }

                case OP_NEW_ARR: {
                    auto& type = GET_TYPE();
                    m_output += fmt::format("    newarr {}", vm_type_to_string(type));
                    break;
                }

                case OP_FREE: {
                    m_output += "    free";
                    break;
                }

                case OP_LD_CONST: {
                    auto& type = GET_TYPE();
                    size_t idx = static_cast<size_t>(*(++m_program_counter));
                    
                    switch (type.kind) {
                        case VMTypeKind::I1: {
                            bool val = static_cast<bool>(std::get<u64>(m_op_codes->constant_table[idx]));
                            m_output += fmt::format("    ldc {} {}", vm_type_to_string(type), val);
                            break;
                        }

                        case VMTypeKind::I8:
                        case VMTypeKind::U8: {
                            u8 val = static_cast<u8>((std::get<u64>(m_op_codes->constant_table[idx])));
                            m_output += fmt::format("    ldc {} {}", vm_type_to_string(type), val);
                            break;
                        }

                        case VMTypeKind::I16:
                        case VMTypeKind::U16: {
                            u16 val = static_cast<u16>((std::get<u64>(m_op_codes->constant_table[idx])));
                            m_output += fmt::format("    ldc {} {}", vm_type_to_string(type), val);
                            break;
                        }

                        case VMTypeKind::I32:
                        case VMTypeKind::U32: {
                            u32 val = static_cast<u32>((std::get<u64>(m_op_codes->constant_table[idx])));
                            m_output += fmt::format("    ldc {} {}", vm_type_to_string(type), val);
                            break;
                        }

                        case VMTypeKind::I64:
                        case VMTypeKind::U64: {
                            u64 val = static_cast<u64>((std::get<u64>(m_op_codes->constant_table[idx])));
                            m_output += fmt::format("    ldc {} {}", vm_type_to_string(type), val);
                            break;
                        }

                        case VMTypeKind::Float: {
                            float val = std::get<float>(m_op_codes->constant_table[idx]);
                            m_output += fmt::format("    ldc {} {}", vm_type_to_string(type), val);
                            break;
                        }

                        case VMTypeKind::Double: {
                            double val = std::get<double>(m_op_codes->constant_table[idx]);
                            m_output += fmt::format("    ldc {} {}", vm_type_to_string(type), val);
                            break;
                        }
                    }

                    break;
                }

                case OP_LD_STR: {
                    std::string_view str = GET_STR();
                    m_output += fmt::format("    ldstr {:?}", str);
                    break;
                }

                case OP_LD_NULL: {
                    m_output += "    ldnull";
                    break;
                }

                case OP_LD: {
                    auto& type = GET_TYPE();
                    m_output += fmt::format("    ld {}", vm_type_to_string(type));
                    break;
                }

                case OP_LD_LOCAL: {
                    size_t idx = static_cast<size_t>(*(++m_program_counter));
                    m_output += fmt::format("    ldloc {}", idx);
                    break;
                }

                case OP_LD_GLOBAL: {
                    std::string_view str = GET_STR();
                    m_output += fmt::format("    ldglob {}", str);
                    break;
                }

                case OP_LD_FIELD: {
                    size_t idx = static_cast<size_t>(*(++m_program_counter));
                    auto& structType = GET_TYPE();

                    m_output += fmt::format("    ldfield {} {}", idx, vm_type_to_string(structType));
                    break;
                }

                case OP_LD_PTR_LOCAL: {
                    size_t idx = static_cast<size_t>(*(++m_program_counter));
                    m_output += fmt::format("    ldptrloc {}", idx);
                    break;
                }

                case OP_LD_PTR_GLOBAL: {
                    std::string_view str = GET_STR();
                    m_output += fmt::format("    ldptrglob {}", str);
                    break;
                }

                case OP_LD_PTR_FIELD: {
                    size_t idx = static_cast<size_t>(*(++m_program_counter));
                    auto& structType = GET_TYPE();

                    m_output += fmt::format("    ldptrfield {} {}", idx, vm_type_to_string(structType));
                    break;
                }

                case OP_LD_PTR: {
                    m_output += "    ldptr";
                    break;
                }

                case OP_ST_LOCAL: {
                    size_t idx = static_cast<size_t>(*(++m_program_counter));
                    m_output += fmt::format("    stloc {}", idx);
                    break;
                }

                case OP_ST_GLOBAL: {
                    std::string_view str = GET_STR();
                    m_output += fmt::format("    stglob {}", str);
                    break;
                }

                case OP_ST_FIELD: {
                    size_t idx = static_cast<size_t>(*(++m_program_counter));
                    auto& structType = GET_TYPE();

                    m_output += fmt::format("    stfield {} {}", idx, vm_type_to_string(structType));
                    break;
                }

                case OP_ST_ADDR: {
                    m_output += "    staddr";
                    break;
                }

                case OP_DUP: {
                    m_output += "    dup";
                    break;
                }

                case OP_POP: {
                    m_output += "    pop";
                    break;
                }

                case OP_DECL_LOCAL: {
                    size_t idx = static_cast<size_t>(*(++m_program_counter));
                    m_output += fmt::format("    decl.loc {}", idx);
                    break;
                }

                case OP_DECL_GLOBAL: {
                    std::string_view str = GET_STR();
                    m_output += fmt::format("    decl.glob {}", str);
                    break;
                }

                case OP_ADDI: {
                    m_output += "    addi";
                    break;
                }

                case OP_ADDU: {
                    m_output += "    addu";
                    break;
                }

                case OP_ADDF: {
                    m_output += "    addf";
                    break;
                }

                case OP_SUBI: {
                    m_output += "    subi";
                    break;
                }

                case OP_SUBU: {
                    m_output += "    subu";
                    break;
                }

                case OP_SUBF: {
                    m_output += "    subf";
                    break;
                }

                case OP_MULI: {
                    m_output += "    muli";
                    break;
                }

                case OP_MULU: {
                    m_output += "    mulu";
                    break;
                }

                case OP_MULF: {
                    m_output += "    mulf";
                    break;
                }

                case OP_DIVI: {
                    m_output += "    divi";
                    break;
                }

                case OP_DIVU: {
                    m_output += "    divu";
                    break;
                }

                case OP_DIVF: {
                    m_output += "    divf";
                    break;
                }

                case OP_MODI: {
                    m_output += "    modi";
                    break;
                }

                case OP_MODU: {
                    m_output += "    modu";
                    break;
                }

                case OP_MODF: {
                    m_output += "    modf";
                    break;
                }

                case OP_CMPI: {
                    m_output += "    cmpi";
                    break;
                }

                case OP_CMPU: {
                    m_output += "    cmpu";
                    break;
                }

                case OP_CMPF: {
                    m_output += "    cmpf";
                    break;
                }

                case OP_NCMPI: {
                    m_output += "    ncmpi";
                    break;
                }

                case OP_NCMPU: {
                    m_output += "    ncmpu";
                    break;
                }

                case OP_NCMPF: {
                    m_output += "    ncmpf";
                    break;
                }

                case OP_LTI: {
                    m_output += "    lti";
                    break;
                }

                case OP_LTU: {
                    m_output += "    ltu";
                    break;
                }

                case OP_LTF: {
                    m_output += "    ltf";
                    break;
                }

                case OP_LTEI: {
                    m_output += "    ltei";
                    break;
                }

                case OP_LTEU: {
                    m_output += "    lteu";
                    break;
                }

                case OP_LTEF: {
                    m_output += "    ltef";
                    break;
                }

                case OP_GTI: {
                    m_output += "    gti";
                    break;
                }

                case OP_GTU: {
                    m_output += "    gtu";
                    break;
                }

                case OP_GTF: {
                    m_output += "    gtf";
                    break;
                }

                case OP_GTEI: {
                    m_output += "    gtei";
                    break;
                }

                case OP_GTEU: {
                    m_output += "    gteu";
                    break;
                }

                case OP_GTEF: {
                    m_output += "    gtef";
                    break;
                }

                case OP_SHLI: {
                    m_output += "    shli";
                    break;
                }

                case OP_SHLU: {
                    m_output += "    shlu";
                    break;
                }

                case OP_SHRI: {
                    m_output += "    shri";
                    break;
                }

                case OP_SHRU: {
                    m_output += "    shru";
                    break;
                }

                case OP_ANDI: {
                    m_output += "    andi";
                    break;
                }

                case OP_ANDU: {
                    m_output += "    andu";
                    break;
                }

                case OP_ORI: {
                    m_output += "    ori";
                    break;
                }

                case OP_ORU: {
                    m_output += "    oru";
                    break;
                }

                case OP_XORI: {
                    m_output += "    xori";
                    break;
                }

                case OP_XORU: {
                    m_output += "    xoru";
                    break;
                }

                case OP_NEGI: {
                    m_output += "    negi";
                    break;
                }

                case OP_NEGF: {
                    m_output += "    negf";
                    break;
                }

                case OP_OFFP: {
                    auto& type = GET_TYPE();
                    m_output += fmt::format("    offp {}", vm_type_to_string(type));
                    break;
                }

                case OP_LOGAND: {
                    m_output += "    logand";
                    break;
                }

                case OP_LOGOR: {
                    m_output += "    logor";
                    break;
                }

                case OP_LOGNOT: {
                    m_output += "    lognot";
                    break;
                }
              
                case OP_JMP: {
                    std::string_view label = GET_STR();
                    m_output += fmt::format("    jmp {}", label);
                    break;
                }

                case OP_JT: {
                    std::string_view label = GET_STR();
                    m_output += fmt::format("    jt {}", label);
                    break;
                }

                case OP_JF: {
                    std::string_view label = GET_STR();
                    m_output += fmt::format("    jf {}", label);
                    break;
                }

                case OP_JT_POP: {
                    std::string_view label = GET_STR();
                    m_output += fmt::format("    jtpop {}", label);
                    break;
                }

                case OP_JF_POP: {
                    std::string_view label = GET_STR();
                    m_output += fmt::format("    jfpop {}", label);
                    break;
                }

                case OP_CONV_ITOI: {
                    auto& type = GET_TYPE();
                    m_output += fmt::format("    conv.itoi {}", vm_type_to_string(type));
                    break;
                }

                case OP_CONV_FTOF: {
                    auto& type = GET_TYPE();
                    m_output += fmt::format("    conv.ftof {}", vm_type_to_string(type));
                    break;
                }

                case OP_CONV_ITOF: {
                    auto& type = GET_TYPE();
                    m_output += fmt::format("    conv.itof {}", vm_type_to_string(type));
                    break;
                }

                case OP_CONV_FTOI: {
                    auto& type = GET_TYPE();
                    m_output += fmt::format("    conv.ftoi {}", vm_type_to_string(type));
                    break;
                }

                case OP_CALL: {
                    std::string_view sig = GET_STR();
                    m_output += fmt::format("    call '{}'", sig);
                    break;
                }

                case OP_RET: {
                    m_output += "    ret";
                    break;
                }

                case OP_RET_VAL: {
                    m_output += "    retval";
                    break;
                }

                case OP_FUNCTION: {
                    std::string_view sig = GET_STR();
                    m_output += fmt::format(".function '{}' {{", sig);
                    break;
                }

                case OP_ENDFUNCTION: {
                    m_output += "}";
                    break;
                }

                case OP_LABEL: {
                    std::string_view name = GET_STR();
                    m_output += fmt::format("{}:", name);
                    break;
                }
            }

            m_output += '\n';
            m_program_counter++;
        }
    }

    std::string Disassembler::vm_type_to_string(const VMType& type) {
        switch (type.kind) {
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
            case VMTypeKind::Slice: return "slice";

            case VMTypeKind::Struct: {
                return std::string(std::get<VMStruct>(type.data).name);
            }
        
            default: ARIA_UNREACHABLE();
        }
    }

} // namespace Aria::Internal
