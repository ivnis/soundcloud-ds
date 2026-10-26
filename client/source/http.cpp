#include "http.h"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

static bool equal(const char* a, const char* b) {
    while (*a && *b) {
        if (std::tolower(static_cast<unsigned char>(*a++)) !=
            std::tolower(static_cast<unsigned char>(*b++))) return false;
    }
    return *a == *b;
}

static int hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}

static void decodeTitle(const char* source, char* target, std::size_t size) {
    std::size_t out = 0;
    while (*source && out + 1 < size) {
        unsigned char c = *source++;
        if (c == '%' && source[0] && source[1] && hex(source[0]) >= 0 && hex(source[1]) >= 0) {
            c = static_cast<unsigned char>(hex(source[0]) * 16 + hex(source[1]));
            source += 2;
        }
        target[out++] = c >= 32 ? static_cast<char>(c) : ' ';
    }
    target[out] = 0;
}

static bool decimal(const char* text, uint64_t& result) {
    if (!*text) return false;
    result = 0;
    while (*text) {
        if (*text < '0' || *text > '9') return false;
        unsigned digit = *text++ - '0';
        if (result > (std::numeric_limits<uint64_t>::max() - digit) / 10) return false;
        result = result * 10 + digit;
    }
    return true;
}

bool HttpAudio::lineByte(uint8_t byte, bool& complete) {
    complete = false;
    if (byte == '\n') {
        if (!line_size_ || line_[line_size_ - 1] != '\r') return fail("Invalid HTTP line");
        line_[--line_size_] = 0;
        complete = true;
        return true;
    }
    if (byte == 0 || line_size_ + 1 >= sizeof(line_)) return fail("HTTP line too long");
    line_[line_size_++] = static_cast<char>(byte);
    return true;
}

bool HttpAudio::header() {
    if (!status_seen_) {
        int code = 0;
        if (std::sscanf(line_, "HTTP/%*u.%*u %d", &code) != 1) return fail("Invalid HTTP status");
        status_code_ = code;
        if (code == 409) return fail("Server busy; retry");
        if (code == 416) return fail("Seek position is unavailable");
        if (code == 502 || code == 503 || code == 504) return fail("Audio source unavailable");
        if(code>=300 && code<400)return fail("Redirect unsupported. Use an HTTP URL.");
        if (code != 200) return fail("Server HTTP error");
        status_seen_ = true;
        return true;
    }
    if (!line_[0]) {
        if (pcm_ && (!valid_rate_ || !valid_channels_ || !valid_format_)) return fail("Unsupported PCM format");
        if (chunked_ && has_length_) return fail("Ambiguous HTTP framing");
        headers_done_ = true;
        if (has_length_ && content_length_ == 0) return fail("Empty audio stream");
        return true;
    }
    char* value = std::strchr(line_, ':');
    if (!value) return fail("Invalid HTTP header");
    *value++ = 0;
    while (*value == ' ' || *value == '\t') ++value;
    std::size_t len = std::strlen(value);
    while (len && (value[len - 1] == ' ' || value[len - 1] == '\t')) value[--len] = 0;
    if (equal(line_, "Transfer-Encoding")) {
        if (!equal(value, "chunked")) return fail("Unsupported HTTP encoding");
        chunked_ = true;
    } else if (equal(line_, "Content-Encoding") && !equal(value, "identity")) {
        return fail("Compressed HTTP body");
    } else if (equal(line_, "Content-Length")) {
        if (has_length_ || !decimal(value, content_length_)) return fail("Invalid Content-Length");
        has_length_ = true;
    } else if (equal(line_, "X-Audio-Rate")) valid_rate_ = equal(value, "22050");
    else if (equal(line_, "X-Audio-Channels")) valid_channels_ = equal(value, "1");
    else if (equal(line_, "X-Audio-Format")) valid_format_ = equal(value, "s16le");
    else if (equal(line_, "X-Track-Title")) decodeTitle(value, title_, sizeof(title_));
    else if (equal(line_, "X-Track-Artist")) decodeTitle(value, artist_, sizeof(artist_));
    else if (equal(line_, "X-Track-Duration-Ms")) {
        uint64_t number;
        if (!decimal(value, number) || number > 86400000) return fail("Invalid track duration");
        duration_ = static_cast<unsigned>(number);
    } else if (equal(line_, "X-Audio-Start-Ms")) {
        uint64_t number;
        if (!decimal(value, number) || number > 86400000) return fail("Invalid seek position");
        start_ms_ = static_cast<unsigned>(number);
    }
    return true;
}

bool HttpAudio::feed(const uint8_t* data, std::size_t count, Sink sink, void* context) {
    if (error_) return false;
    while (count && !done_) {
        if (!headers_done_) {
            if (++header_bytes_ > 8192) return fail("HTTP headers too large");
            bool complete;
            if (!lineByte(*data++, complete)) return false;
            --count;
            if (complete) {
                if (!header()) return false;
                line_size_ = 0;
            }
            continue;
        }
        if (chunked_ && (chunk_ == Chunk::Size || chunk_ == Chunk::Trailers)) {
            if (chunk_ == Chunk::Trailers && ++header_bytes_ > 16384) return fail("HTTP trailers too large");
            bool complete;
            if (!lineByte(*data++, complete)) return false;
            --count;
            if (complete) {
                if (chunk_ == Chunk::Trailers) {
                    if (line_size_ == 0) done_ = true;
                } else {
                    char* extension = std::strchr(line_, ';');
                    if (extension) *extension = 0;
                    if (!line_[0]) return fail("Invalid chunk size");
                    chunk_left_ = 0;
                    for (const char* p = line_; *p; ++p) {
                        int digit = hex(*p);
                        if (digit < 0 || chunk_left_ > 0xffffff) return fail("Invalid chunk size");
                        chunk_left_ = chunk_left_ * 16 + digit;
                    }
                    chunk_ = chunk_left_ ? Chunk::Data : Chunk::Trailers;
                }
                line_size_ = 0;
            }
            continue;
        }
        if (chunked_ && (chunk_ == Chunk::CR || chunk_ == Chunk::LF)) {
            if (*data++ != (chunk_ == Chunk::CR ? '\r' : '\n')) return fail("Invalid chunk delimiter");
            --count;
            chunk_ = chunk_ == Chunk::CR ? Chunk::LF : Chunk::Size;
            continue;
        }
        std::size_t take = count;
        uint64_t remaining = chunked_ ? chunk_left_ : has_length_ ? content_length_ - received_ : take;
        if (take > remaining) take = static_cast<std::size_t>(remaining);
        if (!sink(context, data, take)) return fail("Audio buffer overflow");
        received_ += take;
        data += take;
        count -= take;
        if (chunked_) {
            chunk_left_ -= take;
            if (!chunk_left_) chunk_ = Chunk::CR;
        } else if (has_length_ && received_ == content_length_) done_ = true;
    }
    return true;
}

bool HttpAudio::eof() {
    if (error_) return false;
    if (!headers_done_ || (chunked_ && !done_) || (has_length_ && received_ != content_length_))
        return fail("Audio connection interrupted");
    if (pcm_ && (!received_ || (received_ & 1))) return fail("Invalid PCM length");
    if (pcm_ && start_ms_ > duration_) return fail("Invalid seek position");
    if (pcm_ && duration_ - start_ms_ > 2000 && received_ * 1000 / 44100 + 2000 < duration_ - start_ms_)
        return fail("Audio ended too early");
    done_ = true;
    return true;
}
