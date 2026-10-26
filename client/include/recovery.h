#pragma once
#include <cstdint>

// Count silence only while a stream actually wants data. Full buffers and
// pauses deliberately stop reads and must never trigger a reconnect.
class StreamRecovery {
public:
    void reset(unsigned tick) { progressAt_=tick; attempts_=stableBytes_=0; }
    void observe(unsigned tick, unsigned bytes, bool wantsData) {
        if (bytes || !wantsData) progressAt_=tick;
        if (bytes) {
            stableBytes_ += bytes;
            if (stableBytes_ >= 32768) { attempts_=0; stableBytes_=0; }
        }
    }
    bool stalled(unsigned tick, unsigned timeoutFrames = 60*5) const {
        return tick-progressAt_ >= timeoutFrames;
    }
    bool retry(unsigned tick) {
        if (attempts_ != 0xffffffffu) ++attempts_;
        stableBytes_=0; progressAt_=tick;
        return true;
    }
    unsigned retryDelayFrames() const {
        unsigned shift = attempts_ ? attempts_ - 1 : 0;
        if (shift > 2) shift = 2;
        return 15u << shift; // 0.25s, 0.5s, then a 1s ceiling.
    }
private:
    unsigned progressAt_=0, attempts_=0, stableBytes_=0;
};

// start_ms rounds down to whole milliseconds. Drop the small duplicated PCM
// prefix, using FFmpeg's nearest-sample rounding at the output rate.
struct ResumePoint { unsigned milliseconds; unsigned skipBytes; };
inline ResumePoint resumePoint(uint64_t sample, unsigned rate) {
    unsigned ms=static_cast<unsigned>(sample*1000/rate);
    uint64_t first=(static_cast<uint64_t>(ms)*rate+500)/1000;
    return {ms,static_cast<unsigned>((sample-first)*2)};
}
