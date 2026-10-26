#pragma once
#include <cstddef>
#include <cstdint>
#include "adpcm.h"

// Incremental, allocation-free HTTP response parser. Handles arbitrary TCP
// boundaries, chunk framing, and connection-close / content-length bodies.
class HttpAudio {
public:
    explicit HttpAudio(bool pcm = true) : pcm_(pcm) {}
    using Sink = bool (*)(void*, const uint8_t*, std::size_t);
    bool feed(const uint8_t* data, std::size_t count, Sink sink, void* context);
    bool eof();
    bool headersDone() const { return headers_done_; }
    bool done() const { return done_; }
    const char* error() const { return error_; }
    const char* title() const { return title_; }
    const char* artist() const { return artist_; }
    unsigned durationMs() const { return duration_; }
    unsigned startMs() const { return start_ms_; }
    uint64_t bodyBytes() const { return received_; }
    int statusCode() const { return status_code_; }
private:
    char line_[2048]{};
    std::size_t line_size_ = 0, header_bytes_ = 0;
    bool status_seen_ = false, headers_done_ = false, done_ = false;
    bool chunked_ = false, has_length_ = false;
    bool valid_rate_ = false, valid_channels_ = false, valid_format_ = false;
    uint64_t content_length_ = 0, received_ = 0, chunk_left_ = 0;
    enum class Chunk { Size, Data, CR, LF, Trailers } chunk_ = Chunk::Size;
    char title_[481] = "Connecting...", artist_[321]{};
    unsigned duration_ = 0;
    unsigned start_ms_ = 0;
    bool pcm_ = true;
    bool adpcm_ = false;
    AdpcmDecoder decoder_;
    int status_code_ = 0;
    const char* error_ = nullptr;
    bool fail(const char* message) { error_ = message; return false; }
    bool lineByte(uint8_t byte, bool& complete);
    bool header();
};
