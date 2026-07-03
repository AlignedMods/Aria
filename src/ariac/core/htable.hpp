#pragma once

#include "ariac/core.hpp"

#include <string_view>

namespace ariac {

    void* alloc_arena(size_t size); // Defined in vector.cpp

    template <typename T>
    struct HTable {
    private:
        struct Node {
            std::string_view key;
            T value{};
            Node* next = nullptr;
        };

    public:
        inline void insert(std::string_view key, const T& value) {
            if (capacity == 0) {
                capacity = 32;
                items = reinterpret_cast<Node**>(alloc_arena(capacity * sizeof(Node*)));
                memset(items, 0, sizeof(Node*) * capacity);
            }

            size_t idx = hash(key);

            Node* new_node = reinterpret_cast<Node*>(alloc_arena(sizeof(Node)));
            new_node->key = key;
            new_node->value = value;
            new_node->next = nullptr;

            if (items[idx] == nullptr) {
                items[idx] = new_node;
            } else {
                new_node->next = items[idx];
                items[idx] = new_node;
            }
        }

        inline bool contains(std::string_view key) {
            if (capacity == 0) { return false; }

            size_t idx = hash(key);
            Node* head = items[idx];

            while (head != nullptr) {
                if (head->key == key) {
                    return true;
                }

                head = head->next;
            }

            return false;
        }

        inline T& at(std::string_view key) {
            size_t idx = hash(key);
            Node* head = items[idx];

            while (head != nullptr) {
                if (head->key == key) {
                    return head->value;
                }

                head = head->next;
            }

            ARIA_UNREACHABLE();
        }

    private:
        inline size_t hash(std::string_view key) {
            size_t sum = 0;
            size_t factor = 31;

            for (size_t i = 0; i < key.length(); i++) {
                sum = ((sum % capacity) + (key[i] * factor) % capacity) % capacity; 
                factor = ((factor % INT16_MAX) * (31 % INT16_MAX)) % INT16_MAX;
            }

            return sum;
        }

    public:
        size_t capacity = 0;
        Node** items = nullptr; // Base address of linked list
    };

} // namespace ariac