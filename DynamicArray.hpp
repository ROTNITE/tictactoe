#pragma once
#include <cstddef>
#include <stdexcept>
#include <utility>

template<typename T>
class DynamicArray {
private:
    T* data_;
    size_t size_;
    size_t capacity_;

    void resize(size_t newCapacity) {
        T* newData = new T[newCapacity];
        for (size_t i = 0; i < size_; ++i) {
            newData[i] = std::move(data_[i]);
        }
        delete[] data_;
        data_ = newData;
        capacity_ = newCapacity;
    }

public:
    DynamicArray() : data_(nullptr), size_(0), capacity_(0) {}
    
    explicit DynamicArray(size_t initialCapacity) 
        : data_(new T[initialCapacity]), size_(0), capacity_(initialCapacity) {}
    
    ~DynamicArray() {
        delete[] data_;
    }

    DynamicArray(const DynamicArray& other) 
        : data_(new T[other.capacity_]), size_(other.size_), capacity_(other.capacity_) {
        for (size_t i = 0; i < size_; ++i) {
            data_[i] = other.data_[i];
        }
    }

    DynamicArray(DynamicArray&& other) noexcept
        : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }

    DynamicArray& operator=(const DynamicArray& other) {
        if (this != &other) {
            delete[] data_;
            data_ = new T[other.capacity_];
            size_ = other.size_;
            capacity_ = other.capacity_;
            for (size_t i = 0; i < size_; ++i) {
                data_[i] = other.data_[i];
            }
        }
        return *this;
    }

    DynamicArray& operator=(DynamicArray&& other) noexcept {
        if (this != &other) {
            delete[] data_;
            data_ = other.data_;
            size_ = other.size_;
            capacity_ = other.capacity_;
            other.data_ = nullptr;
            other.size_ = 0;
            other.capacity_ = 0;
        }
        return *this;
    }

    void push_back(const T& value) {
        if (size_ >= capacity_) {
            size_t newCapacity = capacity_ == 0 ? 1 : capacity_ * 2;
            resize(newCapacity);
        }
        data_[size_++] = value;
    }

    void push_back(T&& value) {
        if (size_ >= capacity_) {
            size_t newCapacity = capacity_ == 0 ? 1 : capacity_ * 2;
            resize(newCapacity);
        }
        data_[size_++] = std::move(value);
    }

    void pop_back() {
        if (size_ > 0) {
            --size_;
        }
    }

    T& operator[](size_t index) {
        if (index >= size_) {
            throw std::out_of_range("Index out of range");
        }
        return data_[index];
    }

    const T& operator[](size_t index) const {
        if (index >= size_) {
            throw std::out_of_range("Index out of range");
        }
        return data_[index];
    }

    size_t size() const { return size_; }
    size_t capacity() const { return capacity_; }
    bool empty() const { return size_ == 0; }
    
    T* begin() { return data_; }
    T* end() { return data_ + size_; }
    const T* begin() const { return data_; }
    const T* end() const { return data_ + size_; }

    void clear() {
        size_ = 0;
    }

    void reserve(size_t newCapacity) {
        if (newCapacity > capacity_) {
            resize(newCapacity);
        }
    }
};
