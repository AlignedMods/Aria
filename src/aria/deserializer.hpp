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

        std::string

        void deserialize_header();
        void deserialize_constants();
        void deserialize_types();
        void deserialize_strings();
        void deserialize_code();

    private:
        u32 m_version_number = 0;
        u32 m_const_size = 0;
        u32 m_types_size = 0;
        u32 m_strings_size = 0;

        OpCodes m_op_codes;

        std::string m_source;
        size_t m_index = 0;
    };

} // namespace Aria::Internal