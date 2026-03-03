#pragma once

#include "black_lua/internal/compiler/compilation_context.hpp"

namespace BlackLua::Internal {

    template <typename T>
    struct TinyVector {
        using iterator = T*;
        using const_iterator = T*;

        T* Items = nullptr;
        size_t Capacity = 0;
        size_t Size = 0;
    
        inline void Append(CompilationContext* ctx, T t) {
            if (Capacity == 0) {
                Items = reinterpret_cast<T*>(ctx->AllocateSized(sizeof(T) * 1));
                Capacity = 1;
            }
    
            if (Size >= Capacity) {
                Capacity *= 2;
    
                T* newItems = reinterpret_cast<T*>(ctx->AllocateSized(sizeof(T) * Capacity));
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
    };

} // namespace BlackLua::Internal
