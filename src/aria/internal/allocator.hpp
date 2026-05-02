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
            : m_Capacity(bytes), m_Data(new uint8_t[bytes]) {}

        inline ~ArenaAllocator() {
            delete[] m_Data;
            m_Data = nullptr;
            m_Capacity = 0;
            m_Index = 0;
        }

        // Copying/moving an allocator is not valid
        // The only way to pass around an allocator is by pointer
        ArenaAllocator(const ArenaAllocator& other) = delete;
        ArenaAllocator(ArenaAllocator&& other) = delete;
        void operator=(const ArenaAllocator& other) = delete;
        void operator=(ArenaAllocator&& other) = delete;

        [[nodiscard]] inline void* allocate(size_t bytes) {
            ARIA_ASSERT(m_Index + bytes < m_Capacity, "Too much memory allocated!");
            uint8_t* mem = &m_Data[m_Index];

            m_Index += bytes;

            return reinterpret_cast<void*>(mem);
        }

        template <typename T>
        [[nodiscard]] inline T* allocate_named() {
            T* mem = reinterpret_cast<T*>(allocate(sizeof(T)));

            // return mem;
            return new (mem) T{};
        }

        template <typename T, typename... Args>
        [[nodiscard]] inline T* allocate_named(Args&&... args) {
            T* mem = reinterpret_cast<T*>(allocate(sizeof(T)));

            return new (mem) T{std::forward<Args>(args)...};
        }

    private:
        uint8_t* m_Data = nullptr;
        size_t m_Capacity = 0;
        size_t m_Index = 0;
    };

} // namespace Aria
