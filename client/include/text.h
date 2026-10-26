#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

inline uint32_t nextCodepoint(const char*& text) {
    unsigned char first = static_cast<unsigned char>(*text);
    if (!first) return 0;
    ++text;
    if (first < 128) return first;
    int extra = (first & 0xe0) == 0xc0 ? 1 : (first & 0xf0) == 0xe0 ? 2 : (first & 0xf8) == 0xf0 ? 3 : 0;
    if (!extra) return '?';
    uint32_t value = first & ((1u << (6 - extra)) - 1);
    const char* tail = text;
    for (int i = 0; i < extra; ++i) {
        unsigned char byte = static_cast<unsigned char>(tail[i]);
        if (!byte || (byte & 0xc0) != 0x80) return '?';
        value = (value << 6) | (byte & 63);
    }
    text += extra;
    if (value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) return '?';
    if ((extra == 1 && value < 128) || (extra == 2 && value < 2048) || (extra == 3 && value < 65536)) return '?';
    return value;
}

inline void eraseCodepoint(char* text) {
    std::size_t length = std::strlen(text);
    if (!length) return;
    --length;
    while (length && (static_cast<unsigned char>(text[length]) & 0xc0) == 0x80) --length;
    text[length] = 0;
}

inline unsigned codepointCount(const char* text) {
    unsigned count = 0;
    while (*text) { nextCodepoint(text); ++count; }
    return count;
}

inline bool appendText(char* target, std::size_t capacity, const char* value,unsigned maxCodepoints=48) {
    std::size_t used = std::strlen(target), extra = std::strlen(value);
    if (used + extra >= capacity || codepointCount(target) + codepointCount(value) > maxCodepoints) return false;
    std::memcpy(target + used, value, extra + 1);
    return true;
}

inline bool urlEncode(const char* source, char* target, std::size_t capacity) {
    static const char hex[] = "0123456789ABCDEF";
    std::size_t pos = 0;
    while (*source) {
        auto c = static_cast<unsigned char>(*source++);
        bool plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                     (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
        if (pos + (plain ? 1 : 3) >= capacity) return false;
        if (plain) target[pos++] = c;
        else { target[pos++] = '%'; target[pos++] = hex[c >> 4]; target[pos++] = hex[c & 15]; }
    }
    target[pos] = 0;
    return true;
}
