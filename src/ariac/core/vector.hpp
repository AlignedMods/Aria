#pragma once

#include <cstring>

namespace Aria::Internal {

    struct CompilationContext;

    void* alloc_arena(CompilationContext* ctx, size_t size);

    template <typename T>
    struct TinyVector {
        using iterator = T*;
        using const_iterator = T*;

        T* items = nullptr;
        size_t capacity = 0;
        size_t size = 0;
    
        inline void append(CompilationContext* ctx, T t) {
            if (capacity == 0) {
                items = reinterpret_cast<T*>(alloc_arena(ctx, sizeof(T) * 32));
                capacity = 32;
            }
    
            if (size >= capacity) {
                capacity *= 2;
    
                T* newitems = reinterpret_cast<T*>(alloc_arena(ctx, sizeof(T) * capacity));
                memcpy(newitems, items, sizeof(T) * size);
                items = newitems;
            }
    
            items[size] = t;
            size++;
        }

        inline iterator begin() { return items; }
        inline const_iterator begin() const { return items; }

        inline iterator end() { return items + size; }
        inline const_iterator end() const { return items + size; }

        inline iterator rbegin() { return items + size - 1; }
        inline const_iterator rbegin() const { return items + size - 1; }

        inline iterator rend() { return items - 1; }
        inline const_iterator rend() const { return items - 1; }
    };

} // namespace Aria::Internal
