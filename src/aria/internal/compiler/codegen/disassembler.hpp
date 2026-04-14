#pragma once

#include "aria/internal/vm/op_codes.hpp"

#include <vector>

namespace Aria::Internal {

    class Disassembler {
    public:
        Disassembler(const OpCodes& ops, bool verbose);

        std::string& GetDisassembly();

    private:
        void DisassembleImpl();
        void DisassembleOpCode(const OpCode& op);

        std::string VMTypeToString(const VMType& type);

    private:
        const OpCodes* m_Program = nullptr;
        bool m_Verbose = false;
        size_t m_ProgramCounter = 0;

        std::string m_Output;
        std::string m_Indentation;
    };

} // namespace Aria::Internal
