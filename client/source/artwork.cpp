#include "artwork.h"
#include <cstring>

namespace { unsigned word(const uint8_t* p) { return p[0] | (static_cast<unsigned>(p[1]) << 8); } }

bool parseArtwork(const uint8_t* data, std::size_t size, Artwork& result) {
    result.clear();
    if (size != kArtworkBytes || std::memcmp(data, "SCAR", 4) != 0 || word(data + 4) != 1 ||
        word(data + 6) != 64 || word(data + 8) != 64 || word(data + 14) != 256) return false;
    result.accent = word(data + 10) | 0x8000;
    result.ink = word(data + 12) | 0x8000;
    for (unsigned i = 0; i < 4096; ++i) {
        unsigned color = data[528 + i];
        result.pixels[i] = word(data + 16 + color * 2) | 0x8000;
    }
    result.valid = true;
    return true;
}
