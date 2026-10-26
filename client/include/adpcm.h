#pragma once
#include <cstddef>
#include <cstdint>

// Streaming ima4-ds-v1 decoder; fixed memory, arbitrary HTTP boundaries.
class AdpcmDecoder {
public:
    using Sink = bool (*)(void*, const uint8_t*, std::size_t);
    bool feed(const uint8_t* data, std::size_t size, Sink sink, void* context) {
        while (size--) {
            block_[used_++] = *data++;
            if (used_ == 6) {
                samples_ = block_[4] | (unsigned(block_[5]) << 8);
                if (!samples_ || samples_ > 512 || block_[2] > 88 || block_[3]) return false;
                needed_ = 6 + samples_ / 2;
            }
            if (used_ >= 6 && used_ == needed_) {
                int predictor = int(block_[0] | (unsigned(block_[1]) << 8));
                if (predictor >= 32768) predictor -= 65536;
                int index = block_[2];
                uint8_t pcm[1024];
                for (unsigned i = 0; i < samples_; ++i) {
                    if (i) {
                        unsigned code = (block_[6 + (i - 1) / 2] >> (((i - 1) & 1) * 4)) & 15;
                        int step = steps_[index];
                        int change = (step >> 3) + ((code & 4) ? step : 0) +
                            ((code & 2) ? step >> 1 : 0) + ((code & 1) ? step >> 2 : 0);
                        predictor += (code & 8) ? -change : change;
                        if (predictor < -32768) predictor = -32768;
                        if (predictor > 32767) predictor = 32767;
                        index += changes_[code & 7];
                        if (index < 0) index = 0;
                        if (index > 88) index = 88;
                    }
                    pcm[i * 2] = uint8_t(predictor);
                    pcm[i * 2 + 1] = uint8_t(unsigned(predictor) >> 8);
                }
                if (!sink(context, pcm, samples_ * 2)) return false;
                decoded_ += samples_ * 2;
                used_ = 0;
            }
        }
        return true;
    }
    bool complete() const { return used_ == 0; }
    uint64_t decodedBytes() const { return decoded_; }
private:
    uint8_t block_[262]{};
    unsigned used_ = 0, needed_ = 6, samples_ = 0;
    uint64_t decoded_ = 0;
    inline static constexpr int changes_[8] = {-1,-1,-1,-1,2,4,6,8};
    inline static constexpr int steps_[89] = {
        7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,
        50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,
        279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,
        1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,
        3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,
        11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767};
};
