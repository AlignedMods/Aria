#pragma once

#include "common/types.hpp"
#include "common/op_codes.hpp"

#include <string>

namespace Aria::Internal {
    
    class Deserializer {
    public:
        Deserializer(const std::string& source);

        OpCodes& get_ops();

    private:
        void deserialze_impl();

        u8 deserialize_u8();
        u32 deserialize_u32();
        u64 deserialize_u64();
        float deserialize_float();
        double deserialize_double();
        std::string_view deserialize_string(size_t length);

        void deserialize_header();
        void deserialize_constants();
        void deserialize_types();
        void deserialize_strings();
        void deserialize_code();

    private:
        u8 m_version_number = 0;
        u32 m_const_size = 0;
        u32 m_types_size = 0;
        u32 m_strings_size = 0;

        OpCodes m_op_codes;

        std::string m_source;
        size_t m_index = 0;
    };

} // namespace Aria::Internal