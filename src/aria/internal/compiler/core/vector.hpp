#pragma once

#include <cstring>

namespace Aria::Internal {

    struct CompilationContext;

    void* alloc_arena(CompilationContext* ctx, size_t size);

    template <typename T>
    struct TinyVector {
        using iterator = T*;
        using const_iterator = T*;

        T* Items = nullptr;
        size_t Capacity = 0;
        size_t Size = 0;
    
        inline void append(CompilationContext* ctx, T t) {
            if (Capacity == 0) {
                Items = reinterpret_cast<T*>(alloc_arena(ctx, sizeof(T) * 32));
                Capacity = 32;
            }
    
            if (Size >= Capacity) {
                Capacity *= 2;
    
                T* newItems = reinterpret_cast<T*>(alloc_arena(ctx, sizeof(T) * Capacity));
                memcpy(newItems, Items, sizeof(T) * Size);
                Items = newItems;
            }
    
            Items[Size] = t;
            Size++;
        }

        inline iterator begin() { return Items; }
        inline const_iterator begin() const { return Items; }

        inline iterator end() { return Items + Size; }
        inline const_iterator end() const { return Items + Size; }

        inline iterator rbegin() { return Items + Size - 1; }
        inline const_iterator rbegin() const { return Items + Size - 1; }

        inline iterator rend() { return Items - 1; }
        inline const_iterator rend() const { return Items - 1; }
    };

} // namespace Aria::Internal
