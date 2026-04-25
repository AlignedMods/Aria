#pragma once

#include "aria/internal/vm/op_codes.hpp"

#include <vector>

namespace Aria::Internal {

    class Disassembler {
    public:
        Disassembler(const OpCodes& ops);

        std::string& GetDisassembly();

    private:
        void DisassembleImpl();

        std::string VMTypeToString(const VMType& type);

    private:
        const OpCodes* m_OpCodes = nullptr;
        const OpCode* m_ProgramCounter = nullptr;

        std::string m_Output;
        std::string m_Indentation;
    };

} // namespace Aria::Internal
