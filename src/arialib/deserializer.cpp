#include "arialib/deserializer.hpp"

namespace Aria::Internal {
    
    Deserializer::Deserializer(const std::string& source) {
        m_source = source;
        deserialze_impl();
    }

    OpCodes& Deserializer::get_ops() {
        return m_op_codes;
    }

    void Deserializer::deserialze_impl() {
        deserialize_header();
        deserialize_constants();
        deserialize_types();
        deserialize_strings();
        deserialize_code();
    }

    u8 Deserializer::deserialize_u8() {
        ARIA_ASSERT(m_index + 1 < m_source.length(), "Invalid bytecode");
        return static_cast<u8>(m_source.at(m_index++));
    }

    u32 Deserializer::deserialize_u32() {
        ARIA_ASSERT(m_index + 4 < m_source.length(), "Invalid bytecode");

        return static_cast<u32>(m_source.at(m_index++)) >> 24 |
               static_cast<u32>(m_source.at(m_index++)) >> 16 |
               static_cast<u32>(m_source.at(m_index++)) >> 8 |
               static_cast<u32>(m_source.at(m_index++));
    }

    u64 Deserializer::deserialize_u64() {
        ARIA_ASSERT(m_index + 8 < m_source.length(), "Invalid bytecode");

        return static_cast<u64>(m_source.at(m_index++)) >> 56 |
               static_cast<u64>(m_source.at(m_index++)) >> 48 |
               static_cast<u64>(m_source.at(m_index++)) >> 40 |
               static_cast<u64>(m_source.at(m_index++)) >> 32 |
               static_cast<u64>(m_source.at(m_index++)) >> 24 |
               static_cast<u64>(m_source.at(m_index++)) >> 16 |
               static_cast<u64>(m_source.at(m_index++)) >> 8 |
               static_cast<u64>(m_source.at(m_index++));
    }

    float Deserializer::deserialize_float() {
        float f = 0.0f;
        u32 u = deserialize_u32();
        memcpy(&f, &u, sizeof(f));
        return f;
    }

    double Deserializer::deserialize_double() {
        double d = 0.0;
        u64 u = deserialize_u64();
        memcpy(&d, &u, sizeof(d));
        return d;
    }

    std::string_view Deserializer::deserialize_string(size_t length) {
        ARIA_ASSERT(m_index + length < m_source.length(), "Invalid bytecode");
        std::string_view str(m_source.data() + m_index, length);
        m_index += length;

        return str;
    }

    void Deserializer::deserialize_header() {
        ARIA_ASSERT(m_source.length() > 2, "Missing signature");
        std::string_view sig = deserialize_string(2);
        ARIA_ASSERT(sig == "AR", "Invalid signature");

        m_version_number = deserialize_u8();
        m_const_size = deserialize_u32();
        m_types_size = deserialize_u32();
        m_strings_size = deserialize_u32();
    }

    void Deserializer::deserialize_constants() {
        for (u32 i = 0; i < m_const_size; i++) {
            u8 kind = deserialize_u8();

            switch (kind) {
                case 0: m_op_codes.constant_table.push_back(deserialize_u64()); break;
                case 1: m_op_codes.constant_table.push_back(deserialize_float()); break;
                case 2: m_op_codes.constant_table.push_back(deserialize_double()); break;
                default: ARIA_UNREACHABLE();
            }
        }
    }

    void Deserializer::deserialize_types() {
        for (u32 i = 0; i < m_types_size; i++) {
            u64 kind = deserialize_u64();
            m_op_codes.type_table.push_back({ static_cast<VMTypeKind>(kind) });
        }
    }

    void Deserializer::deserialize_strings() {
        for (u32 i = 0; i < m_strings_size; i++) {
            u64 len = deserialize_u64();
            std::string_view str = deserialize_string(len);
            m_op_codes.string_table.push_back(std::string(str));
        }
    }

    void Deserializer::deserialize_code() {
        while (m_index + 1 < m_source.size()) {
            m_op_codes.program.push_back(static_cast<OpCode>(deserialize_u8()));
        }
    }

} // namespace Aria::Internal