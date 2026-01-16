
#pragma once
#include "DynamicArray.hpp"
#include <functional>
#include <utility>
#include <stdexcept>

template<typename K, typename V>
class HashMap {
private:
    class Entry {
    public:
        K key;
        V value;
        bool occupied;
        bool deleted;

        Entry() : occupied(false), deleted(false) {}
        Entry(const K& k, const V& v) : key(k), value(v), occupied(true), deleted(false) {}
    };

    DynamicArray<Entry> table_;
    size_t size_;
    size_t capacity_;
    std::hash<K> hasher_;

    static constexpr double MAX_LOAD_FACTOR = 0.7;
    static constexpr size_t INITIAL_CAPACITY = 16;

    size_t hash(const K& key) const {
        return hasher_(key) % capacity_;
    }

    size_t probe(size_t index, size_t i) const {
        return (index + i) % capacity_;
    }

    void rehash() {
        size_t oldCapacity = capacity_;
        DynamicArray<Entry> oldTable = std::move(table_);
        
        capacity_ = oldCapacity * 2;
        table_ = DynamicArray<Entry>(capacity_);
        for (size_t i = 0; i < capacity_; ++i) {
            table_.push_back(Entry());
        }
        size_ = 0;

        for (size_t i = 0; i < oldCapacity; ++i) {
            if (oldTable[i].occupied && !oldTable[i].deleted) {
                insert(oldTable[i].key, oldTable[i].value);
            }
        }
    }

public:
    HashMap() : size_(0), capacity_(INITIAL_CAPACITY) {
        table_ = DynamicArray<Entry>(capacity_);
        for (size_t i = 0; i < capacity_; ++i) {
            table_.push_back(Entry());
        }
    }

    void insert(const K& key, const V& value) {
        if (static_cast<double>(size_) / capacity_ >= MAX_LOAD_FACTOR) {
            rehash();
        }

        size_t index = hash(key);
        for (size_t i = 0; i < capacity_; ++i) {
            size_t pos = probe(index, i);
            Entry& entry = table_[pos];

            if (!entry.occupied || entry.deleted) {
                entry = Entry(key, value);
                ++size_;
                return;
            }

            if (entry.key == key) {
                entry.value = value;
                return;
            }
        }
    }

    bool contains(const K& key) const {
        size_t index = hash(key);
        for (size_t i = 0; i < capacity_; ++i) {
            size_t pos = probe(index, i);
            const Entry& entry = table_[pos];

            if (!entry.occupied) {
                return false;
            }

            if (entry.occupied && !entry.deleted && entry.key == key) {
                return true;
            }
        }
        return false;
    }
    V& get(const K& key) {
        size_t index = hash(key);
        for (size_t i = 0; i < capacity_; ++i) {
            size_t pos = probe(index, i);
            Entry& entry = table_[pos];

            if (!entry.occupied) {
                throw std::out_of_range("Key not found");
            }

            if (entry.occupied && !entry.deleted && entry.key == key) {
                return entry.value;
            }
        }
        throw std::out_of_range("Key not found");
    }

    const V& get(const K& key) const {
        return const_cast<HashMap*>(this)->get(key);
    }

    void remove(const K& key) {
        size_t index = hash(key);
        for (size_t i = 0; i < capacity_; ++i) {
            size_t pos = probe(index, i);
            Entry& entry = table_[pos];

            if (!entry.occupied) {
                return;
            }

            if (entry.occupied && !entry.deleted && entry.key == key) {
                entry.deleted = true;
                --size_;
                return;
            }
        }
    }

    size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }

    void clear() {
        for (size_t i = 0; i < capacity_; ++i) {
            table_[i] = Entry();
        }
        size_ = 0;
    }
};
