#pragma once

#include "aria/core.hpp"

#include <cstddef>
#include <utility>

namespace Aria {

    // The allocator which gets used internally
    // NOTE: This allocator WON'T call destructors, so NEVER store std::string, std::vector, etc in the compiler!
    // If you don't provide an allocator you must call Aria::SetupDefaultAllocator() to set up a default one
    class ArenaAllocator {
    public:
        inline ArenaAllocator(size_t bytes)
            : m_capacity(bytes), m_data(new uint8_t[bytes]) {}

        inline ~ArenaAllocator() {
            delete[] m_data;
            m_data = nullptr;
            m_capacity = 0;
            m_index = 0;
        }

        // Copying/moving an allocator is not valid
        // The only way to pass around an allocator is by pointer
        ArenaAllocator(const ArenaAllocator& other) = delete;
        ArenaAllocator(ArenaAllocator&& other) = delete;
        void operator=(const ArenaAllocator& other) = delete;
        void operator=(ArenaAllocator&& other) = delete;

        [[nodiscard]] inline void* allocate(size_t bytes) {
            ARIA_ASSERT(m_index + bytes < m_capacity, "Too much memory allocated!");
            uint8_t* mem = &m_data[m_index];

            m_index += bytes;
            return reinterpret_cast<void*>(mem);
        }

        template <typename T>
        [[nodiscard]] inline T* allocate_named() {
            T* mem = reinterpret_cast<T*>(allocate(sizeof(T)));

            return new (mem) T{};
        }

        template <typename T, typename... Args>
        [[nodiscard]] inline T* allocate_named(Args&&... args) {
            T* mem = reinterpret_cast<T*>(allocate(sizeof(T)));

            return new (mem) T{std::forward<Args>(args)...};
        }

    private:
        uint8_t* m_data = nullptr;
        size_t m_capacity = 0;
        size_t m_index = 0;
    };

} // namespace Aria
