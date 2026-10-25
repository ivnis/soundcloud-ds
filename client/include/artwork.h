#pragma once
#include <cstddef>
#include <cstdint>

constexpr std::size_t kArtworkBytes = 4624;
constexpr uint16_t kDefaultAccent = 0x8000 | 31 | (13 << 5) | (5 << 10);
constexpr uint16_t kDefaultInk = 0x8000 | 1 | (2 << 5) | (3 << 10);

struct Artwork {
    uint16_t pixels[64 * 64]{};
    uint16_t accent = kDefaultAccent, ink = kDefaultInk;
    bool valid = false;
    void clear() { valid = false; accent = kDefaultAccent; ink = kDefaultInk; }
};
bool parseArtwork(const uint8_t* data, std::size_t size, Artwork& result);
