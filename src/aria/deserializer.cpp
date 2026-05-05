#include "aria/deserializer.hpp"

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

    void Deserializer::deserialize_header() {
    }

} // namespace Aria::Internal