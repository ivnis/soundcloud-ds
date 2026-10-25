#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <algorithm>

// Single-threaded PCM window: retains recently played audio and limits the
// amount downloaded ahead. Positions are absolute samples within the track.
template <std::size_t Capacity, std::size_t HistoryBytes, std::size_t AheadBytes>
class SeekBuffer {
    static_assert(Capacity >= HistoryBytes + AheadBytes && Capacity % 2 == 0, "Invalid PCM window");
    uint8_t bytes_[Capacity]{};
    uint64_t first_ = 0, cursor_ = 0, end_ = 0;
    void trim() {
        uint64_t keep = cursor_ > HistoryBytes ? cursor_ - HistoryBytes : 0;
        if (first_ < keep) first_ = keep;
    }
public:
    void clear(uint64_t sample = 0) { first_ = cursor_ = end_ = sample * 2; }
    std::size_t size() const { return static_cast<std::size_t>(end_ - cursor_); }
    std::size_t free() const {
        if (end_ - cursor_ >= AheadBytes) return 0;
        return static_cast<std::size_t>(std::min<uint64_t>(Capacity - (end_ - first_), AheadBytes - (end_ - cursor_)));
    }
    uint64_t positionSamples() const { return cursor_ / 2; }
    uint64_t firstSample() const { return first_ / 2; }
    uint64_t endSample() const { return end_ / 2; }
    bool seekSamples(uint64_t sample) {
        uint64_t target = sample * 2;
        if (target < first_ || target > end_) return false;
        cursor_ = target;
        trim();
        return true;
    }
    bool advanceToSample(uint64_t sample) {
        if (seekSamples(sample)) return true;
        if (sample > endSample()) seekSamples(endSample());
        return false;
    }
    std::size_t write(const uint8_t* data, std::size_t count) {
        count = std::min(count, free());
        std::size_t index = end_ % Capacity, first = std::min(count, Capacity - index);
        std::memcpy(bytes_ + index, data, first);
        std::memcpy(bytes_, data + first, count - first);
        end_ += count;
        return count;
    }
    std::size_t read(uint8_t* data, std::size_t count) {
        count = std::min(count, size()) & ~std::size_t(1);
        std::size_t index = cursor_ % Capacity, first = std::min(count, Capacity - index);
        std::memcpy(data, bytes_ + index, first);
        std::memcpy(data + first, bytes_, count - first);
        cursor_ += count;
        trim();
        return count;
    }
};

inline unsigned seekStepMs(unsigned current, int delta, unsigned duration) {
    int64_t target = static_cast<int64_t>(current) + delta;
    return static_cast<unsigned>(std::max<int64_t>(0, std::min<int64_t>(duration, target)));
}

inline unsigned seekTouchMs(int x, unsigned duration) {
    int position = std::max(0, std::min(224, x - 16));
    return static_cast<unsigned>(static_cast<uint64_t>(position) * duration / 224);
}
