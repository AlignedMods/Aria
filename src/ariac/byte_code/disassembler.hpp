#pragma once

#include "common/op_codes.hpp"

#include <vector>

namespace Aria::Internal {

    class Disassembler {
    public:
        Disassembler(const OpCodes& ops);

        std::string& get_dissasembly();

    private:
        void dissasemble_impl();

        std::string vm_type_to_string(const VMType& type);

    private:
        const OpCodes* m_op_codes = nullptr;
        const OpCode* m_program_counter = nullptr;

        std::string m_output;
    };

} // namespace Aria::Internal
