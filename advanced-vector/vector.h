#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <algorithm>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
            : buffer_(std::move(Allocate(capacity)))
            , capacity_(capacity) {
    }

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;

    RawMemory(RawMemory&& other) noexcept
    {
        Deallocate(buffer_);
        buffer_ = std::exchange(other.buffer_, nullptr);
        capacity_ = std::exchange(other.capacity_, 0);
    }
    RawMemory& operator=(RawMemory&& rhs) noexcept {
        RawMemory tmp(std::move(rhs));
        Swap(tmp);
        return *this;
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

    Vector() = default;

    explicit Vector(size_t size)
            : data_(size)
            , size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other)
            : data_(other.size_)
            , size_(other.size_)
    {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(other.data_.GetAddress(), size_, data_.GetAddress());
        } else {
            std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
        }
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

    Vector(Vector&& other) noexcept
    {
        data_ = std::move(other.data_);
        size_ = std::exchange(other.size_, 0);
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                /* Применить copy-and-swap */
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            } else {
                /* Скопировать элементы из rhs, создав при необходимости новые
                   или удалив существующие */
                if (rhs.size_ > size_) {
                    std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + size_, data_.GetAddress());
                    std::uninitialized_copy(rhs.data_.GetAddress() + size_, rhs.data_.GetAddress() + rhs.size_, data_.GetAddress() + size_);
                } else {
                    std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + rhs.size_, data_.GetAddress());
                    std::destroy(data_.GetAddress() + rhs.size_, data_.GetAddress() + size_);
                }
            }
            size_ = rhs.size_;
        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            data_.Swap(rhs.data_);
            std::swap(size_, rhs.size_);
        }
        return *this;
    }

    iterator begin() noexcept {
        return data_.GetAddress();
    }
    iterator end() noexcept {
        return begin() + size_;
    }
    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator end() const noexcept {
        return begin() + size_;
    }
    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator cend() const noexcept {
        return cbegin() + size_;
    }

    void Swap(Vector& other) noexcept {
        this->data_.Swap(other.data_);
        std::swap(other.size_, this->size_);
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
        // constexpr оператор if будет вычислен во время компиляции
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n( data_.GetAddress(), size_, new_data.GetAddress());
        } else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        data_.Swap(new_data);
        std::destroy_n(new_data.GetAddress(), size_);
    }

    void Resize(size_t new_size) {
        if (size_ < new_size) {
            if (Capacity() < new_size) {
                Reserve(new_size);
            }
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        } else {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
        }
        size_ = new_size;
    }
    void PushBack(const T& value) {
        EmplaceBack(value);
    }
    void PushBack(T&& value) {
        EmplaceBack(std::forward<T>(value));
    }
    void PopBack() /* noexcept */ {
        assert(Size() != 0);
        std::destroy_n(data_.GetAddress() + (size_ - 1), 1);
        --size_;
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        if (size_ == 0) {
            Reserve(1);
            new (data_.GetAddress()) T(std::forward<Args>(args)...);
        } else if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ * 2);
            new (new_data + size_) T(std::forward<Args>(args)...);

            try {
                if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                    std::uninitialized_move_n( data_.GetAddress(), size_, new_data.GetAddress());
                } else {
                    std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
                }
                std::destroy_n(data_.GetAddress(), size_);
                data_.Swap(new_data);
            } catch (...) {
                std::destroy_at(new_data.GetAddress() + size_);
                throw; // Пробрасываем исключение дальше
            }
        } else {
            new (data_ + size_) T(std::forward<Args>(args)...);
        }
        ++size_;

        return data_[size_ - 1];
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        assert(pos >= begin() && pos <= end());
        const size_t dist = pos - begin();

        if (dist == size_) {
            return &EmplaceBack(std::forward<Args>(args)...);
        } else {
            if (size_ >= Capacity()) {
                RawMemory<T> new_data(size_ * 2);
                new (new_data + dist) T(std::forward<Args>(args)...);

                try {
                    if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                        std::uninitialized_move_n(data_.GetAddress(),
                                                  dist,
                                                  new_data.GetAddress());
                        std::uninitialized_move_n(data_.GetAddress() + dist,
                                                  size_ - dist,
                                                  new_data.GetAddress() + dist + 1);
                    } else {
                        std::uninitialized_copy_n(data_.GetAddress(),
                                                  dist,
                                                  new_data.GetAddress());
                        try {
                            std::uninitialized_copy_n(data_.GetAddress() + dist,
                                                      size_ - dist,
                                                      new_data.GetAddress() + dist + 1);
                        } catch (...) {
                            std::destroy_n(new_data.GetAddress(), dist + 1);
                            throw;
                        }
                    }
                    std::destroy_n(data_.GetAddress(), size_);
                    data_.Swap(new_data);
                } catch (...) {
                    std::destroy_n(new_data.GetAddress() + dist, 1);
                    throw;
                }
            } else {
                try {
                    new (data_ + size_) T(std::move(*(end() - 1)));
                    std::move_backward(begin() + dist, end(), end() + 1);
                    std::destroy_n(data_.GetAddress() + dist, 1);
                    new (data_ + dist) T(std::forward<Args>(args)...);
                } catch (...) {
                    std::destroy_n(data_ + size_, 1);
                    throw;
                }
            }
        }
        ++size_;
        return &data_[dist];
    }

    iterator Erase(const_iterator pos) /*noexcept(std::is_nothrow_move_assignable_v<T>)*/ {
        assert(pos >= begin() && pos < end());
        const size_t dist = pos - begin();

        std::move(begin() + dist + 1, end(), begin() + dist);
        std::destroy_n(end() - 1, 1);
        --size_;
        return &data_[dist];
    }
    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }
    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::forward<T>(value));
    }
private:
    RawMemory<T> data_;
    size_t size_ = 0;

    // Вызывает деструкторы n объектов массива по адресу buf
    static void DestroyN(T* buf, size_t n) noexcept {
        for (size_t i = 0; i != n; ++i) {
            Destroy(buf + i);
        }
    }

    // Создаёт копию объекта elem в сырой памяти по адресу buf
    static void CopyConstruct(T* buf, const T& elem) {
        new (buf) T(elem);
    }

    // Вызывает деструктор объекта по адресу buf
    static void Destroy(T* buf) noexcept {
        buf->~T();
    }
};