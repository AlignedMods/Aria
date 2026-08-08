#pragma once

#include "ariac/core.hpp"

#include <cstddef>
#include <utility>
#include <vector>

namespace ariac {

    // 16 megabytes
    constexpr size_t BLOCK_CAPACITY = 16777216;

    // The allocator which gets used internally
    // NOTE: This allocator WON'T call destructors, so NEVER store std::string, std::vector, etc in the compiler!
    class ArenaAllocator {
    private:
        struct Block {
            uint8_t* data = nullptr;
            size_t capacity = 0;
            size_t index = 0;
        };

    public:
        inline ArenaAllocator() {
            create_block();
        }

        inline ~ArenaAllocator() {
            for (auto& b : m_blocks) {
                delete b.data;
            }
            m_blocks.clear();
        }

        // Copying/moving an allocator is not valid
        // The only way to pass around an allocator is by pointer
        ArenaAllocator(const ArenaAllocator& other) = delete;
        ArenaAllocator(ArenaAllocator&& other) = delete;
        void operator=(const ArenaAllocator& other) = delete;
        void operator=(ArenaAllocator&& other) = delete;

        [[nodiscard]] inline void* allocate(size_t bytes) {
            ARIA_ASSERT(bytes < BLOCK_CAPACITY, "Object is too large");

            // We may need to create a new block for this object
            if (m_block->index + bytes >= m_block->capacity) { create_block(); }

            uint8_t* mem = &m_block->data[m_block->index];
            m_block->index += bytes;
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
        void create_block() {
            Block b;
            b.data = new uint8_t[BLOCK_CAPACITY];
            b.capacity = BLOCK_CAPACITY;
            m_blocks.push_back(b);
            m_block = &m_blocks.back();
        }

    private:
        std::vector<Block> m_blocks;
        Block* m_block = nullptr;
    };

} // namespace ariac
