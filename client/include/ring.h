#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

// Accessed only from the main thread (Maxmod is in manual update mode).
template <std::size_t Capacity> class Ring {
    uint8_t bytes_[Capacity]{};
    std::size_t head_ = 0, tail_ = 0, size_ = 0;
public:
    void clear() { head_ = tail_ = size_ = 0; }
    std::size_t size() const { return size_; }
    std::size_t free() const { return Capacity - size_; }
    std::size_t write(const uint8_t* data, std::size_t count) {
        if (count > free()) count = free();
        std::size_t first = count < Capacity - head_ ? count : Capacity - head_;
        std::memcpy(bytes_ + head_, data, first);
        std::memcpy(bytes_, data + first, count - first);
        head_ = (head_ + count) % Capacity;
        size_ += count;
        return count;
    }
    std::size_t read(uint8_t* data, std::size_t count) {
        if (count > size_) count = size_;
        std::size_t first = count < Capacity - tail_ ? count : Capacity - tail_;
        std::memcpy(data, bytes_ + tail_, first);
        std::memcpy(data + first, bytes_, count - first);
        tail_ = (tail_ + count) % Capacity;
        size_ -= count;
        return count;
    }
};
