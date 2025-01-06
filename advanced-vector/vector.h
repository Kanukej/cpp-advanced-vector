#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <algorithm>

#include <iostream>

template <typename T>
class RawMemory {
public:    
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }
    
    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;
    RawMemory(RawMemory&& other) noexcept {
        Swap(other);
    }
    RawMemory& operator=(RawMemory&& rhs) noexcept {
        if (this != &rhs) {
            Swap(rhs);
        }
        return *this;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:    
    using iterator = T*;
    using const_iterator = const T*;
    
    iterator begin() noexcept {
        return data_.GetAddress();
    }
    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }
    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator end() const noexcept {
        return data_.GetAddress() + size_;
    }
    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator cend() const noexcept {
        return data_.GetAddress() + size_;
    }
    
    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size)  //
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }
    
    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)  //
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }
    
    Vector(Vector&& other) {
        Swap(other);
    }
    
    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }
    
    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }
    
    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        SwapCopy(new_data);
    }
    
    void Resize(size_t new_size) {
        if (new_size < size_) {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
        } else {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() + new_size, new_size - size_);            
        }
        size_ = new_size;
    }
    
    void PushBack(const T& value) {
        EmplaceBack(value);
    }
    
    void PushBack(T&& value) {
        EmplaceBack(std::move(value));
    }
    
    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new (new_data + size_) T(std::forward<Args>(args)...);
            SwapCopy(new_data);
        } else {
            new (data_ + size_) T(std::forward<Args>(args)...);
        }
        ++size_;
        return data_[size_ - 1];
    }
    
    void PopBack() {
        if (size_ == 0) {
            return;
        }
        std::destroy_at(data_.GetAddress() + size_ - 1);
        --size_;
    }
    
    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }
    
    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }
    
    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        if (pos >= begin() && pos <= end()) {
            size_t idx = pos - begin();
            if (size_ == data_.Capacity()) {
                RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
                new (new_data + idx) T(std::forward<Args>(args)...);
                try {
                    CopyTo(new_data, 0, idx, 0);
                } catch (...) {
                    std::destroy_at(new_data + idx);
                    throw;
                }
                try {
                    CopyTo(new_data, idx, size_, idx + 1);
                } catch (...) {
                    std::destroy_n(new_data.GetAddress(), idx + 1);
                    throw;
                }
                std::destroy_n(data_.GetAddress(), size_);
                data_.Swap(new_data);
            } else {
                T tmp(std::forward<Args>(args)...);
                std::move_backward(begin() + idx, end(), end() + 1);
                new (data_ + idx) T(std::move(tmp));
            }
            ++size_;
            return begin() + idx;
        }
        return end();
    }
    
    iterator Erase(const_iterator pos) {
        if (pos >= begin() && pos <= end()) {
            size_t idx = pos - begin();
            std::destroy_at(data_ + idx);
            std::move(begin() + idx + 1, end(), begin() + idx);
            --size_;
            return begin() + idx;
        }
        return end();
    }
    
    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            } else {
                CopyFrom(rhs);
            }
        }
        return *this;
    }
    
    Vector& operator=(Vector&& rhs) {
        if (this != &rhs) {
            Swap(rhs);
        }
        return *this;
    }
    
    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

private:
    void CopyFrom(const Vector& rhs) {
        size_t cp_size = std::min(size_, rhs.size_);
        auto it = std::copy_n(rhs.data_.GetAddress(), cp_size, data_.GetAddress());
        if (size_ > rhs.size_) {
            std::destroy_n(it, size_ - cp_size);
        } else {
            std::uninitialized_copy_n(rhs.data_.GetAddress() + cp_size, rhs.size_ - cp_size, it);
        }
        size_ = rhs.size_;
    }
    void CopyTo(RawMemory<T>& new_data, size_t from, size_t to, size_t pos) {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress() + from, to - from, new_data.GetAddress() + pos);
        } else {
            std::uninitialized_copy_n(data_.GetAddress() + from, to - from, new_data.GetAddress() + pos);
        }
    }

    void SwapCopy(RawMemory<T>& new_data) {
        CopyTo(new_data, 0, size_, 0);
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }
    RawMemory<T> data_;
    size_t size_ = 0;
};
