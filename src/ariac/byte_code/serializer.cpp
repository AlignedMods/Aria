#include "ariac/byte_code/serializer.hpp"

namespace Aria::Internal {

    Serialzier::Serialzier(CompilationContext* ctx) {
        m_context = ctx;
        m_op_codes = &ctx->ops;
        m_output = std::ofstream("output.ariac");

        if (!m_output) {
            fmt::println("Failed to open file '{}' for writing", "output.ariac");
            return;
        }

        serialize_impl();
    }

    void Serialzier::serialize_impl() {
        serialize_header();
        serialize_constants();
        serialize_types();
        serialize_strings();
        serialize_code();
    }

    void Serialzier::serialize_header() {
        m_output << "ARIAC";
        m_output << m_current_version;
        m_output << '\0';
        
        u32 const_size = 0;
        u32 type_size = 0;
        u32 string_size = 0;
        for (auto& _ : m_op_codes->constant_table) { const_size += 1; }
        for (auto& _ : m_op_codes->type_table) { type_size += 1; }
        for (auto& _ : m_op_codes->string_table) { string_size += 1; }

        m_output << const_size; m_output << '\0';
        m_output << type_size; m_output << '\0';
        m_output << string_size; m_output << '\0';
    }

    void Serialzier::serialize_constants() {
        for (auto& c : m_op_codes->constant_table) {
            if (std::holds_alternative<u64>(c)) {
                m_output << '\0';
                m_output << std::get<u64>(c);
                m_output << '\0';
            } else if (std::holds_alternative<float>(c)) {
                m_output << '\1';
                m_output << std::get<float>(c);
                m_output << '\0';
            } else if (std::holds_alternative<double>(c)) {
                m_output << '\2';
                m_output << std::get<double>(c);
                m_output << '\0';
            }
        }
    }

    void Serialzier::serialize_types() {
        for (auto& t : m_op_codes->type_table) {
            m_output << static_cast<u64>(t.kind);
            if (std::holds_alternative<VMStruct>(t.data)) {
                m_output << '\1';
            } else {
                m_output << '\0';
            }
        }
    }

    void Serialzier::serialize_strings() {
        for (auto& s : m_op_codes->string_table) {
            m_output << static_cast<u64>(s.length());
            m_output << '\0';
            
            m_output << s;
        }
    }

    void Serialzier::serialize_code() {
        for (OpCode op : m_op_codes->program) {
            m_output << static_cast<u16>(op);
            m_output << '\0';
        }
    }

} // namespace Aria::Internal