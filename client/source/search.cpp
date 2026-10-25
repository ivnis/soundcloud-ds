#include "search.h"
#include <cstdint>
#include <cstring>

namespace {
class Json {
    const char* p;
    const char* end;
    bool good = true;
    void ws() { while (p < end && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) ++p; }
    static int hex(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }
    uint32_t unicode() {
        uint32_t code = 0;
        for (int i = 0; i < 4; ++i) {
            if (p == end || hex(*p) < 0) { good = false; return 0; }
            code = code * 16 + hex(*p++);
        }
        return code;
    }
public:
    Json(const char* data, std::size_t size) : p(data), end(data + size) {}
    bool take(char c) { ws(); if (p < end && *p == c) { ++p; return true; } return false; }
    bool finished() { ws(); return good && p == end; }
    bool string(char* out, std::size_t capacity) {
        if (!take('"')) return false;
        std::size_t used = 0;
        auto put = [&](unsigned c) { if (capacity && used + 1 < capacity) out[used++] = static_cast<char>(c); };
        while (p < end && *p != '"') {
            unsigned char c = *p++;
            if (c < 32) return false;
            if (c != '\\') { put(c); continue; }
            if (p == end) return false;
            c = *p++;
            if (c == 'u') {
                uint32_t code = unicode();
                if (code >= 0xd800 && code <= 0xdbff) {
                    if (end - p < 2 || *p++ != '\\' || *p++ != 'u') return false;
                    uint32_t low = unicode();
                    if (low < 0xdc00 || low > 0xdfff) return false;
                    code = 0x10000 + ((code - 0xd800) << 10) + low - 0xdc00;
                } else if (code >= 0xdc00 && code <= 0xdfff) return false;
                if (!good) return false;
                if (code < 32) put(' ');
                else if (code < 128) put(code);
                else if (code < 2048) { put(0xc0 | (code >> 6)); put(0x80 | (code & 63)); }
                else if (code < 65536) { put(0xe0 | (code >> 12)); put(0x80 | ((code >> 6) & 63)); put(0x80 | (code & 63)); }
                else { put(0xf0 | (code >> 18)); put(0x80 | ((code >> 12) & 63)); put(0x80 | ((code >> 6) & 63)); put(0x80 | (code & 63)); }
            } else if (c == '"' || c == '\\' || c == '/') put(c);
            else if (c == 'n' || c == 'r' || c == 't' || c == 'b' || c == 'f') put(' ');
            else return false;
        }
        if (capacity) out[used] = 0;
        return take('"');
    }
    bool number(unsigned& result) {
        ws(); result = 0;
        if (p == end || *p < '0' || *p > '9') return false;
        do {
            unsigned digit = *p++ - '0';
            if (result > (0xffffffffu - digit) / 10) return false;
            result = result * 10 + digit;
        } while (p < end && *p >= '0' && *p <= '9');
        return true;
    }
    bool boolean(bool& value) {
        ws();
        if (end - p >= 4 && std::memcmp(p, "true", 4) == 0) { value = true; p += 4; return true; }
        if (end - p >= 5 && std::memcmp(p, "false", 5) == 0) { value = false; p += 5; return true; }
        return false;
    }
    bool skip(unsigned depth = 0) {
        if (depth > 8) return false;
        ws(); if (p == end) return false;
        if (*p == '"') return string(nullptr, 0);
        if (take('{')) {
            if (take('}')) return true;
            do { if (!string(nullptr, 0) || !take(':') || !skip(depth + 1)) return false; } while (take(','));
            return take('}');
        }
        if (take('[')) {
            if (take(']')) return true;
            do { if (!skip(depth + 1)) return false; } while (take(','));
            return take(']');
        }
        bool b;
        if (boolean(b)) return true;
        if (end - p >= 4 && std::memcmp(p, "null", 4) == 0) { p += 4; return true; }
        unsigned n; return number(n);
    }
};

bool track(Json& json, TrackItem& item) {
    if (!json.take('{')) return false;
    do {
        char key[32];
        if (!json.string(key, sizeof(key)) || !json.take(':')) return false;
        if (std::strcmp(key, "id") == 0) { if (!json.string(item.id, sizeof(item.id))) return false; }
        else if (std::strcmp(key, "title") == 0) { if (!json.string(item.title, sizeof(item.title))) return false; }
        else if (std::strcmp(key, "artist") == 0) { if (!json.string(item.artist, sizeof(item.artist))) return false; }
        else if (std::strcmp(key, "duration_ms") == 0) { if (!json.number(item.durationMs)) return false; }
        else if (!json.skip()) return false;
    } while (json.take(','));
    if (!json.take('}') || !item.id[0] || !item.title[0] || item.durationMs > 86400000) return false;
    for (char c : item.id) { if (!c) break; if (c < '0' || c > '9') return false; }
    return std::strlen(item.id) <= 20;
}
}

bool parseSearch(const char* data, std::size_t length, SearchResults& result) {
    result = SearchResults{};
    Json json(data, length);
    if (!json.take('{')) return false;
    bool tracksSeen = false;
    do {
        char key[32];
        if (!json.string(key, sizeof(key)) || !json.take(':')) return false;
        if (std::strcmp(key, "tracks") == 0) {
            if (tracksSeen || !json.take('[')) return false;
            tracksSeen = true;
            if (!json.take(']')) {
                do {
                    if (result.count == 12 || !track(json, result.tracks[result.count++])) return false;
                } while (json.take(','));
                if (!json.take(']')) return false;
            }
        } else if (std::strcmp(key, "has_more") == 0) { if (!json.boolean(result.hasMore)) return false; }
        else if (std::strcmp(key, "offset") == 0) { if (!json.number(result.offset)) return false; }
        else if (std::strcmp(key, "next_offset") == 0) { if (!json.number(result.nextOffset)) return false; }
        else if (!json.skip()) return false;
    } while (json.take(','));
    return tracksSeen && json.take('}') && json.finished() && result.offset <= 5000 && result.nextOffset <= 5012;
}

bool parseHealth(const char* data, std::size_t length) {
    Json json(data, length);
    if (!json.take('{')) return false;
    bool ok = false;
    do {
        char key[32];
        if (!json.string(key, sizeof(key)) || !json.take(':')) return false;
        if (std::strcmp(key, "status") == 0) {
            char value[16];
            if (!json.string(value, sizeof(value))) return false;
            ok = std::strcmp(value, "ok") == 0;
        } else if (!json.skip()) return false;
    } while (json.take(','));
    return ok && json.take('}') && json.finished();
}
