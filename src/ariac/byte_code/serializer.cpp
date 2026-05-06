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

    void Serialzier::serialize_u32(u32 u) {
        m_output << static_cast<u8>((u << 24) & 0xff);
        m_output << static_cast<u8>((u << 16) & 0xff);
        m_output << static_cast<u8>((u << 8) & 0xff);
        m_output << static_cast<u8>(u & 0xff);
    }

    void Serialzier::serialize_u64(u64 u) {
        m_output << static_cast<u8>((u << 56) & 0xff);
        m_output << static_cast<u8>((u << 48) & 0xff);
        m_output << static_cast<u8>((u << 40) & 0xff);
        m_output << static_cast<u8>((u << 32) & 0xff);
        m_output << static_cast<u8>((u << 24) & 0xff);
        m_output << static_cast<u8>((u << 16) & 0xff);
        m_output << static_cast<u8>((u << 8) & 0xff);
        m_output << static_cast<u8>(u & 0xff);
    }

    void Serialzier::serialize_float(float f) {
        u32 u = 0;
        memcpy(&u, &f, sizeof(f));
        serialize_u32(u);
    }

    void Serialzier::serialize_double(double d) {
        u64 u = 0;
        memcpy(&u, &d, sizeof(d));
        serialize_u64(u);
    }

    void Serialzier::serialize_header() {
        m_output << "AR";
        m_output << m_current_version;
        
        u32 const_size = 0;
        u32 type_size = 0;
        u32 string_size = 0;
        for (auto& _ : m_op_codes->constant_table) { const_size += 1; }
        for (auto& _ : m_op_codes->type_table) { type_size += 1; }
        for (auto& _ : m_op_codes->string_table) { string_size += 1; }

        serialize_u32(const_size);
        serialize_u32(type_size);
        serialize_u32(string_size);
    }

    void Serialzier::serialize_constants() {
        for (auto& c : m_op_codes->constant_table) {
            if (std::holds_alternative<u64>(c)) {
                m_output << '\0';
                serialize_u64(std::get<u64>(c));
            } else if (std::holds_alternative<float>(c)) {
                m_output << '\1';
                serialize_float(std::get<float>(c));
            } else if (std::holds_alternative<double>(c)) {
                m_output << '\2';
                serialize_double(std::get<double>(c));
            }
        }
    }

    void Serialzier::serialize_types() {
        for (auto& t : m_op_codes->type_table) {
            serialize_u64(static_cast<u64>(t.kind));
        }
    }

    void Serialzier::serialize_strings() {
        for (auto& s : m_op_codes->string_table) {
            serialize_u64(static_cast<u64>(s.length()));
            m_output << s;
        }
    }

    void Serialzier::serialize_code() {
        for (OpCode op : m_op_codes->program) {
            m_output << static_cast<u8>(op);
        }
    }

} // namespace Aria::Internal