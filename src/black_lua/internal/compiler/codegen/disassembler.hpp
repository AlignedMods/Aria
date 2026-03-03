#pragma once

#include "black_lua/internal/vm/vm.hpp"

namespace BlackLua::Internal {

    class Disassembler {
    public:
        Disassembler(const std::vector<OpCode>* opcodes);

        std::string& GetDisassembly();

    private:
        void DisassembleImpl();
        void DisassembleOpCode(const OpCode& op);

        void DisassembleStackSlotIndex(const StackSlotIndex& i);

    private:
        const std::vector<OpCode>* m_OpCodes;

        std::string m_Output;
        std::string m_Indentation;
    };

} // namespace BlackLua::Internal
