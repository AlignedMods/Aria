#pragma once

#include "ariac/compilation_context.hpp"

#include <fstream>

namespace Aria::Internal {
 
    class Serialzier {
    public:
        Serialzier(CompilationContext* ctx);

    private:
        void serialize_impl();

        void serialize_u32(u32 u);
        void serialize_u64(u64 u);
        void serialize_float(float f);
        void serialize_double(double d);

        void serialize_header();
        void serialize_constants();
        void serialize_types();
        void serialize_strings();
        void serialize_code();

    private:
        OpCodes* m_op_codes;
        std::ofstream m_output;

        const u8 m_current_version = 1;

        CompilationContext* m_context = nullptr;
    };

} // namespace Aria::Internal